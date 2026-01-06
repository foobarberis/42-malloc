#include "malloc.h"

static size_t print_zone( t_heap * heap, char * zone_name ) {
    size_t total_bytes = 0;

    while ( heap ) {
        f_printf( "%s : %p\n", zone_name, ( void * )heap );
        t_block * block = heap->block_list;
        while ( block ) {
            if ( !block->is_free ) {
                void * start_addr = ( void * )( ( char * )block + sizeof( t_block ) );
                void * end_addr = ( void * )( ( char * )start_addr + block->size );
                f_printf( "%p - %p : %zu bytes\n", start_addr, end_addr, block->size );
                total_bytes += block->size;
            }
            block = block->next;
        }
        heap = heap->next;
    }
    return total_bytes;
}

void show_alloc_mem( void ) {
    size_t total_bytes = 0;

    total_bytes += print_zone( g_heap_head.tiny, "TINY" );
    total_bytes += print_zone( g_heap_head.small, "SMALL" );
    total_bytes += print_zone( g_heap_head.large, "LARGE" );

    f_printf( "Total : %zu bytes\n", total_bytes );
}
