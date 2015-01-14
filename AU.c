#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "AU.h"
#include "AUConf.h"

#define ISSUE_ERROR(err) xerror(err, # err)

/////////////////////////
//// Alignment Utils ////
/////////////////////////

union AlignmentType {
  int i;
  long l;
  long long ll;
  void *vp;
  void (*funp)(void);
  float f;
  double d;
  long double ld;
  float *fp;
  double *dp;
  long double *ldp;
};

enum {
  ALIGNMENT_BOUNDARY = sizeof (union AlignmentType),
};

static inline size_t
AlignSize(size_t n, size_t boundary) {
  assert(boundary > 1);
  assert(n <= SIZE_MAX - boundary + 1);
  assert(n > 0);
  size_t result = ((n - 1) + boundary)/boundary * boundary;
  assert(result >= n);
  return result;
}

#define PTR_SIZE_ALIGN AlignSize(sizeof (void*), ALIGNMENT_BOUNDARY)

static inline size_t
maxsz(size_t a, size_t b) {
  return a < b ? b : a;
}

//////////////////////
//// BYTE Builder ////
//////////////////////

#ifndef NDEBUG

#define ASSERT_VALID_B1(b1) \
  do { \
    assert(b1); \
    assert((b1)->mem); \
    assert((b1)->used <= (b1)->cap); \
    assert((b1)->cap > 0); \
  } while (0)

#else

#define ASSERT_VALID_B1(b1)

#endif

int
AU_B1_Setup(AU_ByteBuilder *b1, size_t cap) {
  assert(cap > 0);

  b1->cap = cap;
  b1->used = 0;
  b1->mem = xmalloc(cap);
  if (!b1->mem) {
    ISSUE_ERROR(AU_ERR_XMALLOC);
    return AU_ERR_XMALLOC;
  }
  ASSERT_VALID_B1(b1);
  return 0;
}

int
AU_B1_Append(AU_ByteBuilder *b1, const void *mem, size_t size) {
  ASSERT_VALID_B1(b1);
  assert(mem);

  void *out_addr = AU_B1_AppendForSetup(b1, size);
  if (!out_addr) {
    return -1;
  }
  memcpy(out_addr, mem, size);
  return 0;
}

void *
AU_B1_AppendForSetup(AU_ByteBuilder *b1, size_t size) {
  ASSERT_VALID_B1(b1);

  if (b1->used > SIZE_MAX - size) {
    ISSUE_ERROR(AU_ERR_OVERFLOW);
    return 0;
  }
  if (b1->used + size > b1->cap) {
    if (b1->cap == SIZE_MAX || b1->cap > SIZE_MAX - size) {
      // This seems so absurd...
      ISSUE_ERROR(AU_ERR_OVERFLOW);
      return 0;
    }
    size_t new_cap = b1->cap > SIZE_MAX/2
                     ? SIZE_MAX
                     : maxsz(b1->cap*2, size);
    void *p = xrealloc(b1->mem, new_cap);
    if (!p) {
      ISSUE_ERROR(AU_ERR_XREALLOC);
      return 0;
    }
    b1->mem = p;
    b1->cap = new_cap;
  }
  void *out_addr = (char*)b1->mem + b1->used;
  b1->used += size;
  return out_addr;
}

void *
AU_B1_GetMemory(const AU_ByteBuilder *b1) {
  ASSERT_VALID_B1(b1);

  return b1->mem;
}

void
AU_B1_DiscardAppends(AU_ByteBuilder *b1) {
  ASSERT_VALID_B1(b1);

  b1->used = 0;
}

void
AU_B1_DiscardLastBytes(AU_ByteBuilder *b1, size_t n) {
  ASSERT_VALID_B1(b1);
  assert(b1->used >= n);

  b1->used -= n;
}

size_t
AU_B1_GetUsedCount(AU_ByteBuilder *b1) {
  return b1->used;
}

////////////////////////////
//// Fixed Size Builder ////
////////////////////////////

#ifndef NDEBUG

#define ASSERT_VALID_FSB(fsb) \
  do { \
    assert(fsb); \
    assert((fsb)->elt_size > 0); \
    assert((fsb)->elt_size <= SIZE_MAX / (fsb)->b1.cap); \
    ASSERT_VALID_B1(&(fsb)->b1); \
  } while (0)

