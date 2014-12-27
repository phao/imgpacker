#ifndef REGION_INFO_H
#define REGION_INFO_H

#include <SDL2/SDL.h>

struct NamedSurface {
  SDL_Surface *surf;
  char *name;
};

struct RegionInfo {
  SDL_Rect rect;
  struct NamedSurface *img;
};

#endif
