/*
 * bench.c - compare first-fit, best-fit and the C library allocator.
 *
 * Usage: ./bench first | best | libc
 *
 * Runs the same random sequence of 50,000 malloc/free operations
 * (sizes 1..4096 bytes, fixed seed) and prints the heap statistics
 * and the time taken. Each policy runs in its own process so every
 * run starts from a fresh heap.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "myalloc.h"

#define OPS   50000
#define SLOTS 1000
#define SEED  2026

static int    op_slot[OPS];
static size_t op_size[OPS];

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s first|best|libc\n", argv[0]);
        return 2;
    }
    int use_libc = strcmp(argv[1], "libc") == 0;
    if (!use_libc) {
        if (strcmp(argv[1], "best") == 0)
            set_policy(FIT_BEST);
        else if (strcmp(argv[1], "first") == 0)
            set_policy(FIT_FIRST);
        else {
            fprintf(stderr, "unknown policy '%s'\n", argv[1]);
            return 2;
        }
    }

    /* build the operation sequence first so rand() is not timed */
    srand(SEED);
    for (int i = 0; i < OPS; i++) {
        op_slot[i] = rand() % SLOTS;
        op_size[i] = (size_t)(rand() % 4096) + 1;
    }

    static char *slot[SLOTS];
    double t0 = now_ms();
    for (int i = 0; i < OPS; i++) {
        int s = op_slot[i];
        if (slot[s] == NULL) {                    /* empty slot: allocate */
            slot[s] = use_libc ? malloc(op_size[i]) : my_malloc(op_size[i]);
            if (slot[s] == NULL) {
                fprintf(stderr, "allocation failed at op %d\n", i);
                return 1;
            }
            slot[s][0] = 1;                       /* touch the memory */
        } else {                                  /* full slot: free */
            if (use_libc) free(slot[s]); else my_free(slot[s]);
            slot[s] = NULL;
        }
    }
    double ms = now_ms() - t0;

    int live = 0;
    for (int s = 0; s < SLOTS; s++) if (slot[s]) live++;

    if (use_libc) {
        printf("policy=libc  time_ms=%.2f live_blocks=%d\n", ms, live);
        return 0;
    }

    heap_stats_t st;
    heap_stats(&st);
    double frag = st.free_bytes ? 1.0 - (double)st.largest_free / st.free_bytes : 0.0;
    printf("policy=%-5s time_ms=%.2f live_blocks=%d heap_bytes=%zu "
           "used_bytes=%zu free_bytes=%zu free_blocks=%zu largest_free=%zu "
           "ext_frag=%.3f heap_ok=%d\n",
           argv[1], ms, live, st.heap_bytes, st.used_bytes, st.free_bytes,
           st.free_blocks, st.largest_free, frag, heap_check());
    return 0;
}
