# 42 Malloc - Custom Memory Allocator

A zone-based, `mmap`-backed implementation of the dynamic memory allocation family (`malloc`, `free`, `realloc`) for the 42 project. This implementation focuses on minimizing syscalls through pre-allocation and ensuring strict 16-byte alignment.

---

## Project Structure

- **[inc/malloc.h](inc/malloc.h)**: Core header containing structure definitions and alignment macros.
- **[src/malloc.c](src/malloc.c)**: Entry point for `malloc` and heap extension logic.
- **[src/free.c](src/free.c)**: Implementation of `free`, including pointer validation and heap unmapping.
- **[src/realloc.c](src/realloc.c)**: Logic for resizing allocations, optimizing for in-place growth.
- **[src/utils.c](src/utils.c)**: Internal helpers for block splitting and coalescing.
- **[src/show_alloc_mem.c](src/show_alloc_mem.c)**: Memory visualization utility.
- **[test/](test/)**: Comprehensive test suite including subject-specific evaluation tests.

---

## Quick Start

### Build the Library
The project compiles into a shared library. If `HOSTTYPE` is not set, it defaults to your system architecture.

```bash
make
```
This produces:
- `libft_malloc_$(HOSTTYPE).so` (The physical shared object)
- `libft_malloc.so` (A symbolic link for convenience)

### Usage
To use this allocator with any existing command (on Linux):

```bash
LD_PRELOAD=./libft_malloc.so ls
```

To run the internal test suite:
```bash
make test
./test/test_main
```

To run the evaluation specific tests:
```bash
make evaluate
```

---

## General Concepts

This section provides a foundation for understanding how memory management works at the operating system level.

-   **Virtual Memory:** An abstraction over physical memory that allows each program to view memory as one contiguous block with its own dedicated address space, even if physical RAM is fragmented.
-   **Pages:** Virtual memory is divided into **pages**, usually **4096 bytes** (4KB). The OS maps these virtual pages to physical RAM on demand.
-   **System Calls:**
    -   `mmap(2)`: Requests memory from the OS. It returns a pointer to a memory area that is a multiple of the page size.
    -   `munmap(2)`: Returns memory back to the OS.
    -   `sysconf(_SC_PAGESIZE)`: Retrieves the exact system page size (typically 4096 on Linux).
-   **Minor Page Faults (The Proxy for Memory Usage):**
    -   When we request memory via `mmap` with `MAP_ANONYMOUS`, the kernel doesn't immediately allocate physical RAM (Resident Set Size - RSS). Instead, it updates the process's page table with "empty" entries.
    -   A **Minor Page Fault** occurs the first time a program attempts to read from or write to a virtual page that has been mapped but not yet "backed" by physical memory.
    -   By measuring minor page faults (e.g., via `/usr/bin/time -v`), we can verify that our allocator is efficiently managing memory. If we see a surge in page faults, it means we are requesting new pages from the OS rather than reusing existing ones in our TINY/SMALL zones.
-   **Alignment:**
    - CPU access is faster (and sometimes required) when data lies on specific address boundaries.
    - We ensure all returned pointers are **16-byte aligned** to support all standard C types and vector instructions (SSE/AVX).
-   **Fragmentation:**
    -   **Internal:** Wasted space *inside* an allocated block (e.g., due to alignment padding).
    -   **External:** Wasted space *between* allocated blocks (free blocks too small to satisfy new requests).
-   **Metadata:** To manage memory, we prepend a "header" to each chunk containing its size and status. The pointer returned to the user starts immediately after this header.
-   **Zones (Heaps):** To avoid calling `mmap` for every tiny allocation, we pre-allocate large "zones" and carve small allocations out of them.

---

## Architecture & Memory Layout

We use a **Zone-based** allocation strategy to minimize `mmap` syscall frequency.

### Memory Tiers

| Tier | Size Range (Bytes) | Zone Size (Pages) | Allocations/Zone |
| :--- | :--- | :--- | :--- |
| **TINY** | 1 - 128 | 4 Pages (16 KB) | ~100+ |
| **SMALL** | 129 - 1024 | 26 Pages (104 KB) | ~100+ |
| **LARGE** | > 1024 | Exact + Metadata | 1 (Individual) |

### Zone Calculation Logic

The subject requires that each TINY/SMALL zone stores at least 100 allocations. To calculate the required `mmap` size, we use the formula:
`ZoneSize = (MaxRequestSize + BlockOverhead) * 100`, rounded up to the nearest system page.

- **Block Overhead (32 bytes):** Our `t_block` metadata header consists of a `size_t` (8B), a pointer (8B), and an `int` (4B). While the raw size is 20 bytes, the `__attribute__((aligned(16)))` forces the header to be **32 bytes** to ensure the user payload that follows is 16-byte aligned.
- **TINY Zone:** `(128 + 32) * 100 = 16,000` bytes. Next page boundary is **16,384** (4 Pages).
- **SMALL Zone:** `(1024 + 32) * 100 = 105,600` bytes. Next page boundary is **106,496** (26 Pages).

### Data Structures

