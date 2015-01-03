#ifndef BinPack2D_H
#define BinPack2D_H

#include "RegionInfo.h"

enum {
  ATTEMPT_OK = 0,
  ATTEMPT_NO_MEM = -1,
  ATTEMPT_NO_SURFACE = -2
};

struct BinPack2DResult {
  int attempt;
  SDL_Surface *img;
  struct RegionInfo *regions;
};

struct BinPack2DOptions {
  int w, h;
};

struct BinPack2DResult
bin_pack_2d(struct NamedSurface *imgs,
            const int num_imgs,
            struct BinPack2DOptions opts);

const char *
bp2d_strerror(const int attempt);

#endif