#else

#define ASSERT_VALID_FSB(fsb)

#endif

int
AU_FSB_Setup(AU_FixedSizeBuilder *fsb, size_t elt_size, size_t cap) {
  assert(cap > 0);
  assert(elt_size > 0);
  assert(cap <= SIZE_MAX/elt_size);

  int b1_res = AU_B1_Setup(&fsb->b1, elt_size*cap);
  if (b1_res < 0) {
    return b1_res;
  }
  fsb->elt_size = elt_size;

  ASSERT_VALID_FSB(fsb);

  return 0;
}

int
AU_FSB_Append(AU_FixedSizeBuilder *fsb, const void *mem, size_t n) {
  ASSERT_VALID_FSB(fsb);
  assert(mem);

  if (n != 0 && fsb->elt_size > SIZE_MAX/n) {
    ISSUE_ERROR(AU_ERR_OVERFLOW);
    return AU_ERR_OVERFLOW;
  }

  int res = AU_B1_Append(&fsb->b1, mem, n*fsb->elt_size);
  if (res < 0) {
    return res;
  }
  return 0;
}

void *
AU_FSB_AppendForSetup(AU_FixedSizeBuilder *fsb, size_t n) {
  ASSERT_VALID_FSB(fsb);

  if (n != 0 && fsb->elt_size > SIZE_MAX/n) {
    ISSUE_ERROR(AU_ERR_OVERFLOW);
    return 0;
  }
  void *mem = AU_B1_AppendForSetup(&fsb->b1, n*fsb->elt_size);
  if (!mem) {
    return 0;
  }
  return mem;
}

void *
AU_FSB_GetMemory(AU_FixedSizeBuilder *fsb) {
  ASSERT_VALID_FSB(fsb);

  return AU_B1_GetMemory(&fsb->b1);
}

void
AU_FSB_DiscardAppends(AU_FixedSizeBuilder *fsb) {
  ASSERT_VALID_FSB(fsb);

  AU_B1_DiscardAppends(&fsb->b1);
}

void
AU_FSB_DiscardLastAppends(AU_FixedSizeBuilder *fsb, size_t n) {
  ASSERT_VALID_FSB(fsb);
  assert(n == 0 || fsb->elt_size <= SIZE_MAX/n);
  assert(fsb->b1.used/fsb->elt_size >= n);

  AU_B1_DiscardLastBytes(&fsb->b1, n*fsb->elt_size);
}

size_t
AU_FSB_GetUsedCount(AU_FixedSizeBuilder *fsb) {
  return fsb->b1.used/fsb->elt_size;
}

///////////////////////////////
//// Variable Size Builder ////
///////////////////////////////

int
AU_VSB_Setup(AU_VarSizeBuilder *vsb, size_t cap, size_t align) {
  vsb->align = align == AU_ALIGN_CONSERVATIVE ? ALIGNMENT_BOUNDARY : align;
  return AU_B1_Setup(&vsb->b1, AlignSize(cap, vsb->align));
}

int
AU_VSB_Append(AU_VarSizeBuilder *vsb, const void *mem, size_t n) {
  return AU_B1_Append(&vsb->b1, mem, AlignSize(n, vsb->align));
}

void *
AU_VSB_AppendForSetup(AU_VarSizeBuilder *vsb, size_t n) {
  return AU_B1_AppendForSetup(&vsb->b1, AlignSize(n, vsb->align));
}

void *
AU_VSB_GetMemory(AU_VarSizeBuilder *vsb) {
  return AU_B1_GetMemory(&vsb->b1);
}

void
AU_VSB_DiscardAppends(AU_VarSizeBuilder *vsb) {
  AU_B1_DiscardAppends(&vsb->b1);
}

size_t
AU_VSB_GetUsedCount(AU_VarSizeBuilder *vsb) {
  return AU_B1_GetUsedCount(&vsb->b1);
}

//////////////////////////////
//// Fixed Size Allocator ////
//////////////////////////////

enum {
  // How many pointers to initially allocate for a FSA.
  FSA_INITIAL_NUM_PTRS = 4
};

