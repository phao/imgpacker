#ifndef X_PNG_H
#define X_PNG_H

#include <SDL/SDL.h>

enum {
  X_PNG_FAIL_LIBC = -1,
  X_PNG_FAIL = -2,
  X_PNG_OK = 0
};

int
xpng_save_surface(const char *filename, SDL_Surface *surf);

const char *
xpng_strerror(int code);

#endif
