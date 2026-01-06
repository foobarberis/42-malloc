#include "malloc.h"

t_heap_handler g_heap_head = { NULL, NULL, NULL };

/* size: size of payload (aligned), zone_size: 4 * sysconf(_SC_PAGESIZE) or 26 * sysconf(_SC_PAGESIZE) or 0 */
static void * extend_heap( t_heap ** heap_head, size_t size, size_t zone_size ) {
    /* zone_size of 0 means LARGE allocation, needs its own heap + header */
    size_t total_size;
    if ( zone_size == 0 ) {
        total_size = size + sizeof( t_heap ) + sizeof( t_block );
    } else
        total_size = zone_size;

    /* request new mapping and handle MAP_FAILED */
    t_heap * new_heap = ( t_heap * )mmap( NULL, total_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0 );
    if ( new_heap == MAP_FAILED ) {
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "extend_heap \tmmap failed total_size=%zu zone_size=%zu\n", total_size, zone_size );
        return NULL;
    }

    if ( LOGGING_ENABLED )
        f_dprintf( 2, "extend_heap \tmmap=%p total_size=%zu\n", ( void * )new_heap, total_size );

    new_heap->total_size = total_size;
    new_heap->block_count = 1;
    new_heap->next = NULL;
    new_heap->prev = NULL;

    /* add the new heap to the list */
    if ( *heap_head == NULL ) {
        /* first heap of the list */
        *heap_head = new_heap;
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "extend_heap \theap_head was NULL, new_head=%p\n", ( void * )new_heap );
    } else {
        /* append to the end of the list */
        t_heap * last = *heap_head;
        while ( last->next != NULL )
            last = last->next;
        last->next = new_heap;
        new_heap->prev = last;
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "extend_heap \tadded heap, linked prev=%p\n", ( void * )new_heap->prev );
    }

    /* create the first big block in the new heap */
    new_heap->block_list = ( t_block * )( ( char * )new_heap + sizeof( t_heap ) );
    new_heap->block_list->size = total_size - sizeof( t_heap ) - sizeof( t_block );
    new_heap->block_list->next = NULL;
    new_heap->block_list->is_free = 1;

    if ( LOGGING_ENABLED )
        f_dprintf( 2, "extend_heap \tinit_block=%p size=%zu\n", ( void * )new_heap->block_list, new_heap->block_list->size );

    /* carve the user block out of the big free block */
    split_block( new_heap->block_list, size, NULL );
    new_heap->block_list->is_free = 0;
    new_heap->free_size = total_size - sizeof( t_heap ) - sizeof( t_block ) - new_heap->block_list->size;
    void * user_ptr = ( void * )( ( char * )new_heap->block_list + sizeof( t_block ) );

    if ( LOGGING_ENABLED )
        f_dprintf( 2, "extend_heap \treturn ptr=%p payload=%zu free_size=%zu block_count=%zu\n",
                   user_ptr, new_heap->block_list->size, new_heap->free_size, new_heap->block_count );

    /* return pointer to the payload area */
    return user_ptr;
}

void * malloc( size_t size ) {
    /* malloc(0), abort */
    if ( size == 0 ) {
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "malloc \t\tmalloc(0) called, returning NULL\n" );
        return NULL;
    }

    /* align requested size up to nearest multiple of ALIGNMENT */
    size_t payload = align( size );
    if ( payload == 0 ) {
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "malloc \t\talign overflow for size=%zu, returning NULL\n", size );
        return NULL;
    }

    /* determine if it is a TINY, SMALL or LARGE allocation */
    size_t zone_size;
    t_heap ** heap_head;
    if ( payload <= TINY_ALLOC_SIZE ) {
        heap_head = &g_heap_head.tiny;
        zone_size = tiny_max_size();
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "malloc \t\tsize=%zu aligned to %zu (TINY)\n", size, payload );
    } else if ( payload <= SMALL_ALLOC_SIZE ) {
        heap_head = &g_heap_head.small;
        zone_size = small_max_size();
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "malloc \t\tsize=%zu aligned to %zu (SMALL)\n", size, payload );
    } else {
        /* check for potential overflow when adding heap/block headers for LARGE zones */
        if ( payload > ( size_t )-1 - sizeof( t_heap ) - sizeof( t_block ) ) {
            if ( LOGGING_ENABLED )
                f_dprintf( 2, "malloc \t\tLARGE header overflow for size=%zu, returning NULL\n", size );
            return NULL;
        }
        heap_head = &g_heap_head.large;
        zone_size = 0;
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "malloc \t\tsize=%zu aligned to %zu (LARGE)\n", size, payload );
    }

    /* walk through heaps */
    t_heap * heap = *heap_head;
    while ( heap ) {
        /* found a heap with enough free space */
        if ( heap->free_size >= payload ) {
            t_block * b = heap->block_list;
            /* walk through the block list */
            while ( b ) {
                /* found a free block with enough size, split it */
                if ( b->is_free && b->size >= payload ) {
                    if ( LOGGING_ENABLED )
                        f_dprintf( 2, "malloc \t\tfound free block=%p size=%zu\n", ( void * )b, b->size );
                    size_t old_free = b->size;
                    split_block( b, payload, heap );
                    b->is_free = 0;
                    heap->block_count++;
                    heap->free_size -= old_free;
                    /* return pointer to the payload, right after the header */
                    return ( void * )( ( char * )b + sizeof( t_block ) );
                }
                b = b->next;
            }
        }
        heap = heap->next;
    }

    /* no suitable heap found, create new heap */
    if ( LOGGING_ENABLED ) {
        f_dprintf( 2, "malloc \t\tno suitable heap found, extending heap\n" );
        f_dprintf( 2, "malloc \t\theap_head=%p payload=%zu zone_size=%zu\n", ( void * )*heap_head, payload, zone_size );
    }
    return extend_heap( heap_head, payload, zone_size );
}
