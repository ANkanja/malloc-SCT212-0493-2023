/*
 * myalloc.c - a simple malloc/free built on sbrk() with a block list.
 */
#define _DEFAULT_SOURCE

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>                    /* snprintf only: no printf/puts */
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

    if (head == NULL) {
        head = b;
    } else if (tail->free && block_end(tail) == (char *)b) {
        /* new memory starts right after a free tail: extend it */
        tail->size += HDR + b->size;
        return tail;
    } else {
        tail->next = b;
    }
    tail = b;
    return b;
}

static fit_policy_t policy = FIT_FIRST;

/*
 * Find a free block with at least `need` bytes, or NULL.
 * First-fit stops at the first one; best-fit scans the whole list and
 * keeps the smallest one (stopping early on an exact match).
 */
static block_t *find_fit(size_t need)
{
    block_t *best = NULL;
    for (block_t *b = head; b != NULL; b = b->next) {
        if (!b->free || b->size < need)
            continue;
        if (policy == FIT_FIRST || b->size == need)
            return b;
        if (best == NULL || b->size < best->size)
            best = b;
    }
    return best;
}

#define MIN_PAYLOAD 16                /* smallest block worth splitting off */

/*
 * If b is bigger than needed, cut it in two: b keeps exactly `need`
 * bytes and the rest becomes a new free block right after it.
 * Only splits when the rest can hold a header plus MIN_PAYLOAD.
 */
static void split(block_t *b, size_t need)
{
    if (b->size < need + HDR + MIN_PAYLOAD)
        return;                       /* too small: give the whole block */

    /* byte arithmetic on char *, not block_t * */
    block_t *rest = (block_t *)((char *)payload(b) + need);
    rest->size  = b->size - need - HDR;
    rest->next  = b->next;
    rest->free  = 1;
    rest->magic = MAGIC;

    b->size = need;
    b->next = rest;
    if (tail == b)
        tail = rest;
}

/* write a fixed message to stderr without stdio */
static void report(const char *msg)
{
    ssize_t r = write(STDERR_FILENO, msg, strlen(msg));
    (void)r;
}

/*
 * Merge every run of free blocks that are physically next to each
 * other. List neighbours are only merged when the first one's payload
 * ends exactly where the second one's header starts: after a gap
 * caused by libc moving the break they are not adjacent.
 */
static void coalesce(void)
{
    block_t *b = head;
    while (b != NULL && b->next != NULL) {
        block_t *n = b->next;
        if (b->free && n->free && block_end(b) == (char *)n) {
            b->size += HDR + n->size;     /* absorb n */
            b->next  = n->next;
            if (tail == n)
                tail = b;
            /* stay on b: it may now touch the next free block too */
        } else {
            b = n;
        }
    }
}

/*
 * Find the block whose payload is ptr. Never reads memory outside the
 * blocks we own, so a garbage pointer cannot crash us. Returns NULL if
 * ptr is not the start of one of our payloads.
 */
static block_t *find_block(void *ptr)
{
    if (((uintptr_t)ptr & (ALIGNMENT - 1)) != 0)
        return NULL;
    for (block_t *b = head; b != NULL; b = b->next)
        if (payload(b) == ptr)
            return b->magic == MAGIC ? b : NULL;
    return NULL;
}

/* is ptr somewhere inside a free block (e.g. a block merged away)? */
static int inside_free_block(void *ptr)
{
    for (block_t *b = head; b != NULL; b = b->next)
        if (b->free && (char *)ptr > (char *)b && (char *)ptr < block_end(b))
            return 1;
    return 0;
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
    split(b, need);
    b->free = 0;
    return payload(b);
}

void my_free(void *ptr)
{
    if (ptr == NULL)
        return;

    block_t *b = find_block(ptr);
    if (b == NULL) {
        /* a block that was freed and then merged into its neighbour
         * no longer has its own header in the list */
        if (((uintptr_t)ptr & (ALIGNMENT - 1)) == 0 && inside_free_block(ptr))
            report("my_free: double free detected\n");
        else
            report("my_free: invalid pointer\n");
        return;
    }
    if (b->free) {
        report("my_free: double free detected\n");
        return;
    }

    b->free = 1;
    coalesce();
}

