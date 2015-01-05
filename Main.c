#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include "XFlow.h"
#include "Config.h"
#include "RegionInfo.h"
#include "BinPack2D.h"
#include "xPNG.h"

enum {
  PINT_EMPTY_INPUT = -1,
  PINT_INVALID_INPUT = -2,
  PINT_OVERFLOW_INPUT = -3,
  PINT_SUCCESS = 0
};

static struct Config cfg = CONFIG_DEFAULT_INIT_CODE;
static int num_imgs, loaded;
static struct NamedSurface *imgs;
static char **files;
static struct BinPack2DResult bp2d;

/**
 * Parse positive int.
 */
static int
parse_pint(const char *text, int *out) {
  return_if(!*text, PINT_EMPTY_INPUT);
  char *e;
  long lout = strtol(text, &e, 10);
  return_if(*e, PINT_INVALID_INPUT);
  return_if(lout <= 0 || lout > INT_MAX, PINT_OVERFLOW_INPUT);
  *out = (int) lout;
  return PINT_SUCCESS;
}

static void
cleanup(void) {
  for (int i = 0; i < loaded; i++) {
    SDL_FreeSurface(imgs[i].surf);
    // The cast is to indicate to the compiler that we really mean to
    // discard the const qualifier.
    free((void*) imgs[i].name);
  }
  free(imgs);
  free(bp2d.regions);
  if(bp2d.img) {
    SDL_FreeSurface(bp2d.img);
  }
  loaded = 0;
  imgs = 0;
  bp2d.regions = 0;
  bp2d.img = 0;
  IMG_Quit();
  SDL_Quit();
}

static void
print_usage(void) {
  fputs("Usage:\n"
        "imgpacker [-l] [-w WITDH] [-h HEIGHT] [-o PNG_OUT_FILE]\n"
        "\t[-c CSV_OUT_FILE] [-r REPLACEMENT_CHAR] <input file>+\n"
        "\n"
        "* In case no PNG output file is specified, 'out.png' will be used.\n"
        "* In case no CSV output file is specified, 'out.csv' will be used.\n",
        stderr);
}

static void
print_err(const char *fmt, va_list params) {
  fputs("Error: ", stderr);
  vfprintf(stderr, fmt, params);
  putc('\n', stderr);
}

static void
err_exit(const char *fmt, ...) {
  va_list params;
  va_start(params, fmt);
  print_err(fmt, params);
  va_end(params);
  cleanup();
  exit(EXIT_FAILURE);
}

static void
uerr_exit(const char *fmt, ...) {
  va_list params;
  va_start(params, fmt);
  print_err(fmt, params);
  va_end(params);
  print_usage();
  cleanup();
  exit(EXIT_FAILURE);
}

static void
vlog(const char *fmt, ...) {
  if (CONFIG_IS_VERBOSE(cfg)) {
    va_list params;
    va_start(params, fmt);
    vfprintf(stderr, fmt, params);
    va_end(params);
  }
}

static char *
dup_adjust_name(const char *name) {
  size_t ulen = strlen(name);
  if (ulen >= INT_MAX) {
    err_exit("String too big (really?): len=%zu.", ulen);
  }
  int len = (int) ulen;
  assert(len+1 <= INT_MAX);
  errno = 0;
  char *str = malloc((size_t) (len+1));
  if (!str) {
    err_exit("libc: %s.", strerror(errno));
  }
  strcpy(str, name);
  for (int i = len-1; i >= 0; i--) {
    if (str[i] == '.') {
      str[i] = 0;
      len = i;
      break;
    }
  }
  for (int i = 0; i < len; i++) {
    if (!isalnum(str[i]) && str[i] != '_') {
      str[i] = '_';
    }
  }
  return str;
}

