# Task 8: First-fit vs best-fit

**Author:** Austin Ngunjiri (SCT212-0493/2023)

## Method

`bench.c` builds one random sequence of 50,000 operations with a fixed
seed (`srand(2026)`). Each operation picks one of 1,000 slots. If the
slot is empty it allocates 1–4096 bytes and writes to the block, and if
it is full it frees the block. The sequence is generated before the
timer starts, so only allocator work is timed (`clock_gettime`,
`CLOCK_MONOTONIC`). Each policy runs in a separate process (`./bench
first`, `./bench best`, `./bench libc`), so every run starts from an
empty heap. The statistics are read with `heap_stats()` at the end of
the sequence, while 530 blocks are still live.

Built with `gcc -O2` and run on Cygwin (Windows 11, gcc 14.4.0). The
benchmark was run 4 times. The heap statistics were identical every
time, and the times below are the range over the 4 runs.

## Results

| Measure | First-fit | Best-fit |
|---|---:|---:|
| heap_bytes (from sbrk) | 1,310,720 | 1,310,720 |
| used_bytes (live payload) | 1,016,656 | 1,017,728 |
| free_bytes | 270,192 | 271,424 |
| free_blocks | 216 | 144 |
| largest_free | 107,520 | 85,792 |
| External fragmentation `1 - largest_free / free_bytes` | **0.602** | **0.684** |
| `heap_check()` after run | 1 | 1 |

| Allocator | Time for 50,000 ops |
|---|---:|
| my_malloc, first-fit | 258 – 335 ms |
| my_malloc, best-fit | 273 – 400 ms |
| glibc-style C library `malloc`/`free` (Cygwin's) | 6.0 – 8.8 ms |

## Observations

- Both policies needed the same amount of memory from the OS (20 chunks
  of 64 KB).
- Best-fit left fewer free blocks (144 vs 216), because it fills the
  hole that fits most tightly and so leaves fewer small leftovers.
  However, its largest free block was smaller, so its external
  fragmentation figure was higher (0.684 vs 0.602). Best-fit tends to
  carve pieces out of medium holes and leave the big one alone less
  often than you might expect.
- Best-fit's used_bytes is about 1 KB higher. When the leftover after a
  split would be smaller than a header plus 16 bytes, the whole block is
  handed out. Best-fit hits this case more often, which is internal
  fragmentation.
- Best-fit was a little slower on average, because it always scans the
  whole list unless it finds an exact match.
- The C library allocator was about 40× faster than both policies.

## Discussion

<!-- TODO: about 150 words, in my own words: which policy I would
choose and why, and one reason my allocator is slower or less
space-efficient than the C library's. -->
