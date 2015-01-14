#ifndef X_MALLOC_H
#define X_MALLOC_H

#include <stdio.h>
#include <stdlib.h>

#define xmalloc malloc
#define xfree free
#define xrealloc realloc

#define xerror(err_code, err_name) \
  fprintf(stderr, "AU_Error: %d: %s\n", (err_code), (err_name))

#endif
