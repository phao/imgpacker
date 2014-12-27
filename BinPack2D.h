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

struct BinPack2DResult
bin_pack_2d(struct NamedSurface *imgs,
            int num_imgs,
            int limit_w,
            int limit_h);

const char *
bp2d_strerror(int attempt);

#endif
