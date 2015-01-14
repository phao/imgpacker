#ifndef ALLOC_UTILS_H
#define ALLOC_UTILS_H

/**
 * With the allocator and builder types, you're supposed to use them through
 * the provided typedefs. The underlying representation of those types are
 * reserved for the library authors to decide at their own will. It can change
 * from non-debug to debug verion, from this version to the next, and so forth.
 *
 * They are only defined here inside the header file so you can have them on
 * automatic storage or static storage. You can still put them on allocated
 * storage, but you're not required. That is, the types are only defined here
 * so you can better choose the storage of your variables.
 */

#include <stdlib.h>
#include <limits.h>

enum {
  AU_ERR_XMALLOC = INT_MIN,
  AU_ERR_XREALLOC,
  AU_ERR_XCALLOC,
  AU_ERR_OVERFLOW
};

enum {
  AU_ALIGN_CONSERVATIVE = 0
};

//////////////////////
//// BYTE Builder ////
//////////////////////

struct AU_ByteBuilder {
  void *mem;
  size_t used, cap;
};

typedef struct AU_ByteBuilder AU_ByteBuilder;

int
AU_B1_Setup(AU_ByteBuilder *b1, size_t cap);

int
AU_B1_Append(AU_ByteBuilder *b1, const void *mem, size_t size);

void*
AU_B1_AppendForSetup(AU_ByteBuilder *b1, size_t size);

void*
AU_B1_GetMemory(const AU_ByteBuilder *b1);

void
AU_B1_DiscardAppends(AU_ByteBuilder *b1);

void
AU_B1_DiscardLastBytes(AU_ByteBuilder *b1, size_t n);

size_t
AU_B1_GetUsedCount(AU_ByteBuilder *b1);

////////////////////////////
//// Fixed Size Builder ////
////////////////////////////

struct AU_FixedSizeBuilder {
  AU_ByteBuilder b1;
  size_t elt_size;
};

/**
 * As with all other other builder/allocator types, the representation of an
 * AU_FixedSizeBuilder shouldn't be relied upon (check the other comment in
 * the beginning of this file).
 *
 * Functions on fixed size buffers have sizes in temrs of the element size,
 * whose case is the only exception to this rule. The element size you specify
 * in bytes during the setup function (AU_FSB_Setup). All the other sizes and
 * offsets are specified in terms of element sizes.
 */
typedef struct AU_FixedSizeBuilder AU_FixedSizeBuilder;

int
AU_FSB_Setup(AU_FixedSizeBuilder *fsb, size_t elt_size, size_t cap);

int
AU_FSB_Append(AU_FixedSizeBuilder *fsb, const void *mem, size_t n);

void *
AU_FSB_AppendForSetup(AU_FixedSizeBuilder *fsb, size_t n);

void*
AU_FSB_GetMemory(AU_FixedSizeBuilder *fsb);

void
AU_FSB_DiscardAppends(AU_FixedSizeBuilder *fsb);

void
AU_FSB_DiscardLastAppends(AU_FixedSizeBuilder *fsb, size_t n);

size_t
AU_FSB_GetUsedCount(AU_FixedSizeBuilder *fsa);

///////////////////////////////
//// Variable Size Builder ////
///////////////////////////////

struct AU_VarSizeBuilder {
  AU_ByteBuilder b1;
  size_t align;
};

/**
 * As with all other other builder/allocator types, the representation of an
 * AU_VarSizeBuilder shouldn't be relied upon (check the other comment in
 * the beginning of this file).
 *
 * Functions on variable size buffers have sizes and offsets specified in terms
 * of bytes, just like a byte builder.
 */
typedef struct AU_VarSizeBuilder AU_VarSizeBuilder;

/**
 * @param align You're allows to pass AU_ALIGN_CONSERVATIVE in the third
 * parameter to indicate you'd like a conservative alignment boundary the
 * library can come up with. Otherwise, the given boundary will be used.
 */
int
AU_VSB_Setup(AU_VarSizeBuilder *vsb, size_t cap, size_t align);

int
AU_VSB_Append(AU_VarSizeBuilder *vsb, const void *mem, size_t n);

void *
AU_VSB_AppendForSetup(AU_VarSizeBuilder *vsb, size_t n);

void *
AU_VSB_GetMemory(AU_VarSizeBuilder *vsb);

void
AU_VSB_DiscardAppends(AU_VarSizeBuilder *vsb);

size_t
AU_VSB_GetUsedCount(AU_VarSizeBuilder *vsa);

//////////////////////////////
//// Fixed Size Allocator ////
//////////////////////////////

struct AU_FixedSizeAllocator {
  // This builder keeps track of the base pointers for the regions this
  // allocators allocated.
  AU_FixedSizeBuilder fsb_base_ptrs;

  // The head of the free list for this allocator.
  void *free_head;

  // The element size.
  size_t elt_size;

  // Total capacity. Used to know how much to allocate on the next round as
  // soon as free_head becomes null.
  size_t total_cap;
};

/**
 * As with all other other builder/allocator types, the representation of an
 * AU_VarSizeBuilder shouldn't be relied upon (check the other comment in
 * the beginning of this file).
 *
 * Functions on variable size buffers have sizes and offsets specified in terms
 * of bytes, just like a byte builder.
 */
typedef struct AU_FixedSizeAllocator AU_FixedSizeAllocator;

int
AU_FSA_Setup(AU_FixedSizeAllocator *fsa, size_t elt_size, size_t cap);

void*
AU_FSA_Alloc(AU_FixedSizeAllocator *fsa);

void
AU_FSA_Free(AU_FixedSizeAllocator *fsa, void *mem);

void
AU_FSA_Destroy(AU_FixedSizeAllocator *fsa);

#endif
