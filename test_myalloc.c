/*
 * test_myalloc.c - test harness for myalloc.c
 *
 * NOTE: the official starter pack was not available to me, so this
 * harness was written from Section 7.1 of the assignment brief:
 * 8 test groups (T1-T8) with 29 checks in total. If the official
 * test_myalloc.c becomes available it should replace this file.
 */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "myalloc.h"

static int passed = 0, failed = 0;

static void check(const char *id, int ok, const char *what)
{
    printf("%s %-5s %s\n", ok ? "PASS" : "FAIL", id, what);
    if (ok) passed++; else failed++;
}

static int aligned16(const void *p)
{
    return ((uintptr_t)p & 15u) == 0;
}

static heap_stats_t stats(void)
{
    heap_stats_t s;
    heap_stats(&s);
    return s;
}

/* T1: basic allocation */
static void t1(void)
{
    printf("-- T1 basic allocation\n");
    char *p = my_malloc(100);
    check("T1.1", p != NULL, "my_malloc(100) returns non-NULL");
    check("T1.2", aligned16(p), "result is 16-byte aligned");
    if (p) memset(p, 'x', 100);
    check("T1.3", my_malloc(0) == NULL, "my_malloc(0) returns NULL");
    my_free(NULL);
    my_free(p);
    check("T1.4", heap_check(), "free(NULL) is ignored and heap is consistent");
}

/* T2: no overlap between live blocks */
static void t2(void)
{
    printf("-- T2 no overlap\n");
    enum { N = 20 };
    unsigned char *p[N];
    size_t sz[N];
    int all_ok = 1, data_ok = 1, no_overlap = 1;

    for (int i = 0; i < N; i++) {
        sz[i] = (size_t)(i * 37 + 1);
        p[i] = my_malloc(sz[i]);
        if (p[i] == NULL || !aligned16(p[i])) all_ok = 0;
        else memset(p[i], i + 1, sz[i]);
    }
    check("T2.1", all_ok, "20 blocks of different sizes are non-NULL and aligned");

    for (int i = 0; i < N && all_ok; i++)
        for (size_t k = 0; k < sz[i]; k++)
            if (p[i][k] != (unsigned char)(i + 1)) data_ok = 0;
    check("T2.2", all_ok && data_ok, "every block keeps its own data");

    for (int i = 0; i < N && all_ok; i++)
        for (int j = 0; j < N; j++) {
            if (i == j) continue;
            if (p[i] < p[j] + sz[j] && p[j] < p[i] + sz[i]) no_overlap = 0;
        }
    check("T2.3", all_ok && no_overlap, "no two blocks overlap");

    for (int i = 0; i < N; i++) my_free(p[i]);
    check("T2.4", heap_check(), "heap is consistent after freeing all");
}

/* T3: freed memory is reused and the block is split */
static void t3(void)
{
    printf("-- T3 reuse and split\n");
    char *a = my_malloc(1000);
    char *guard = my_malloc(16);        /* keeps a from merging with the tail */
    my_free(a);
    heap_stats_t before = stats();

    char *c = my_malloc(200);
    heap_stats_t after = stats();
    check("T3.1", c == a, "freed block is reused for a smaller request");
    check("T3.2", after.heap_bytes == before.heap_bytes, "heap did not grow");
    check("T3.3", after.free_bytes > 0 && after.free_blocks >= 1,
          "remainder of the split shows up as a free block");
    my_free(c);
    my_free(guard);
    check("T3.4", heap_check(), "heap is consistent");
}

/* T4: coalescing when freeing out of order */
static void t4(void)
{
    printf("-- T4 coalescing\n");
    heap_stats_t base = stats();
    char *a = my_malloc(128);
    char *b = my_malloc(128);
    char *c = my_malloc(128);
    char *guard = my_malloc(16);

    my_free(a);
    my_free(c);
    heap_stats_t mid = stats();
    check("T4.1", mid.free_blocks >= 2, "a and c are separate free blocks");

    my_free(b);
    heap_stats_t end = stats();
    check("T4.2", end.free_blocks == mid.free_blocks - 1,
          "freeing b merges a, b and c into one block");
    check("T4.3", end.largest_free >= 3 * 128, "merged block holds all three");
    my_free(guard);
    heap_stats_t fin = stats();
    check("T4.4", heap_check() && fin.free_blocks == base.free_blocks,
          "after freeing everything the free-block count is back to the start");
}

