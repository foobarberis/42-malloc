# Copilot Instructions for 42-malloc

## 🏗 Architecture & Design
This is a zone-based memory allocator using `mmap` to request large segments (heaps) from the OS, then partitioning them into smaller blocks.

- **TINY Zone**: Blocks up to 128 bytes. Zone size fixed at 4 pages.
- **SMALL Zone**: Blocks up to 1024 bytes. Zone size fixed at 26 pages.
- **LARGE Zone**: Blocks > 1024 bytes. Each is its own `mmap`'d heap.
- **Alignment**: Every pointer returned by `malloc` **must** be 16-byte aligned.

### Data Structures
- [inc/malloc.h](inc/malloc.h): Contains the core `t_heap` (zone header, 48B) and `t_block` (inline chunk header, 32B) structures.
- Both structures use `__attribute__((aligned(16)))`.

## 🛠 Developer Workflows
- **Build**: `make` creates `libft_malloc.so`.
- **Test**: `make test` runs internal tests using `LD_PRELOAD`.
- **Evaluate**: `make evaluate` runs performance comparisons against system `malloc`.
- **Usage**: Test with any binary via `LD_PRELOAD=./libft_malloc.so <cmd>`.

## 📝 Coding Standards & Patterns
- **Library Dependency**: Use the custom `libft` (prefixed with `f_`). For example, use `f_printf` or `f_memcpy` instead of standard ones.
- **Parentheses Style**: Maintain spaces inside parentheses for control flow and function calls: `if ( condition )`, `malloc( size )`.
- **Logging**: Use [inc/malloc.h](inc/malloc.h)'s `LOGGING_ENABLED` macro and `f_dprintf( 2, "..." )` for debug output to `stderr`.
- **Memory Safety**:
    - Always `align()` the requested size up to 16 bytes before processing.
    - Check `MAP_FAILED` after `mmap`.
    - Block splitting logic is in `split_block`, and merging is in `coalesce_blocks`. Both are in [src/utils.c](src/utils.c).

## 🔍 Key Files
- [src/malloc.c](src/malloc.c): Core entry point and heap extension logic.
- [src/free.c](src/free.c): Deallocation, coalescing, and `munmap` logic.
- [src/realloc.c](src/realloc.c): Reallocation logic (optimizes by checking if next block is free).
- [src/show_alloc_mem.c](src/show_alloc_mem.c): Visualizes the state of the heaps (required for 42).
