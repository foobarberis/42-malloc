#include "malloc.h"

static void unmap_heap( t_heap * heap, t_heap ** heap_list_ptr ) {
    if ( heap->prev )
        heap->prev->next = heap->next;
    if ( heap->next )
        heap->next->prev = heap->prev;
    if ( *heap_list_ptr == heap )
        *heap_list_ptr = heap->next;
    munmap( heap, heap->total_size );
}

static int has_another_empty_heap( t_heap * head, t_heap * current ) {
    while ( head ) {
        if ( head != current && head->block_count == 0 )
            return 1;
        head = head->next;
    }
    return 0;
}

void free( void * ptr ) {
    /* free NULL pointer */
    if ( !ptr ) {
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "free \t\tfree(NULL) called, returning\n" );
        return;
    }

    t_heap ** heap_lists[ 3 ] = { &g_heap_head.tiny, &g_heap_head.small, &g_heap_head.large };

    /* walk through three heap lists (TINY, SMALL, LARGE) */
    for ( int i = 0; i < 3; i++ ) {
        t_heap * heap = *heap_lists[ i ];
        /* walk through heaps in the current list */
        while ( heap ) {

            /* check if user pointer is within the current heap address range */
            if ( ( void * )heap < ptr && ptr < ( void * )( ( char * )heap + heap->total_size ) ) {

                /* walk through blocks in the current heap */
                t_block * curr = heap->block_list;
                t_block * prev = NULL;
                while ( curr ) {

                    /* check if we have the right block */
                    if ( ( void * )( ( char * )curr + sizeof( t_block ) ) == ptr ) {

                        /* double free, abort */
                        if ( curr->is_free ) {
                            if ( LOGGING_ENABLED )
                                f_dprintf( 2, "free \t\tdouble free, returning\n" );
                            return;
                        }

                        /* mark block as free and update heap metadata */
                        curr->is_free = 1;
                        heap->block_count--;
                        heap->free_size += curr->size;
                        if ( LOGGING_ENABLED )
                            f_dprintf( 2, "free \t\tfreed ptr=%p payload=%zu free_size=%zu block_count=%zu\n",
                                       ptr, curr->size, heap->free_size, heap->block_count );

                        /* coalesce forward */
                        if ( curr->next && curr->next->is_free ) {
                            coalesce_blocks( curr, curr->next, heap );
                        }

                        /* coalesce backward */
                        if ( prev && prev->is_free ) {
                            coalesce_blocks( prev, curr, heap );
                        }

                        /* unmap heap if all blocks are free */
                        if ( heap->block_count == 0 ) {
                            /* if LARGE or if another empty heap of same type exists */
                            if ( i == 2 || has_another_empty_heap( *heap_lists[ i ], heap ) ) {
                                unmap_heap( heap, heap_lists[ i ] );
                                if ( LOGGING_ENABLED )
                                    f_dprintf( 2, "free \t\tunmapped empty heap %p\n", ( void * )heap );
                            } else if ( LOGGING_ENABLED ) {
                                f_dprintf( 2, "free \t\tkeeping empty heap %p as cache\n", ( void * )heap );
                            }
                        }
                        return;
                    }
                    prev = curr;
                    curr = curr->next;
                }
                /* pointer was not in range */
                if ( LOGGING_ENABLED )
                    f_dprintf( 2, "free \t\tpointer %p was not allocated, returning\n", ptr );
                return;
            }
            heap = heap->next;
        }
    }
    if ( LOGGING_ENABLED )
        f_dprintf( 2, "free \t\tpointer %p was not allocated using malloc, returning\n", ptr );
}
