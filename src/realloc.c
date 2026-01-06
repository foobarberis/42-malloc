#include "malloc.h"

void * realloc( void * ptr, size_t size ) {
    /* realloc(NULL, size) -> malloc */
    if ( !ptr ) {
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "realloc \t\trealloc(NULL) called, allocating size=%zu\n", size );
        return malloc( size );
    }

    /* realloc(ptr, 0) -> free(ptr) and return NULL */
    if ( size == 0 ) {
        if ( LOGGING_ENABLED )
            f_dprintf( 2, "realloc \t\trealloc(0) called, freeing ptr=%p and returning NULL\n", ptr );
        free( ptr );
        return NULL;
    }

    size_t payload = align( size );
    if ( payload == 0 )
        return NULL;

    t_heap ** heap_lists[ 3 ] = { &g_heap_head.tiny, &g_heap_head.small, &g_heap_head.large };

    /* walk trough each heap type */
    for ( int i = 0; i < 3; i++ ) {
        t_heap * heap = *heap_lists[ i ];
        /* walk through each heap */
        while ( heap ) {
            /* check if the user pointer is within this heap */
            if ( ( void * )heap < ptr && ptr < ( void * )( ( char * )heap + heap->total_size ) ) {
                t_block * curr = heap->block_list;
                /* walk through the block list */
                while ( curr ) {
                    /* check if we found the block corresponding to the user pointer */
                    if ( ( void * )( ( char * )curr + sizeof( t_block ) ) == ptr ) {

                        /* invalid: double free or already free */
                        if ( curr->is_free ) {
                            if ( LOGGING_ENABLED )
                                f_dprintf( 2, "realloc \t\tdouble free / invalid ptr, returning NULL\n" );
                            return NULL;
                        }

                        size_t old_size = curr->size;

                        /* no size change */
                        if ( payload == old_size ) {
                            if ( LOGGING_ENABLED )
                                f_dprintf( 2, "realloc \t\tsame size requested ptr=%p size=%zu, returning original\n",
                                           ( void * )ptr, payload );
                            return ptr;
                        }

                        /* shrink */
                        if ( payload < old_size ) {
                            split_block( curr, payload, heap );

                            /* if the newly created free block can be coalesced with next, do it */
                            if ( curr->next && curr->next->next && curr->next->next->is_free ) {
                                /* coalesce curr->next with its next neighbor */
                                t_block * nb = curr->next;
                                coalesce_blocks( nb, nb->next, heap );
                            }

                            if ( LOGGING_ENABLED )
                                f_dprintf( 2, "realloc \t\tshrink split ptr=%p old=%zu new=%zu leftover=%zu\n",
                                           ( void * )ptr, old_size, payload,
                                           ( curr->next ? curr->next->size : 0 ) );
                            return ptr;
                        }

                        /* try in-place expand by consuming next free block */
                        if ( curr->next && curr->next->is_free ) {
                            t_block * next = curr->next;
                            if ( curr->size + sizeof( t_block ) + next->size >= payload ) {
                                /* consume next's payload from free_size */
                                heap->free_size -= next->size;
                                /* merge using utility to ensure logging */
                                coalesce_blocks( curr, next, NULL );

                                /* if too big, split leftover */
                                split_block( curr, payload, heap );

                                if ( LOGGING_ENABLED )
                                    f_dprintf( 2, "realloc \t\tinplace expanded ptr=%p new_size=%zu free_size=%zu\n",
                                               ( void * )ptr, curr->size, heap->free_size );
                                return ptr;
                            }
                        }

                        /* cannot expand in-place, allocate new block */
                        if ( LOGGING_ENABLED )
                            f_dprintf( 2, "realloc \t\tallocating new block ptr=%p old_size=%zu want=%zu\n",
                                       ( void * )ptr, old_size, payload );

                        void * new_ptr = malloc( size );
                        if ( !new_ptr ) {
                            if ( LOGGING_ENABLED )
                                f_dprintf( 2, "realloc \t\tfail allocate new_size=%zu ptr=%p\n",
                                           payload, ( void * )ptr );
                            return NULL;
                        }

                        /* copy the lesser of old and new sizes */
                        size_t copy_size = old_size < payload ? old_size : payload;
                        f_memcpy( new_ptr, ptr, copy_size );

                        if ( LOGGING_ENABLED )
                            f_dprintf( 2, "realloc \t\tmoved ptr=%p -> %p old_size=%zu new_size=%zu\n",
                                       ( void * )ptr, ( void * )new_ptr, old_size, payload );

                        free( ptr );
                        return new_ptr;
                    }
                    curr = curr->next;
                }
                if ( LOGGING_ENABLED )
                    f_dprintf( 2, "realloc \t\tpointer %p was not allocated, returning NULL\n", ptr );
                return NULL;
            }
            heap = heap->next;
        }
    }
    return NULL;
}
