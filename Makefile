CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g

all: test

test_myalloc: myalloc.c test_myalloc.c myalloc.h
	$(CC) $(CFLAGS) -o test_myalloc myalloc.c test_myalloc.c

test: test_myalloc
	./test_myalloc

asan: myalloc.c test_myalloc.c myalloc.h
	$(CC) -std=c11 -g -fsanitize=address,undefined -o t_asan myalloc.c test_myalloc.c
	./t_asan

bench: myalloc.c bench.c myalloc.h
	$(CC) $(CFLAGS) -O2 -o bench myalloc.c bench.c
	./bench first
	./bench best
	./bench libc

clean:
	rm -f test_myalloc t_asan bench *.o *.exe

.PHONY: all test asan bench clean
