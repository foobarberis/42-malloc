#ifndef MALLOC_H
#define MALLOC_H

#include "libft.h"    /* f_printf */
#include <stddef.h>   /* size_t */
#include <sys/mman.h> /* mmap, munmap */
#include <unistd.h>   /* sysconf */

#define LOGGING_ENABLED 0

/* align to 16 bytes */
#define ALIGNMENT 16
/* maximum size for tiny allocations */
#define TINY_ALLOC_SIZE 128
/* maximum size for small allocations */
#define SMALL_ALLOC_SIZE 1024

void * malloc( size_t size );
void free( void * ptr );
void * realloc( void * ptr, size_t size );
void show_alloc_mem( void );

/* using static variable to avoid repeated calls to sysconf */
static inline size_t tiny_max_size( void ) {
    static size_t v = 0;
    if ( v == 0 )
        v = 4 * sysconf( _SC_PAGESIZE );
    return v;
}

static inline size_t small_max_size( void ) {
    static size_t v = 0;
    if ( v == 0 )
        v = 26 * sysconf( _SC_PAGESIZE );
    return v;
}

/* align 'size' up to the nearest multiple of ALIGNMENT. Returns 0 on overflow. */
static inline size_t align( size_t size ) {
    if ( size > ( size_t )-1 - ( ALIGNMENT - 1 ) )
        return 0;
    return ( size + ( ALIGNMENT - 1 ) ) & ~( ( size_t )ALIGNMENT - 1 );
}

/* structure to hold block metadata */
typedef struct s_block {
    size_t size;           /* size of the data payload (user capacity) */
    struct s_block * next; /* next block in this specific zone */
    int is_free;           /* 1 if free, 0 if allocated */
} __attribute__( ( aligned( 16 ) ) ) t_block;

/* structure to hold heap metadata */
typedef struct s_heap {
    struct s_heap * prev; /* double linked list to manage zones */
    struct s_heap * next;
    size_t total_size;    /* total size obtained from mmap */
    size_t free_size;     /* remaining space (optimization) */
    size_t block_count;   /* number of allocations (used for cleanup) */
    t_block * block_list; /* pointer to the first block inside this zone */
} __attribute__( ( aligned( 16 ) ) ) t_heap;

/* structure to manage our heaps */
typedef struct s_heap_handler {
    t_heap * tiny;
    t_heap * small;
    t_heap * large;
} t_heap_handler;

extern t_heap_handler g_heap_head;

void split_block( t_block * b, size_t size, t_heap * heap );
void coalesce_blocks( t_block * head, t_block * next, t_heap * heap );

#endif
