#ifndef CONFIG_H
#define CONFIG_H

#include <limits.h>

struct Config {
  int w, h;
  unsigned flags;
  const char *png_out;
  const char *csv_out;
  const char *img_list_in;
  char repl;
};

static const char CONFIG_DEFAULT_PNG_OUT[] = "out.png";
static const char CONFIG_DEFAULT_CSV_OUT[] = "out.csv";

#define CONFIG_DEFAULT_IMG_LIST_IN ((char*)0)

enum {
  CONFIG_VERBOSE_FLAG = 1 << 0,
};

enum {
  CONFIG_DEFAULT_REPL = '_',
  CONFIG_DEFAULT_WIDTH = INT_MAX,
  CONFIG_DEFAULT_HEIGHT = INT_MAX,
  CONFIG_DEFAULT_FLAGS = 0,
  CONFIG_DEFAULT_PNG_OUT_LENGTH = sizeof CONFIG_DEFAULT_PNG_OUT - 1,
  CONFIG_DEFAULT_CSV_OUT_LENGTH = sizeof CONFIG_DEFAULT_CSV_OUT - 1
};

#define CONFIG_DEFAULT_INIT_CODE { CONFIG_DEFAULT_WIDTH, \
  CONFIG_DEFAULT_HEIGHT, CONFIG_DEFAULT_FLAGS, CONFIG_DEFAULT_PNG_OUT, \
  CONFIG_DEFAULT_CSV_OUT, CONFIG_DEFAULT_IMG_LIST_IN, CONFIG_DEFAULT_REPL}

#define CONFIG_IS_VERBOSE(cfg) (((cfg).flags & CONFIG_VERBOSE_FLAG) != 0)

#endif