/* T5: calloc and realloc */
static void t5(void)
{
    printf("-- T5 calloc and realloc\n");
    unsigned char *z = my_calloc(100, 8);
    int zero = z != NULL;
    for (int i = 0; zero && i < 800; i++) if (z[i] != 0) zero = 0;
    check("T5.1", zero, "my_calloc returns zeroed memory");
    my_free(z);

    check("T5.2", my_calloc(SIZE_MAX / 2, 4) == NULL,
          "my_calloc detects multiplication overflow");

    char *r = my_realloc(NULL, 50);
    check("T5.3", r != NULL && aligned16(r), "my_realloc(NULL, n) acts like my_malloc");

    for (int i = 0; i < 50; i++) r[i] = (char)i;
    char *g = my_realloc(r, 5000);
    int kept = g != NULL;
    for (int i = 0; kept && i < 50; i++) if (g[i] != (char)i) kept = 0;
    check("T5.4", kept, "growing with my_realloc preserves contents");

    char *s = my_realloc(g, 20);
    int same = (s == g);
    for (int i = 0; same && i < 20; i++) if (s[i] != (char)i) same = 0;
    check("T5.5", same, "shrinking returns the same pointer and keeps data");

    check("T5.6", my_realloc(s, 0) == NULL && heap_check(),
          "my_realloc(p, 0) frees the block and returns NULL");
}

/* T6: heap growth through sbrk */
static void t6(void)
{
    printf("-- T6 heap growth\n");
    heap_stats_t before = stats();
    size_t big = 300 * 1024;
    unsigned char *p = my_malloc(big);
    heap_stats_t after = stats();
    check("T6.1", p != NULL && aligned16(p), "300 KB request succeeds");
    check("T6.2", after.heap_bytes > before.heap_bytes, "heap grew through sbrk");
    int ok = p != NULL;
    if (ok) {
        memset(p, 0xAB, big);
        for (size_t i = 0; i < big; i += 4096) if (p[i] != 0xAB) ok = 0;
    }
    my_free(p);
    check("T6.3", ok && heap_check(), "whole block is writable; heap consistent");
}

/* T7: first-fit and best-fit choose different holes */
static void t7(void)
{
    printf("-- T7 placement policy\n");
    char *big   = my_malloc(512);
    char *g1    = my_malloc(16);
    char *small = my_malloc(128);
    char *g2    = my_malloc(16);
    my_free(big);
    my_free(small);

    set_policy(FIT_FIRST);
    char *f = my_malloc(100);
    check("T7.1", f == big, "first-fit picks the first hole (512)");
    my_free(f);

    set_policy(FIT_BEST);
    char *b = my_malloc(100);
    check("T7.2", b == small, "best-fit picks the smallest hole (128)");
    my_free(b);

    set_policy(FIT_FIRST);
    my_free(g1);
    my_free(g2);
}

/* T8: stress test */
static void t8(void)
{
    printf("-- T8 stress test\n");
    enum { SLOTS = 256, OPS = 20000 };
    unsigned char *p[SLOTS] = {0};
    size_t sz[SLOTS] = {0};
    int data_ok = 1, consistent = 1;
    srand(12345);

    for (int op = 0; op < OPS; op++) {
        int i = rand() % SLOTS;
        if (p[i]) {
            for (size_t k = 0; k < sz[i]; k++)
                if (p[i][k] != (unsigned char)(i ^ k)) { data_ok = 0; break; }
            if (rand() % 3 == 0) {               /* resize */
                size_t n = (size_t)(rand() % 4096) + 1;
                unsigned char *q = my_realloc(p[i], n);
                if (q == NULL) { data_ok = 0; continue; }
                size_t keep = n < sz[i] ? n : sz[i];
                for (size_t k = 0; k < keep; k++)
                    if (q[k] != (unsigned char)(i ^ k)) { data_ok = 0; break; }
                for (size_t k = 0; k < n; k++) q[k] = (unsigned char)(i ^ k);
                p[i] = q; sz[i] = n;
            } else {
                my_free(p[i]);
                p[i] = NULL;
            }
        } else {
            size_t n = (size_t)(rand() % 4096) + 1;
            p[i] = my_malloc(n);
            if (p[i] == NULL || !aligned16(p[i])) { data_ok = 0; p[i] = NULL; continue; }
            sz[i] = n;
            for (size_t k = 0; k < n; k++) p[i][k] = (unsigned char)(i ^ k);
        }
        if (op % 1000 == 0 && !heap_check()) consistent = 0;
    }
    for (int i = 0; i < SLOTS; i++) my_free(p[i]);
    check("T8.1", data_ok, "20,000 random operations keep data intact");
    check("T8.2", consistent && heap_check(), "heap stays consistent throughout");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);   /* print as we go */
    t1(); t2(); t3(); t4(); t5(); t6(); t7(); t8();
    printf("\n%d/%d checks passed\n", passed, passed + failed);
    return failed == 0 ? 0 : 1;
}
