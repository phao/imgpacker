#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include <SDL2/SDL.h>

#include "XFlow.h"
#include "RegionInfo.h"
#include "BinPack2D.h"
#include "AU.h"

/**
 * Leaf nodes have right == 0 and down == 0. Inner nodes have both not-null.
 * Both kinds of nodes have valid rect fields.
 */
struct TNode {
  SDL_Rect rect;
  struct TNode *right, *down;
};

enum {
  /**
   * These attempt result is only used internally.
   * A call to bin_pack_2d cannot ever return it.
   */
  ATTEMPT_UNFIT = INT_MIN
};

#define assert_leaf_node(n) assert(is_leaf_node(n))
#define assert_inner_node(n) assert(is_inner_node(n))

struct Context {
  AU_FixedSizeAllocator fsa;
  struct BinPack2DOptions opts;
};

static inline int
imax(int a, int b) {
  return a > b ? a : b;
}

static inline int
is_leaf_node(struct TNode *n) {
  return n && !n->right && !n->down;
}

static inline int
is_inner_node(struct TNode *n) {
  return n && n->right && n->down;
}

static inline int
maxside_named_surface_cmp(const void *a, const void *b) {
  assert(a);
  assert(b);

  const struct NamedSurface *ia = a;
  const struct NamedSurface *ib = b;
  int max_side_a = imax(ia->surf->w, ia->surf->h);
  int max_side_b = imax(ib->surf->w, ib->surf->h);
  return max_side_a < max_side_b ? 1 : (max_side_a == max_side_b ? 0 : -1);
}

static struct TNode *
leaf_node(int x, int y, int w, int h, AU_FixedSizeAllocator *fsa) {
  struct TNode *n = AU_FSA_Alloc(fsa);
  return_if(!n, 0);

  n->rect = (SDL_Rect) {x, y, w, h};
  n->right = 0;
  n->down = 0;

  assert_leaf_node(n);

  return n;
}

/**
 * Given a leaf node and an image, assuming the leaf node can support the
 * image. Put it in there. The new inner node which should replace the given
 * leaf node is returned. The given leaf node is freed on success.
 */
static int
split_leaf(struct TNode *n, int img_w, int img_h, AU_FixedSizeAllocator *fsa) {
  assert_leaf_node(n);
  assert(n->rect.w >= img_w);
  assert(n->rect.h >= img_h);

  n->right = leaf_node(n->rect.x + img_w, n->rect.y,
                       n->rect.w - img_w, img_h,
                       fsa);
  goto_if(!n->right, err);
  n->down = leaf_node(n->rect.x, n->rect.y + img_h,
                      n->rect.w, n->rect.h - img_h,
                      fsa);
  goto_if(!n->down, err);

  assert_inner_node(n);

  return 0;

 err:
  n->right = 0;
  n->down = 0;
  return -1;
}

static int
try_insert(struct TNode **head,
           struct RegionInfo *region,
           struct NamedSurface *img,
           struct Context *cx)
{
  if (is_leaf_node(*head)) {
    SDL_Rect *leaf_rect = &(**head).rect;
    int img_w = img->surf->w;
    int img_h = img->surf->h;

    if (leaf_rect->w >= img_w && leaf_rect->h >= img_h) {
      return_if(split_leaf(*head, img_w, img_h, &cx->fsa) < 0,
                ATTEMPT_NO_MEM);
      region->img = img;
      region->rect = (SDL_Rect) {leaf_rect->x, leaf_rect->y, img_w, img_h};
      return ATTEMPT_OK;
    }
    else {
      return ATTEMPT_UNFIT;
    }
  }
  else {
    assert_inner_node(*head);

    int attempt = try_insert(&(**head).right, region, img, cx);
    return_if(attempt == ATTEMPT_OK || attempt == ATTEMPT_NO_MEM,
              attempt);
    assert(attempt == ATTEMPT_UNFIT);
    attempt = try_insert(&(**head).down, region, img, cx);
    return_if(attempt == ATTEMPT_OK || attempt == ATTEMPT_NO_MEM,
              attempt);
    assert(attempt == ATTEMPT_UNFIT);
    return ATTEMPT_UNFIT;
  }
}

static int
grow_right_insert(struct TNode **head,
                  struct RegionInfo *region,
                  struct NamedSurface *img,
                  AU_FixedSizeAllocator *fsa)
{
  assert_inner_node(*head);

  SDL_Rect *head_rect = &(**head).rect;
  int head_x = head_rect->x;
  int head_y = head_rect->y;
  int head_w = head_rect->w;
  int head_h = head_rect->h;
  int img_w = img->surf->w;
  int img_h = img->surf->h;
  int new_w = img_w + head_w;

  struct TNode *new_head = AU_FSA_Alloc(fsa);
  return_if(!new_head, ATTEMPT_NO_MEM);
  struct TNode *right = leaf_node(head_x + head_w, head_y,
                                  img_w, head_h,
                                  fsa);
  if (!right) {
    return ATTEMPT_NO_MEM;
  }
  if (split_leaf(right, img_w, img_h, fsa) < 0) {
    return ATTEMPT_NO_MEM;
  }
  region->img = img;
  region->rect = (SDL_Rect) {head_x + head_w, head_y, img_w, img_h};
  new_head->right = right;
  new_head->down = *head;
  new_head->rect = (SDL_Rect) {head_x, head_y, new_w, head_h};
  *head = new_head;
  return ATTEMPT_OK;
}