static void
build_cfg(int argc, char **argv) {
  char **argv_begin = argv;
  for (char *opt = *++argv;
       opt && *opt == '-' && !opt[2];
       opt = *++argv)
  {
    switch (opt[1]) {
      case 'w':
        argv++;
        if (parse_pint(*argv, &cfg.w) < 0) {
          uerr_exit("Invalid width value: '%s'.", *argv);
        }
        break;
      case 'h':
        argv++;
        if (parse_pint(*argv, &cfg.h) < 0) {
          uerr_exit("Invalid height value: '%s'.", *argv);
        }
        break;
      case 'r':
        argv++;
        cfg.repl = **argv;
        if (!cfg.repl) {
          uerr_exit("Empty string for -r replacement.");
        }
        break;
      case 'o':
        argv++;
        cfg.png_out = *argv;
        if (!*cfg.png_out) {
          uerr_exit("Empty string for PNG output.");
        }
        break;
      case 'c':
        argv++;
        cfg.csv_out = *argv;
        if (!*cfg.csv_out) {
          uerr_exit("Empty string for CSV output.");
        }
        break;
      case 'v':
        cfg.flags |= CONFIG_VERBOSE_FLAG;
        break;
      default:
        uerr_exit("Invalid option: %s.", opt);
        break;
    }
  }

  files = argv;
  num_imgs = (int) (argc - (argv - argv_begin));
  if (num_imgs == 0) {
    uerr_exit("No input files.");
  }
  else if (num_imgs < 0) {
    err_exit("Number of images is invalid: %d.\n", num_imgs);
  }

  /*
   * What if num_imgs is too large? Overflow check here is silly, but the value
   * for num_imgs is out of my control, so it seems like it should be checked.
   *
   * Basically, num_imgs needs to be positive and within SIZE_MAX.
   */

  assert(files);
  assert(num_imgs > 0);
}

static void
load_imgs(void) {
  assert(num_imgs > 0);
  assert(files);

  // What if ((size_t) num_imgs * sizeof (struct NamedSurface)) overflows?

  errno = 0;
  imgs = malloc((size_t) num_imgs * sizeof (struct NamedSurface));
  if (!imgs) {
    err_exit("libc: %s.", strerror(errno));
  }

  for (loaded = 0; loaded < num_imgs; loaded++) {
    imgs[loaded].surf = IMG_Load(files[loaded]);
    if (!imgs[loaded].surf) {
      err_exit("Loading file: %s: SDL2_image: %s.", files[loaded],
         IMG_GetError());
    }
    imgs[loaded].name = dup_adjust_name(files[loaded]);
    vlog("Loaded %s.\n", files[loaded]);
  }
}

static void
init(void) {
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    err_exit("SDL2: %s.", SDL_GetError());
  }
  int img_flags = IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF;
  if ((IMG_Init(img_flags) & img_flags) != img_flags) {
    err_exit("SDL2_image: %s.", IMG_GetError());
  }
}

static void
regions_csv_output(void) {
  errno = 0;
  FILE *csvf = fopen(cfg.csv_out, "w");
  if (!csvf) {
    err_exit("libc: %s.", strerror(errno));
  }
  // check for errors, use ferror
  for (int i = 0; i < num_imgs; i++) {
    struct RegionInfo *reg = bp2d.regions+i;
    fprintf(csvf, "%s,%d,%d,%d,%d\n", reg->img->name, reg->rect.x,
      reg->rect.y, reg->rect.w, reg->rect.h);
  }
  fclose(csvf);
}

static void
output(void) {
  int res = xpng_save_surface(cfg.png_out, bp2d.img);
  if (res < 0) {
    err_exit("xPNG: %s.", xpng_strerror(res));
  }
  regions_csv_output();
}

static void
imgpack(void) {
  vlog("Packing images.\n");
  bp2d = bin_pack_2d(imgs, num_imgs, (struct BinPack2DOptions)
    {cfg.w, cfg.h});
  if (bp2d.attempt < 0) {
    err_exit("BinPack2D: %s.", bp2d_strerror(bp2d.attempt));
  }
  vlog("Done.\n");
}

int
main(int argc, char *argv[]) {
  init();
  build_cfg(argc, argv);
  load_imgs();
  imgpack();
  output();
  cleanup();
  return 0;
}
