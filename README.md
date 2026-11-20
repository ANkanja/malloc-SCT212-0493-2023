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

<!-- TODO: 200-300 words in my own words: header layout, how the list
is organised, how coalescing works. -->

## Questions

<!-- TODO: answers in my own words, 2-4 sentences each. -->

1. **Why must the header size be a multiple of 16 as well as the payload sizes?**

2. **Why can `printf()` inside `my_malloc()` cause infinite recursion?**

3. **`my_free()` is O(n). Which extra field would let you find a block's previous neighbour in O(1), and what is the technique called?**

4. **What is the difference between internal and external fragmentation, and where does each come from in this allocator?**

5. **Why is sbrk memory never returned to the OS here, and how could memory at the end of the heap be given back?**

6. **Two blocks are consecutive in the list but not adjacent in memory. How did this happen, and what does the coalescing code do about it?**

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
to draft the README and report. I wrote the design explanation, the
answers to the questions and the report discussion myself, and I made
sure I understand how each part of the code works.