#### 1. The Heap Header (`t_heap`) - 48 Bytes
Placed at the start of every `mmap`'d region.
```text
+-------------------+-------------------+-------------------+
|      *prev        |      *next        |    total_size     |  (8B each)
+-------------------+-------------------+-------------------+
|     free_size     |    block_count    |    *block_list    |  (8B each)
+-------------------+-------------------+-------------------+
```

#### 2. The Block Header (`t_block`) - 32 Bytes
Prepended to every allocation. Ensures the payload start is **16-byte aligned**.
```text
+-------------------+-------------------+-------------------+
|       size        |       *next       |      is_free      |  (8-8-4-12pad)
+-------------------+-------------------+-------------------+
|                     (12B Alignment Padding)               |
+-------------------+-------------------+-------------------+
|              USER PAYLOAD STARTS HERE (Aligned)           |
+-------------------+-------------------+-------------------+
```

---

## Core Algorithms

#### Malloc
1. **Routing**: Determine if the request is TINY, SMALL, or LARGE based on the aligned size.
2. **First-Fit Search**: Iterate through heaps of the required tier, then through their block lists to find the first free block that can accommodate the request.
3. **Splitting**: If a found block is significantly larger than requested (at least `sizeof(t_block) + ALIGNMENT` extra), it is split to create a new free block and minimize internal fragmentation.
4. **Expansion**: If no existing heap can satisfy the request, a new heap is `mmap`'d (4 pages for TINY, 26 for SMALL, or custom size for LARGE) and appended to the global list.

#### Free
1. **Verification**: Checks if the pointer exists within the address ranges of our allocated zones to avoid segmentation faults on invalid pointers.
2. **Coalescing**: If the adjacent block (next) or the previous block is also free, they are merged into a single larger block. This reduces external fragmentation.
3. **Cleanup**: If a heap becomes empty (`block_count == 0`), it is `munmap`'d to return memory to the OS.
   - **Optimization**: To avoid excessive syscalls, we keep one empty heap of each tier (TINY/SMALL) as a cache for future allocations.

#### Realloc
1. **In-place Growth**: If the requested size is larger but the `next` block is free and provides enough space when combined, we merge them in-place.
2. **In-place Shrink**: If the requested size is smaller and the difference allows for a split, we split the block and free the remainder.
3. **Fallback**: If in-place growth isn't possible, we `malloc` a new area, `f_memcpy` the data, and `free` the old pointer.

#### Memory Visualization (`show_alloc_mem`)
The library provides a diagnostic function that prints the current state of all heaps to the standard output.
- **TINY**: Starts the list of tiny zones.
- **SMALL**: Starts the list of small zones.
- **LARGE**: Starts the list of large allocations.
- **Output Format**:
  ```text
  TINY : 0x7f830a200000
  0x7f830a200030 - 0x7f830a2000b0 : 128 bytes
  0x7f830a2000e0 - 0x7f830a200160 : 128 bytes
  Total : 256 bytes
  ```

---

## Testing Suite

### Internal Reliability
- `make test`: Compiles [test/test_main.c](test/test_main.c) which exercises:
  - Error handling (freeing NULL, freeing offset pointers).
  - Data integrity across reallocations (16M -> 128M).
  - Double free detection (logged to stderr).

### Evaluation Benchmarks
Specific tests from the subject are located in [test/eval/](test/eval/):
- **Test 0-2**: Measure page fault counts using `/usr/bin/time -v`.
- **Test 3-4**: Validate realloc behavior and memory fragmentation.
- **Test 5**: Verification of the `show_alloc_mem()` output format.

---

## Defense / FAQ

**Q: Why is the page count ~300 at start?**
A: This is the overhead of the dynamic linker, shared libraries (libc), and the initial environment. Our allocator also pre-allocates the first TINY/SMALL zones upon the first request.

**Q: Why use 16-byte alignment?**
A: Modern 64-bit systems require 16-byte alignment for instructions like SSE/AVX. It ensures that `long double` and vector types do not cause alignment faults.

**Q: How do you handle fragmentation?**
A: We use **Splitting** during allocation to prevent internal wastage and **Coalescing** during `free` to reunite adjacent fragments.

**Q: What is the "100 allocations" rule?**
A: The subject requires that each zone holds at least 100 allocations. Our calculations:
- TINY: `(128 + 32) * 100 = 16,000` -> 4 Pages (16,384B on a 4KB page system).
- SMALL: `(1024 + 32) * 100 = 105,600` -> 26 Pages (106,496B on a 4KB page system).

---

## Development & Standards

### Coding Standards
- **Standard Library**: Aside from the few libc functions which are authorized for this project, we rely on our own `libft` (prefixed with `f_`).
- **Style**: We follow the 42 Norm (mostly), using spaces inside parentheses `malloc( size )` and clear metadata structures.
- **Safety**: Robust error checking on `mmap` and pointer validation during `free` to prevent crashes from bad user input.

### Debugging
Toggle `LOGGING_ENABLED` in [inc/malloc.h](inc/malloc.h) to see detailed traces of block splitting, coalescing, and system calls. For example:
- `extend_heap`: Logs when a new zone is created.
- `split_block`: Logs how a block is carved.
- `coalesce_blocks`: Logs when blocks are merged.

