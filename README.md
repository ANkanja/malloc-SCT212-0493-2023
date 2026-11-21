# malloc: my own memory allocator on sbrk()

**Name:** Austin Ngunjiri
**Student ID:** SCT212-0493/2023
**Course:** ICS 2305 Systems Programming, practical assignment (Question 2)

`myalloc.c` implements `my_malloc`, `my_free`, `my_calloc` and
`my_realloc` on top of `sbrk()`. It keeps every block in an
address-ordered linked list and supports first-fit and best-fit
placement, block splitting and coalescing.

> **Note about the starter files.** I could not get the starter pack
> from Piazza. So `myalloc.h`, `test_myalloc.c` and the `Makefile` in
> this repository were written by following the brief. The header uses
> the API in Section 3.1, and the harness has the 8 groups (T1–T8) and
> 29 checks described in Section 7.1. If the official files are used
> instead, `myalloc.c` may need small changes to match the official
> `heap_stats_t` field names.

## Build and run

Needs Linux (or WSL/Cygwin) with `gcc` and `make`.

```sh
git clone https://github.com/ANkanja/malloc-SCT212-0493-2023.git
cd malloc-SCT212-0493-2023
make            # builds and runs the test harness
make asan       # runs the tests with AddressSanitizer + UBSan (Linux)
make bench      # runs the Task 8 benchmark
make clean
```

GitHub Actions also builds the code on Ubuntu with `-Werror`, runs the
harness normally and under AddressSanitizer/UBSan, and runs the
benchmark on every push (see the Actions tab).

## Tasks

| Task | Description | Status |
|------|-------------|--------|
| 1 | Block header, `align_up`, `payload`, `header`, `HDR` | Complete |
| 2 | `grow()` with sbrk, 64 KB chunks, alignment padding | Complete |
| 3 | `my_malloc` with first-fit | Complete |
| 4 | Block splitting | Complete |
| 5 | `my_free`, coalescing, double-free / invalid-pointer detection | Complete |
| 6 | `my_calloc` (overflow check) and `my_realloc` (all five cases) | Complete |
| 7 | Best-fit, `heap_stats`, `heap_dump`, `heap_check` | Complete |
| 8 | `bench.c` and `REPORT.md` | Complete |

## Test results

Last lines of `make`:

```
-- T7 placement policy
PASS T7.1  first-fit picks the first hole (512)
PASS T7.2  best-fit picks the smallest hole (128)
-- T8 stress test
PASS T8.1  20,000 random operations keep data intact
PASS T8.2  heap stays consistent throughout

29/29 checks passed
```

## Design explanation

<!-- DRAFT by Claude: edit into your own words before submitting. -->

**Header layout.** Every block starts with a 32-byte header: `size`
(payload bytes, always a multiple of 16), `next` (the next block in the
list), `free` (1 or 0) and `magic` (a constant, `0x4D594131`). The
struct itself is 24 bytes, and `HDR` rounds it up to 32 so that the
payload, which starts `HDR` bytes after the header, is always 16-byte
aligned. `magic` lets `my_free` and `heap_check` tell a real header
from random memory.

**The list.** There is one singly linked list holding every block,
free and used, in increasing address order, with global `head` and
`tail` pointers. `grow()` asks `sbrk()` for at least 64 KB, adds
padding if the break is not 16-byte aligned, and appends the new
memory as one free block. If that memory starts right after a free
tail block, the tail is extended. `my_malloc` rounds the request up to
16, walks the list for a free block that fits (first-fit or best-fit),
calls `grow()` if nothing fits, and then splits the block if the
leftover can hold a header plus 16 bytes.