/*
 * The idea here is to allocate blocks of about N = sizeof (void*) + elt_size
 * bytes. When capacity limit is reached, total_cap + <some_delta> blocks of N
 * bytes are allocated, and the pointer to its first byte is appended to
 * fsb_base_ptrs so we know where they are in order to free them later.
 *
 * Having a block of N bytes, we can store a pointer and the bytes for the
 * element. If we establish that the first bytes will be for the void* and the
 * remaining bytes will be for the element, we can start doing something
 * interesting. This is because what we would have would be pretty much a
 * memory layout for a linked list. And then, we could just proceed on building
 * our own FSA as expected.
 *
 * We can't simply use a builder here for the *whole* allocator because it
 * could invalidate pointers on an allocation, which would be implemented as
 * some AppendForSetup call.
 */

inline static size_t
AU_FSA_NewCap(size_t cap) {
  // About 1/3 more than it was.
  size_t delta = cap/3 + 1;
  return cap > SIZE_MAX - delta ? SIZE_MAX : cap + delta;
}

static int
AU_FSA_Expand(AU_FixedSizeAllocator *fsa, size_t new_cap) {
  assert(new_cap > fsa->total_cap);

  size_t elt_alsize = AlignSize(fsa->elt_size, ALIGNMENT_BOUNDARY);
  size_t node_alsize = PTR_SIZE_ALIGN + elt_alsize;

  if (node_alsize > SIZE_MAX/new_cap) {
    ISSUE_ERROR(AU_ERR_OVERFLOW);
    return AU_ERR_OVERFLOW;
  }

  char *mem = xmalloc(node_alsize*new_cap);
  if (!mem) {
    ISSUE_ERROR(AU_ERR_XMALLOC);
    return AU_ERR_XMALLOC;
  }

  // Appending the pointer into fsb_base_ptrs so we remember it later when
  // we need to destroy this allocator.
  int res = AU_FSB_Append(&fsa->fsb_base_ptrs, &mem, 1);
  if (res < 0) {
    xfree(mem);
    return res;
  }

  // Set up the free list. Make each node point to the next. Last node will
  // point to the old head.
  void *old_head = fsa->free_head;
  fsa->free_head = mem;
  assert(new_cap > 0);
  for (size_t i = 0; i < new_cap-1; i++) {
    *(void**)mem = mem + node_alsize;
    mem += node_alsize;
  }
  *(void**)mem = old_head; // Set up last node.
  fsa->total_cap = new_cap;
  return 0;
}

int
AU_FSA_Setup(AU_FixedSizeAllocator *fsa, size_t elt_size, size_t cap) {
  assert(cap > 0);
  assert(elt_size > 0);
  assert(cap <= SIZE_MAX/elt_size);

  // Basic setup and allocation of an initial memory region.
  fsa->total_cap = 0;
  fsa->elt_size = elt_size;
  fsa->free_head = 0;

  int res = AU_FSB_Setup(&fsa->fsb_base_ptrs,
                         sizeof (void*),
                         FSA_INITIAL_NUM_PTRS);
  if (res < 0) {
    return res;
  }

  return AU_FSA_Expand(fsa, cap);
}

void *
AU_FSA_Alloc(AU_FixedSizeAllocator *fsa) {
  if (!fsa->free_head) {
    if (fsa->total_cap == SIZE_MAX) {
      // Seriously?
      ISSUE_ERROR(AU_ERR_OVERFLOW);
      return 0;
    }
    if (AU_FSA_Expand(fsa, AU_FSA_NewCap(fsa->total_cap)) < 0) {
      return 0;
    }
  }
  char *free_head = fsa->free_head;
  void *new_free_head = *(void**)free_head;
  void *out = free_head + PTR_SIZE_ALIGN;
  fsa->free_head = new_free_head;
  return out;
}

void
AU_FSA_Free(AU_FixedSizeAllocator *fsa, void *mem) {
  void *node = (char*)mem - PTR_SIZE_ALIGN;
  *(void**)node = fsa->free_head;
  fsa->free_head = node;
}

void
AU_FSA_Destroy(AU_FixedSizeAllocator *fsa) {
  void **regions = AU_FSB_GetMemory(&fsa->fsb_base_ptrs);
  size_t used = AU_FSB_GetUsedCount(&fsa->fsb_base_ptrs);
  for (size_t i = 0; i < used; i++) {
    xfree(regions[i]);
  }
  xfree(regions);
}
