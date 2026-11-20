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

#define CHUNK     (64 * 1024)         /* minimum bytes asked from sbrk */

static block_t *head = NULL;          /* first block (lowest address) */
static block_t *tail = NULL;          /* last block (highest address) */
static size_t   heap_total = 0;       /* bytes obtained from the OS */

/* first byte after a block's payload */
static inline char *block_end(block_t *b)
{
    return (char *)payload(b) + b->size;
}

/*
 * Ask the OS for more memory with sbrk() and append it to the list as
 * one free block with at least `need` payload bytes. Asks for at least
 * CHUNK bytes so sbrk is not called for every small allocation.
 * Returns the free block, or NULL if sbrk fails.
 */
static block_t *grow(size_t need)
{
    if (need > SIZE_MAX / 2)          /* far too big; avoid overflow */
        return NULL;

    size_t request = need + HDR;
    if (request < CHUNK)
        request = CHUNK;

    /* the break may not be 16-byte aligned (libc moves it too) */
    void *cur = sbrk(0);
    if (cur == (void *)-1)
        return NULL;
    size_t pad = (ALIGNMENT - ((uintptr_t)cur & (ALIGNMENT - 1))) & (ALIGNMENT - 1);

    void *mem = sbrk((intptr_t)(request + pad));
    if (mem == (void *)-1)
        return NULL;
    heap_total += request + pad;

    block_t *b = (block_t *)((char *)mem + pad);
    b->size  = request - HDR;
    b->next  = NULL;
    b->free  = 1;
    b->magic = MAGIC;

    if (head == NULL)
        head = b;
    else
        tail->next = b;
    tail = b;
    return b;
}

/* first-fit: the first free block that is big enough, or NULL */
static block_t *find_fit(size_t need)
{
    for (block_t *b = head; b != NULL; b = b->next)
        if (b->free && b->size >= need)
            return b;
    return NULL;
}

/* ---- public API ---- */

void *my_malloc(size_t size)
{
    if (size == 0 || size > SIZE_MAX / 2)
        return NULL;
    size_t need = align_up(size);

    block_t *b = find_fit(need);
    if (b == NULL) {
        b = grow(need);               /* no hole fits: get more memory */
        if (b == NULL)
            return NULL;
    }
    b->free = 0;
    return payload(b);
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
