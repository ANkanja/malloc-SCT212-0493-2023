/*
 * myalloc.h - public API for the sbrk-based allocator.
 *
 * NOTE: the official starter pack was not available to me, so this
 * header was written from the specification in the assignment brief
 * (Sections 3 and 3.1). If the official myalloc.h differs, it should
 * replace this file.
 */
#ifndef MYALLOC_H
#define MYALLOC_H

#include <stddef.h>

typedef enum {
    FIT_FIRST = 0,   /* first free block that is large enough (default) */
    FIT_BEST  = 1    /* smallest free block that is large enough */
} fit_policy_t;

typedef struct {
    size_t heap_bytes;    /* total bytes obtained from the OS with sbrk */
    size_t used_bytes;    /* payload bytes in blocks that are in use */
    size_t free_bytes;    /* payload bytes in free blocks */
    size_t used_blocks;   /* number of blocks in use */
    size_t free_blocks;   /* number of free blocks */
    size_t largest_free;  /* payload size of the largest free block */
} heap_stats_t;

void *my_malloc(size_t size);
void  my_free(void *ptr);
void *my_calloc(size_t n, size_t size);
void *my_realloc(void *ptr, size_t size);

void  heap_stats(heap_stats_t *out);
void  heap_dump(void);
void  set_policy(fit_policy_t p);
int   heap_check(void);

#endif /* MYALLOC_H */