void *my_calloc(size_t n, size_t size)
{
    /* n * size would overflow: check without doing the multiply */
    if (n != 0 && size > SIZE_MAX / n)
        return NULL;

    size_t total = n * size;
    void *p = my_malloc(total);
    if (p != NULL)
        memset(p, 0, total);
    return p;
}

void *my_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)                  /* behaves like malloc */
        return my_malloc(size);
    if (size == 0) {                  /* behaves like free */
        my_free(ptr);
        return NULL;
    }

    block_t *b = find_block(ptr);
    if (b == NULL || b->free) {
        report("my_realloc: invalid pointer\n");
        return NULL;
    }
    if (size > SIZE_MAX / 2)
        return NULL;
    size_t need = align_up(size);

    /* 1. already big enough: keep it, give back the unused tail */
    if (need <= b->size) {
        split(b, need);
        coalesce();                   /* tail may touch a free block */
        return ptr;
    }

    /* 2. next block is free, adjacent and big enough: absorb it */
    block_t *n = b->next;
    if (n != NULL && n->free && block_end(b) == (char *)n &&
        b->size + HDR + n->size >= need) {
        b->size += HDR + n->size;
        b->next  = n->next;
        if (tail == n)
            tail = b;
        split(b, need);
        return ptr;
    }

    /* 3. otherwise move: new block, copy, free the old one */
    void *q = my_malloc(size);
    if (q == NULL)
        return NULL;                  /* old block is left untouched */
    memcpy(q, ptr, b->size < size ? b->size : size);
    my_free(ptr);
    return q;
}

/* fill in the statistics by walking the list once */
void heap_stats(heap_stats_t *out)
{
    if (out == NULL)
        return;
    memset(out, 0, sizeof *out);
    out->heap_bytes = heap_total;
    for (block_t *b = head; b != NULL; b = b->next) {
        if (b->free) {
            out->free_blocks++;
            out->free_bytes += b->size;
            if (b->size > out->largest_free)
                out->largest_free = b->size;
        } else {
            out->used_blocks++;
            out->used_bytes += b->size;
        }
    }
}

/* one line per block; snprintf into a local buffer, then write() */
void heap_dump(void)
{
    char line[128];
    int i = 0;
    for (block_t *b = head; b != NULL; b = b->next, i++) {
        int len = snprintf(line, sizeof line, "[%2d] %p size=%zu %s\n",
                           i, payload(b), b->size, b->free ? "FREE" : "USED");
        if (len > 0) {
            ssize_t r = write(STDOUT_FILENO, line, (size_t)len);
            (void)r;
        }
    }
}

void set_policy(fit_policy_t p)
{
    if (p == FIT_FIRST || p == FIT_BEST)
        policy = p;
}

/*
 * Return 1 if the heap is consistent: every header is valid, every
 * payload is aligned, blocks are in increasing address order without
 * overlapping, and no two adjacent free blocks were left unmerged.
 */
int heap_check(void)
{
    block_t *last = NULL;
    for (block_t *b = head; b != NULL; b = b->next) {
        if (b->magic != MAGIC || (b->free != 0 && b->free != 1))
            return 0;                             /* bad header */
        if (((uintptr_t)payload(b) & (ALIGNMENT - 1)) != 0 ||
            (b->size & (ALIGNMENT - 1)) != 0)
            return 0;                             /* misaligned */
        if (b->next != NULL) {
            if ((char *)b->next < block_end(b))
                return 0;                         /* out of order / overlap */
            if (b->free && b->next->free && block_end(b) == (char *)b->next)
                return 0;                         /* missed a merge */
        }
        last = b;
    }
    return last == tail;
}
