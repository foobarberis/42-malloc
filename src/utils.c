#include "malloc.h"

void split_block( t_block * b, size_t size, t_heap * heap ) {
    /* need ALIGNMENT here to make sure the leftover block is at least ALIGNMENT bytes */
    if ( b->size >= size + sizeof( t_block ) + ALIGNMENT ) {
        if ( LOGGING_ENABLED ) {
            f_dprintf( 2, "split_block \tbefore=%zu payload=%zu metadata=%zu leftover=%zu\n",
                       b->size,
                       size,
                       sizeof( t_block ),
                       ( b->size - size - sizeof( t_block ) ) );
        }

        t_block * new_b = ( t_block * )( ( char * )b + sizeof( t_block ) + size );
        new_b->size = b->size - size - sizeof( t_block );
        new_b->next = b->next;
        new_b->is_free = 1;
        b->size = size;
        b->next = new_b;

        /* when a heap pointer is provided, update its free_size to account for the newly created free payload */
        if ( heap )
            heap->free_size += new_b->size;
    }
}

void coalesce_blocks( t_block * head, t_block * next, t_heap * heap ) {
    if ( LOGGING_ENABLED ) {
        f_dprintf( 2, "coalesce \thead=%p next=%p new_size=%zu\n", ( void * )head, ( void * )next, head->size + next->size + sizeof( t_block ) );
    }
    head->size += next->size + sizeof( t_block );
    head->next = next->next;
    if ( heap )
        heap->free_size += sizeof( t_block );
}