static int
grow_down_insert(struct TNode **head,
                 struct RegionInfo *region,
                 struct NamedSurface *img,
                 AU_FixedSizeAllocator *fsa)
{
  assert_inner_node(*head);

  SDL_Rect *head_rect = &(**head).rect;
  int head_x = head_rect->x;
  int head_y = head_rect->y;
  int head_w = head_rect->w;
  int head_h = head_rect->h;
  int img_w = img->surf->w;
  int img_h = img->surf->h;
  int new_h = img_h + head_h;

  struct TNode *new_head = AU_FSA_Alloc(fsa);
  return_if(!new_head, ATTEMPT_NO_MEM);
  struct TNode *down = leaf_node(head_x, head_y + head_h,
                                 head_w, img_h,
                                 fsa);
  if (!down) {
    return ATTEMPT_NO_MEM;
  }
  if (split_leaf(down, img_w, img_h, fsa) < 0) {
    return ATTEMPT_NO_MEM;
  }
  region->img = img;
  region->rect = (SDL_Rect) {head_x, head_y + head_h, img_w, img_h};
  new_head->right = *head;
  new_head->down = down;
  new_head->rect = (SDL_Rect) {head_x, head_y, head_w, new_h};
  *head = new_head;
  return ATTEMPT_OK;
}

static int
grow_insert(struct TNode **head,
            struct RegionInfo *region,
            struct NamedSurface *img,
            struct Context *cx)
{
  assert(img);
  assert(region);
  assert(cx->opts.w > 0);
  assert(cx->opts.h > 0);
  assert(*head);

  int img_w = img->surf->w;
  int img_h = img->surf->h;
  int root_w = (*head)->rect.w;
  int root_h = (*head)->rect.h;

  int can_grow_down = root_w >= img_w;
  int can_grow_right = root_h >= img_h;

  assert(can_grow_down || can_grow_right);

  int should_grow_down = can_grow_down &&
    (cx->opts.w <= root_w + img_w || root_w > root_h) &&
    root_h + img_h <= cx->opts.h;
  int should_grow_right = can_grow_right &&
    (cx->opts.h <= root_h + img_h || root_h > root_w) &&
    root_w + img_w <= cx->opts.w;

  AU_FixedSizeAllocator *fsa = &cx->fsa;
  return_if(should_grow_right, grow_right_insert(head, region, img, fsa));
  return_if(should_grow_down, grow_down_insert(head, region, img, fsa));
  return_if(can_grow_down, grow_down_insert(head, region, img, fsa));
  assert(can_grow_right);
  return grow_right_insert(head, region, img, fsa);
}

static int
insert(struct TNode **head,
       struct RegionInfo *region,
       struct NamedSurface *img,
       struct Context *cx)
{
  int attempt = try_insert(head, region, img, cx);
  return_if(attempt == ATTEMPT_OK || attempt == ATTEMPT_NO_MEM, attempt);
  assert(attempt == ATTEMPT_UNFIT);
  return grow_insert(head, region, img, cx);
}

struct BinPack2DResult
bin_pack_2d(struct NamedSurface *imgs,
            int num_imgs,
            struct BinPack2DOptions opts)
{
  assert(num_imgs > 0);
  assert(imgs);
  assert(opts.w > 0);
  assert(opts.h > 0);

  struct BinPack2DResult result = {ATTEMPT_NO_MEM, 0, 0};
  struct TNode *head = 0;
  struct Context cx = {.opts = opts};

  goto_if(AU_FSA_Setup(&cx.fsa, sizeof (struct TNode), 1) < 0,
          err);

  qsort(imgs, num_imgs, sizeof (struct NamedSurface),
        maxside_named_surface_cmp);

  /*
   * Should the regions storage be a parameter?
   */
  result.regions = malloc(num_imgs * sizeof (struct RegionInfo));

  goto_if(!result.regions, err);
  head = leaf_node(0, 0, imgs[0].surf->w, imgs[0].surf->h, &cx.fsa);
  goto_if(!head, err);

  for (int i = 0; i < num_imgs; i++) {
    result.attempt = insert(&head, result.regions+i, imgs+i, &cx);
    goto_if(result.attempt < 0, err);
  }

  assert(head);

  Uint32 rmask, gmask, bmask, amask;

  /* This following code was copied/pasted from the SDL wiki docs. */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  rmask = 0xff000000;
  gmask = 0x00ff0000;
  bmask = 0x0000ff00;
  amask = 0x000000ff;
#else
  rmask = 0x000000ff;
  gmask = 0x0000ff00;
  bmask = 0x00ff0000;
  amask = 0xff000000;
#endif

  result.attempt = ATTEMPT_NO_SURFACE;
  result.img = SDL_CreateRGBSurface(0, head->rect.w, head->rect.h, 32,
                                    rmask, gmask, bmask, amask);
  goto_if(!result.img, err);

  for (int i = 0; i < num_imgs; i++) {
    struct RegionInfo *reg = result.regions + i;
    int blit = SDL_BlitSurface(reg->img->surf, 0, result.img, &reg->rect);
    goto_if(blit < 0, err);
  }

  result.attempt = ATTEMPT_OK;
  AU_FSA_Destroy(&cx.fsa);
  return result;

err:
  assert(result.attempt < 0);
  if (head) {
    AU_FSA_Destroy(&cx.fsa);
  }
  if (result.img) {
    SDL_FreeSurface(result.img);
  }
  free(result.regions);
  return result;
}

const char *
bp2d_strerror(int attempt) {
  switch (attempt) {
    case ATTEMPT_NO_MEM:
      return strerror(errno);
    case ATTEMPT_NO_SURFACE:
      return SDL_GetError();
  }
  return 0;
}