**Coalescing.** `my_free` first checks that the pointer is the payload
of a block in the list, so a bad pointer is reported instead of
crashing. Then it marks the block free and calls `coalesce()`, which
walks the list once. Whenever a free block is followed by another free
block, it compares addresses: they are merged only if the first
block's payload ends exactly where the second header begins (`size +=
HDR + next->size`, then unlink). If the C library moved the break in
between, there is a gap, so the blocks are left separate. This O(n)
scan is simple and keeps the invariant that no two adjacent free
blocks exist, which `heap_check` verifies.

## Questions

<!-- DRAFT by Claude: edit into your own words before submitting. -->

1. **Why must the header size be a multiple of 16 as well as the payload sizes?**
   The pointer given to the user is the header address plus `HDR`. If
   the header start is aligned but `HDR` were, say, 24, every payload
   would sit 8 bytes off a 16-byte boundary. With `HDR` and every
   payload size a multiple of 16, each block ends on a 16-byte
   boundary, so the next header and its payload stay aligned too.

2. **Why can `printf()` inside `my_malloc()` cause infinite recursion?**
   `printf` may call `malloc` internally to set up its output buffer.
   If the allocator itself were installed as `malloc`, that inner call
   would come back into `my_malloc`, which calls `printf` again, and so
   on until the stack overflows. Even with the `my_` names, stdio can
   move the break in the middle of our own `sbrk` work. So the
   allocator only uses `snprintf` into a local buffer and `write()`.

3. **`my_free()` is O(n). Which extra field would let you find a block's previous neighbour in O(1), and what is the technique called?**
   Add a footer at the end of every block that copies its size (and
   free bit). The block just before a header can then be found by
   reading the footer that sits immediately before that header. This
   is the **boundary tag** technique (Knuth). With a `prev` pointer or
   boundary tags, plus an explicit free list, free and merge become
   O(1).

4. **What is the difference between internal and external fragmentation, and where does each come from in this allocator?**
   Internal fragmentation is wasted space inside an allocated block.
   Here it comes from rounding every request up to 16 bytes, from the
   32-byte header, and from handing out a whole block when the leftover
   is too small to split. External fragmentation is free memory split
   into holes that are each too small for a request even though the
   total is big enough. It comes from freeing blocks between used ones.
   The benchmark measured it as 0.60 for first-fit and 0.68 for
   best-fit.

5. **Why is sbrk memory never returned to the OS here, and how could memory at the end of the heap be given back?**
   `my_free` only marks blocks free and merges them; it never lowers
   the break. Also, memory in the middle of the heap cannot be returned
   with `sbrk`, because the break only moves at the end. To give memory
   back, `my_free` could check whether the merged free block is the
   tail *and* ends exactly at `sbrk(0)`. If so, it would unlink the
   tail and call `sbrk(-amount)` (keeping a small reserve to avoid
   calling the OS too often).

6. **Two blocks are consecutive in the list but not adjacent in memory. How did this happen, and what does the coalescing code do about it?**
   Between two calls to `grow()`, something else moved the break, for
   example the C library's own `malloc` used by `printf` in the tests.
   So the new chunk starts after a gap instead of right after our last
   block. `coalesce()` checks `block_end(b) == (char *)b->next` before
   merging, so blocks separated by a gap are never merged, and the gap
   is simply skipped.

## Known limitations

- Not thread-safe: there is no locking around the global list.
- `my_free` and `my_malloc` are O(n): they walk the whole list
  (coalescing scans the list on every free). This is the main reason
  the allocator is about 40× slower than the C library.
- Memory is never returned to the OS, even when the last block is free.
- `heap_bytes` includes the few padding bytes added to align the first
  block of each chunk.
- AddressSanitizer is not available on Cygwin, so the ASan run happens
  on Linux through GitHub Actions.

## Sources and AI-tool declaration

I used Claude (an AI assistant) to explain sbrk and allocator design,
to write the allocator code step by step with me (one commit per task),
to write the replacement header, test harness and benchmark because I
could not get the starter pack, to troubleshoot and run the tests, and
to draft the README and report. Claude also drafted the design
explanation, the answers to the questions and the report discussion,
which I then reviewed and edited. I made sure I understand how each
part of the code works.
