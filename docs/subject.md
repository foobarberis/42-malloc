# General Instructions

- The library must be named libft_malloc_$HOSTTYPE.so.

- A Makefile must compile the project and must contain the usual rules. It
  must recompile and re-link the program only if necessary. Refer to the
  Makefile in `libft` as an example.

- The  Makefile MUST check the existence of the environment variable
  `$HOSTTYPE`. If it is empty or non-existant, to assign the following
  value:

```makefile
ifeq ($(HOSTTYPE),)
	HOSTTYPE := $(shell uname -m)_$(shell uname -s)
endif
```

- The Makefile will have to create a symbolic link `libft_malloc.so`
  pointing to `libft_malloc_$HOSTTYPE.so` so for example: `libft_malloc.so
  -> libft_malloc_intel-mac.so`

- The `libft` folder contains implementations of libc functions. The
  project Makefile must compile the library `libft.a` using the Makefile
  located in the `libft` folder and link it.

- ONE global variable is allowed to manage allocations.

- Errors must be handled carefully, undefined behaviour or segv are
  UNACCEPTABLE.

- The ONLY libc functions allowed for this project are:
	- mmap(2)
	- munmap(2)
	- getpagesize under OSX or sysconf(_SC_PAGESIZE) under linux
	- getrlimit(2)
	- The authorized functions within the libft and the functions from the `libft` folder.

# Project instructions

- The goal of this project is to re-write the following libc functions:
	- malloc(3)
	- free(3)
	- realloc(3)

- The functions MUST be prototyped as follow

```c
#include <stdlib.h>

void *malloc(size_t size);
void free(void *ptr);
void *realloc(void *ptr, size_t size);
```

- These functions DO NOT have to be thread-safe.

- The memory returned by `malloc` MUST be aligned.

- The malloc() function allocates `size` bytes of memory and returns a
pointer to the allocated memory.

- The `realloc()` function tries to change the size of the allocation
  pointed to by `ptr` to `size`, and returns `ptr`. If there is not enough
  room to enlarge the memory allocation pointed to by `ptr`, `realloc()`
  creates a new allocation, copies as much of the old data pointed to by
  `ptr` as will fit to the new allocation, frees the old allocation, and
  returns a pointer to the allocated memory.

- The `free()` function deallocates the memory allocation pointed to by `ptr`.
  If `ptr` is a `NULL` pointer, no operation is performed.

- If there is an error, the `malloc()` and `realloc()` functions return a `NULL` pointer.

- The `mmap(2)` and `munmap(2)` syscall MUST be used to claim and return
  the memory zones to the system.

- The management of our own memory allocations for the internal functioning
  of our implementation of `malloc` MUST NOT rely on the libc `malloc`
  function.

- With performance in mind, you must limit the number of calls to `mmap()`,
  but also to `munmap()`. You have to “pre-allocate” some memory zones
  tostore your “small” and “medium” malloc.

- The size of these zones must be a multiple of `getpagesize()` under OSX
or `syscon (_SC_PAGESIZE)` under Linux.

- Each zone must contain at least 100 allocations.
	- "TINY" mallocs, from 1 to n bytes, will be stored in N bytes big zones.
	- "SMALL" mallocs, from (n+1) to m bytes, will be stored in M bytes big zones.
	- "LARGE" mallocs, fron (m+1) bytes and more, will be stored out of zone, which simply means with `mmap()`, they will be in a zone on their own.

- It’s up to you to define the size of n, m, N and M so that you find a
  good compromise between speed (saving on system recall) and saving
  memory.

- You MUST write a function with the following prototype `void
  show_alloc_mem();` in charge of displaying a representation of the
  allocated memory zones. The visual will be formatted by increasing
  addresses like so:

```
TINY : 0xA0000
0xA0020 - 0xA004A : 42 bytes
0xA006A - 0xA00BE : 84 bytes
SMALL : 0xAD000
0xAD020 - 0xADEAD : 3725 bytes
LARGE : 0xB0000
0xB0020 - 0xBBEEF : 48847 bytes
Total : 52698 bytes
```

# Evaluation

## Preliminary checks

First check the following elements:

- A Makefile is present and has all the requested rules.
- The project does not contain any unauthorized functions.
- AT MOST one global variable is used to manage the allocations.

### Library compilation

- Check that the compilation of the library does generate the requested
  files by modifying `HOSTTYPE`:

```sh
$ export HOSTTYPE=Testing
$ make re
...
$ ln -s libft_malloc_Testing.so libft_malloc.so
$ ls -l libft_malloc.so

libft_malloc.so -> libft_malloc_Testing.so
```

