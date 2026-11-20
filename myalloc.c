/*
 * myalloc.c - a simple malloc/free built on sbrk() with a block list.
 */
#define _DEFAULT_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "myalloc.h"

#define ALIGNMENT 16
#define MAGIC     0x4D594131          /* "MYA1": marks a valid header */

/*
 * Every block starts with this header. The user's memory (payload)
 * starts HDR bytes after the header.
 */
typedef struct block {
    size_t        size;   /* payload size in bytes (multiple of 16) */
    struct block *next;   /* next block, in increasing address order */
    int           free;   /* 1 = free, 0 = in use */
    int           magic;  /* MAGIC if this is a real header */
} block_t;

/* header size rounded up to 16 so payloads stay 16-byte aligned */
#define HDR ((sizeof(block_t) + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1))

/* round n up to a multiple of 16: add 15, then clear the low 4 bits */
static inline size_t align_up(size_t n)
{
    return (n + (ALIGNMENT - 1)) & ~(size_t)(ALIGNMENT - 1);
}

/* header address -> user pointer */
static inline void *payload(block_t *b)
{
    return (char *)b + HDR;
}

/* user pointer -> header address */
static inline block_t *header(void *ptr)
{
    return (block_t *)((char *)ptr - HDR);
}

/* ---- public API (filled in by later tasks) ---- */

void *my_malloc(size_t size)
{
    (void)size;
    return NULL;
}

void my_free(void *ptr)
{
    (void)ptr;
}

void *my_calloc(size_t n, size_t size)
{
    (void)n;
    (void)size;
    return NULL;
}

void *my_realloc(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
    return NULL;
}

void heap_stats(heap_stats_t *out)
{
    memset(out, 0, sizeof *out);
}

void heap_dump(void)
{
}

void set_policy(fit_policy_t p)
{
    (void)p;
}

int heap_check(void)
{
    return 1;
}
