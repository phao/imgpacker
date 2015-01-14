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
#include "AU.h"

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
  *out = lout;
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
  if (cfg.img_list_in) {
    /*
     * The memory allocated by the AU_ByteBuilder in the call to
     * read_files_list.
     */
    free(*files);
  }
  IMG_Quit();
  SDL_Quit();
}

static void
print_usage(void) {
  fputs("Usage:\n"
        "imgpacker [-l] [-w WITDH] [-h HEIGHT] [-o PNG_OUT_FILE]\n"
        "          [-c CSV_OUT_FILE] [-r REPLACEMENT_CHAR] \n"
        "          (-f IMAGE_LIST_FILE | <input file>+)\n"
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
  int len = ulen;
  assert(len+1 <= INT_MAX);
  char *str = malloc(len+1);
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

static char **
read_files_list(const char *img_list_file, int *out_num_files) {
  /*
   * The idea here is to keep two builders around, one for the the chars of
   * the strings, and another one for the length of the strings. Both builders
   * keep their data according to the order of the strings being read.
   *
   * After reading everything into the buffer, I can build a char** and return
   * it.
   */

  enum {
    INITIAL_BUF_CAP = 8000,
    EXPECTED_LINES = 100
  };

  FILE *file = fopen(img_list_file, "r");
  if (!file) {
    err_exit("Couldn't open input file: %s\n"
      "libc: %s", img_list_file, strerror(errno));
  }

  AU_ByteBuilder chars_b1;
  AU_FixedSizeBuilder lens_fsb;
  char buf[INITIAL_BUF_CAP];
  size_t len = 0;

  AU_B1_Setup(&chars_b1, INITIAL_BUF_CAP);
  AU_FSB_Setup(&lens_fsb, sizeof (size_t), EXPECTED_LINES);

  /*
   * buf[INITIAL_BUF_CAP-2] is initially set to 0, and after each iteration,
   * it's reset to 0.
   */
  buf[INITIAL_BUF_CAP-2] = 0;
  while (fgets(buf, INITIAL_BUF_CAP, file)) {
    char candidate_last = buf[INITIAL_BUF_CAP-2];

    if (candidate_last == 0 || candidate_last == '\n') {
      // Found end of line.

      size_t final_len = strlen(buf);
      if (buf[final_len-1] == '\n') {
        buf[final_len-1] = 0;
        final_len--;
      }
      AU_B1_Append(&chars_b1, buf, final_len+1);
      len += final_len;
      AU_FSB_Append(&lens_fsb, &len, 1);
      len = 0;
    }
    else {
      // Still in the middle of the line.

      AU_B1_Append(&chars_b1, buf, INITIAL_BUF_CAP-1);
      len += INITIAL_BUF_CAP-1;
    }

    // The reset to 0.
    buf[INITIAL_BUF_CAP-2] = 0;
  }

  if (ferror(file)) {
    err_exit("Error while reading the file: %s.\n"
      "libc: %s.", img_list_file, strerror(errno));
  }

  assert(feof(file));

  const size_t num_lines = AU_FSB_GetUsedCount(&lens_fsb);
  char **file_names_list = malloc(sizeof (char*) * num_lines);
  if (!file_names_list) {
    err_exit("Allocation error.\n"
      "libc: %s", strerror(errno));
  }

  char *chars_mem = AU_B1_GetMemory(&chars_b1);
  long *line_lens = AU_FSB_GetMemory(&lens_fsb);
  for (size_t i = 0; i < num_lines; i++) {
    file_names_list[i] = chars_mem;
    chars_mem += *line_lens + 1;
    line_lens++;
  }
  *out_num_files = num_lines;

  assert(file_names_list);
  assert(file);
  assert(AU_FSB_GetMemory(&lens_fsb));

  free(AU_FSB_GetMemory(&lens_fsb));
  fclose(file);
  return file_names_list;
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
      case 'f':
        argv++;
        cfg.img_list_in = *argv;
        break;
      default:
        uerr_exit("Invalid option: %s.", opt);
        break;
    }
  }

  if (cfg.img_list_in) {
    files = read_files_list(cfg.img_list_in, &num_imgs);
  }
  else {
    files = argv;
    num_imgs = argc - (argv - argv_begin);
  }

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

  imgs = malloc(num_imgs * sizeof (struct NamedSurface));
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
    imgs[loaded].index = loaded;
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

static inline int
cmp_region_info_by_named_surface_index(const void *a, const void *b) {
  const struct RegionInfo *i1 = (const struct RegionInfo*)a;
  const struct RegionInfo *i2 = (const struct RegionInfo*)b;
  if (i1->img->index < i2->img->index) {
    return -1;
  }
  else if (i1->img->index > i2->img->index) {
    return 1;
  }
  else {
    return 0;
  }
}

static void
regions_csv_output(void) {
  FILE *csvf = fopen(cfg.csv_out, "w");
  if (!csvf) {
    err_exit("libc: %s.", strerror(errno));
  }
  // We're going to sort the regions info so we can get them in an order
  // which reflects the input order.

  qsort(bp2d.regions, num_imgs, sizeof (struct RegionInfo),
        cmp_region_info_by_named_surface_index);

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