The Makefile must use `HOSTTYPE` to define the name of the library
(`libft_malloc_$HOSTTYPE.so`) and create a symbolic link `libft_malloc.so`
pointing towards `libft_malloc_$HOSTTYPE.so`.

### Functions export

Check with `nm` that the library exports the functions `malloc`, `free`, `realloc` and `show_alloc_mem`:

```sh
$ nm libft_malloc.so
0000000000000000 T _free
0000000000000000 T _malloc
0000000000000000 T _realloc
0000000000000000 T _show_alloc_mem
U _mmap
U _munmap
U _getpagesize
U _write
```

Exported functions are marked with `T`.

## Feature testing

Start by creating a script that will only modify the environment variables while you run a test program. It will be named `run.sh`, and be executable:

```sh
#!/bin/sh
export DYLD_LIBRARY_PATH=.
export DYLD_INSERT_LIBRARIES="libft_malloc.so"
export DYLD_FORCE_FLAT_NAMESPACE=1
$@
```

### Malloc test

Make a small test program that does not use malloc as a baseline:

```c
#include <unistd.h>

int main()
{
    int i; char *addr;
    i = 0;
    while (i < 1024)
    {
        i++;
    }
    return (0);
}
```

Compile and run with `/usr/bin/time -v` to record memory usage and page reclaims.

Then add a malloc and write to each allocation so pages become resident:

```c
#include <stdlib.h>

int main()
{
    int i;
    char *addr;
    i = 0;
    while (i < 1024)
    {
        addr = (char*)malloc(1024);
        addr[0] = 42;
        i++;
    }
    return (0);
}
```

Run the tests both with and without the library and compare the "maximum resident set size" and "page reclaims" values to evaluate overhead.

Scoring by page count (example):

- less than 255 pages: allocated memory is insufficient: 0
- 1023 pages and over: malloc works but consumes 1 page minimum for each allocation: 1
- between 513 and 1022 pages: malloc works but overhead is too big: 2
- between 313 and 512 pages: overhead is very big: 3
- between 273 and 312 pages: overhead is big: 4
- between 255 and 272 pages: malloc works and the overhead is fine: 5

The defender may justify another method of counting allocated pages (using existing debug for example).

### Pre-allocated zones

Check inside the source code that the pre-allocated zones for the different malloc sizes allow to store at least 100 times the maximum size for this type of zone. Check also that the size of the zones is a multiple of `getpagesize()`.

### Tests of free

Add a free to the test program and compare page reclaims to determine if `free` works:

```c
#include <stdlib.h>

int main()
{
    int i;
    char *addr;
    i = 0;
    while (i < 1024)
    {
        addr = (char*)malloc(1024);
        addr[0] = 42;
        free(addr);
        i++;
    }
    return (0);
}
```

If `free` produces as many or more page reclaims than the `malloc` test, then `free` doesn't work.

Quality of the `free` function: test2 should have at most 3 more page reclaims than test0.

### Realloc test

Example test for `realloc`:

```c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#define M (1024 * 1024)

void print(char *s)
{
    write(1, s, strlen(s));
}

int main()
{
    char *addr1;
    char *addr3;
    addr1 = (char*)malloc(16*M);
    strcpy(addr1, "Bonjour\n");
    print(addr1);
    addr3 = (char*)realloc(addr1, 128*M);
    addr3[127*M] = 42;
    print(addr3);
    return (0);
}
```

Run with `./run.sh ./test3` and verify output matches expectations.

Also test a scenario where another allocation exists before `realloc` to ensure copying/relocation works.

```c
int main()
{
	char *addr1;
	char *addr2;
	char *addr3;

	addr1 = (char*)malloc(16*M);
	strcpy(addr1, "Bonjour\n");
	print(addr1);
	addr2 = (char*)malloc(16*M);
	addr3 = (char*)realloc(addr1, 128*M);
	addr3[127*M] = 42; print(addr3);
	return (0);
}
```

### Error handling

Test special cases and errors (freeing NULL, freeing offset pointers, invalid realloc):

```c
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void print(char *s)
{
    write(1, s, strlen(s));
}

int main()
{
    char *addr;
    addr = malloc(16);
    free(NULL);
    free((void *)addr + 5);
    if (realloc((void *)addr + 5, 10) == NULL)
        print("Bonjour\n");
}
```

If `realloc` returns `NULL` and "Bonjour" is displayed, behavior matches the example. If the program crashes, select the "crash" flag.

### show_alloc_mem test

Example:

```c
int main()
{
    malloc(1024);
    malloc(1024 * 32);
    malloc(1024 * 1024);
    malloc(1024 * 1024 * 16);
    malloc(1024 * 1024 * 128);
    show_alloc_mem();
    return (0);
}
```

Does the display correspond to the project's TINY/SMALL/LARGE allocation categories?
