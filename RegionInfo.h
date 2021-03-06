#ifndef REGION_INFO_H
#define REGION_INFO_H

#include <SDL2/SDL.h>

struct NamedSurface {
  SDL_Surface *surf;
  const char *name;
  int index;
};

struct RegionInfo {
  SDL_Rect rect;
  struct NamedSurface *img;
};

#endif

