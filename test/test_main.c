#include "malloc.h"
#include <string.h>

#define M ( 1024 * 1024 )

typedef struct s_stats {
    size_t tiny_alloc;
    size_t small_alloc;
    size_t large_alloc;
    size_t tiny_heaps;
    size_t small_heaps;
    size_t large_heaps;
} t_stats;

static t_stats get_stats( void ) {
    t_stats stats = { 0, 0, 0, 0, 0, 0 };
    t_heap * h;
    t_block * b;

    /* Tiny */
    h = g_heap_head.tiny;
    while ( h ) {
        stats.tiny_heaps++;
        b = h->block_list;
        while ( b ) {
            if ( !b->is_free )
                stats.tiny_alloc += b->size;
            b = b->next;
        }
        h = h->next;
    }

    /* Small */
    h = g_heap_head.small;
    while ( h ) {
        stats.small_heaps++;
        b = h->block_list;
        while ( b ) {
            if ( !b->is_free )
                stats.small_alloc += b->size;
            b = b->next;
        }
        h = h->next;
    }

    /* Large */
    h = g_heap_head.large;
    while ( h ) {
        stats.large_heaps++;
        b = h->block_list;
        while ( b ) {
            if ( !b->is_free )
                stats.large_alloc += b->size;
            b = b->next;
        }
        h = h->next;
    }
    return stats;
}

static int is_ptr_in_zone( void * ptr, t_heap * head ) {
    t_heap * h = head;
    while ( h ) {
        t_block * b = h->block_list;
        while ( b ) {
            if ( ( void * )b + sizeof( t_block ) == ptr )
                return 1;
            b = b->next;
        }
        h = h->next;
    }
    return 0;
}

static void print_test_header( char * name ) {
    {
        f_dprintf( 2, "\n\n========================================\n" );
        f_dprintf( 2, "TEST: %s\n", name );
        f_dprintf( 2, "========================================\n" );
    }
}

int main( void ) {
    char * ptr;
    int    fail_count = 0;

    /* ---------------------------------------------------------------------- */
    /* 1. Basic Tests (From original test.c)                                  */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Basic: NULL & Zero Size" );
    {
        t_stats before = get_stats();
        f_dprintf( 2, "-> free(NULL)\n" );
        f_dprintf( 2, "   Expected: Stats remain identical\n" );
        free( NULL );
        t_stats after = get_stats();
        if ( f_memcmp( &before, &after, sizeof( t_stats ) ) == 0 )
            f_dprintf( 2, "   [OK] Stats unchanged.\n" );
        else {
            f_dprintf( 2, "   [FAIL] Stats changed after free(NULL)\n" );
            fail_count++;
        }

        f_dprintf( 2, "-> malloc(0)\n" );
        f_dprintf( 2, "   Expected: returns NULL, stats unchanged\n" );
        void * zero = malloc( 0 );
        after = get_stats();
        if ( zero == NULL && f_memcmp( &before, &after, sizeof( t_stats ) ) == 0 )
            f_dprintf( 2, "   [OK] Malloc(0) returned NULL, stats unchanged.\n" );
        else {
            f_dprintf( 2, "   [FAIL] Malloc(0) error.\n" );
            fail_count++;
        }
    }

    print_test_header( "Basic: Double Free" );
    {
        f_dprintf( 2, "-> malloc(1), free, free\n" );
        f_dprintf( 2, "   Expected: Second free should return without crashing\n" );
        ptr = ( char * )malloc( 1 );
        ptr[ 0 ] = 'a';

        free( ptr );
        t_stats mid = get_stats();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
        free( ptr ); /* Should detect double free if implemented, or at least not crash */
#pragma GCC diagnostic pop
        t_stats after = get_stats();

        if ( f_memcmp( &mid, &after, sizeof( t_stats ) ) == 0 )
            f_dprintf( 2, "   [OK] Statistically stable after double free.\n" );
        else {
            f_dprintf( 2, "   [FAIL] Stats changed after invalid second free.\n" );
            fail_count++;
        }
    }

    print_test_header( "Basic: Invalid Pointer Free" );
    {
        f_dprintf( 2, "-> free(0x1)\n" );
        f_dprintf( 2, "   Expected: ignored, stats unchanged\n" );
        t_stats before = get_stats();
        char * random_ptr = ( char * )0x1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
        free( random_ptr );
#pragma GCC diagnostic pop
        t_stats after = get_stats();
        if ( f_memcmp( &before, &after, sizeof( t_stats ) ) == 0 )
            f_dprintf( 2, "   [OK] Invalid free ignored.\n" );
        else {
            f_dprintf( 2, "   [FAIL] Invalid free modified state.\n" );
            fail_count++;
        }
    }

    print_test_header( "Basic: Alloc/Free Sizes" );
    {
        size_t sizes[] = { 1, 128, 1024, 2048 };
        for ( int i = 0; i < 4; i++ ) {
            f_dprintf( 2, "-> malloc(%zu) then free\n", sizes[ i ] );
            t_stats before = get_stats();
            ptr = ( char * )malloc( sizes[ i ] );
            if ( ptr ) {
                ptr[ 0 ] = 'a';
                t_stats mid = get_stats();
                if ( mid.tiny_alloc + mid.small_alloc + mid.large_alloc > before.tiny_alloc + before.small_alloc + before.large_alloc ) {
                    f_dprintf( 2, "   [OK] Memory increased.\n" );
                }
                free( ptr );
                t_stats after = get_stats();
                if ( after.tiny_alloc + after.small_alloc + after.large_alloc == before.tiny_alloc + before.small_alloc + before.large_alloc ) {
                    f_dprintf( 2, "   [OK] Allocated size returned to previous state.\n" );
                }
            } else {
                f_dprintf( 2, "   [FAIL] Malloc failed for size %zu\n", sizes[ i ] );
                fail_count++;
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 2. Error Handling (from test_error_handling.c)                         */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Eval: Error Handling (Offset Pointers)" );
    {
        char * addr = ( char * )malloc( 1024 );
        if ( addr ) {
            addr[ 0 ] = 42;
            t_stats before = get_stats();
            f_dprintf( 2, "-> free(ptr + 5)\n" );
            f_dprintf( 2, "   Expected: ignored, stats unchanged\n" );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
            free( addr + 5 ); /* Should handle gracefully */
#pragma GCC diagnostic pop
            t_stats after = get_stats();
            if ( f_memcmp( &before, &after, sizeof( t_stats ) ) == 0 )
                f_dprintf( 2, "   [OK] Offset free ignored.\n" );
            else {
                f_dprintf( 2, "   [FAIL] Offset free modified state.\n" );
                fail_count++;
            }

            f_dprintf( 2, "-> realloc(ptr + 5)\n" );
            f_dprintf( 2, "   Expected: returns NULL, stats unchanged\n" );
            before = get_stats();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
            void * res = realloc( addr + 5, 10 );
#pragma GCC diagnostic pop
            after = get_stats();
            if ( res == NULL && f_memcmp( &before, &after, sizeof( t_stats ) ) == 0 )
                f_dprintf( 2, "   [OK] Offset realloc returned NULL, stats unchanged.\n" );
            else {
                f_dprintf( 2, "   [FAIL] Offset realloc error.\n" );
                fail_count++;
            }

            free( addr );
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 3. Realloc Expansion (from test_realloc1.c)                            */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Eval: Realloc Simple Expansion (16M -> 128M)" );
    {
        /* Case: Large allocation + realloc to even larger */
        char * p1 = malloc( 16 * M );
        if ( p1 ) {
            strcpy( p1, "This is very important data." );
            f_dprintf( 2, "-> Allocated 16M, reallocating to 128M...\n" );

            char * p2 = realloc( p1, 128 * M );
            if ( p2 ) {
                f_dprintf( 2, "-> Realloc success. Checking data integrity...\n" );
                if ( strcmp( p2, "This is very important data." ) == 0 ) {
                    f_dprintf( 2, "   [OK] Data preserved.\n" );
                } else {
                    f_dprintf( 2, "   [FAIL] Data corrupted.\n" );
                    fail_count++;
                }
                /* Verify access at end of new block */
                p2[ ( 128 * M ) - 1 ] = 42;
                free( p2 );
            } else {
                f_dprintf( 2, "   [FAIL] Realloc returned NULL\n" );
                fail_count++;
                free( p1 );
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 4. Realloc Obstruction (from test_realloc2.c)                          */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Eval: Realloc With Obstruction" );
    {
        /* Case: blocked expansion requiring move */
        char * p1 = malloc( 16 * M );
        char * p2 = malloc( 16 * M ); /* Blocks p1 from simply extending if contiguous */

        if ( p1 && p2 ) {
            strcpy( p1, "Data that needs to move" );
            f_dprintf( 2, "-> Allocated p1(16M), p2(16M). Reallocating p1 -> 128M...\n" );

            char * p3 = realloc( p1, 128 * M );
            if ( p3 ) {
                if ( p3 == p1 ) {
                    f_dprintf( 2, "  [INFO] Expanded in place (unexpected if allocator is compact)\n" );
                } else {
                    f_dprintf( 2, "  [INFO] Moved to new location (expected)\n" );
                }

                if ( strcmp( p3, "Data that needs to move" ) == 0 ) {
                    f_dprintf( 2, "   [OK] Data preserved.\n" );
                } else {
                    f_dprintf( 2, "   [FAIL] Data corrupted.\n" );
                    fail_count++;
                }
                free( p3 );
            } else {
                f_dprintf( 2, "   [FAIL] Realloc returned NULL\n" );
                fail_count++;
                free( p1 );
            }
            free( p2 );
        } else {
            if ( p1 )
                free( p1 );
            if ( p2 )
                free( p2 );
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 5. Show Alloc Mem (from test_show.c logic)                             */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Eval: Programmatic Zone Check (Mixed Sizes)" );
    {
        /* Create allocations in different zones (TINY, SMALL, LARGE) */
        void * t1 = malloc( 12 );
        void * s1 = malloc( 512 );
        void * l1 = malloc( 2 * M );
        void * t2 = malloc( 42 );
        void * s2 = malloc( 1024 );

        f_dprintf( 2, "-> Allocated mixed sizes. Verifying zone placement:\n" );
        f_dprintf( 2, "   Expected: 12B/42B in TINY, 512B/1024B in SMALL, 2MB in LARGE\n" );

        if ( is_ptr_in_zone( t1, g_heap_head.tiny ) ) f_dprintf( 2, "   [OK] t1 (12B) is in TINY heap\n" );
        else { f_dprintf( 2, "   [FAIL] t1 (12B) NOT in TINY heap\n" ); fail_count++; }

        if ( is_ptr_in_zone( t2, g_heap_head.tiny ) ) f_dprintf( 2, "   [OK] t2 (42B) is in TINY heap\n" );
        else { f_dprintf( 2, "   [FAIL] t2 (42B) NOT in TINY heap\n" ); fail_count++; }

        if ( is_ptr_in_zone( s1, g_heap_head.small ) ) f_dprintf( 2, "   [OK] s1 (512B) is in SMALL heap\n" );
        else { f_dprintf( 2, "   [FAIL] s1 (512B) NOT in SMALL heap\n" ); fail_count++; }

        if ( is_ptr_in_zone( s2, g_heap_head.small ) ) f_dprintf( 2, "   [OK] s2 (1024B) is in SMALL heap\n" );
        else { f_dprintf( 2, "   [FAIL] s2 (1024B) NOT in SMALL heap\n" ); fail_count++; }

        if ( is_ptr_in_zone( l1, g_heap_head.large ) ) f_dprintf( 2, "   [OK] l1 (2MB) is in LARGE heap\n" );
        else { f_dprintf( 2, "   [FAIL] l1 (2MB) NOT in LARGE heap\n" ); fail_count++; }

        show_alloc_mem(); /* Keeping this one as it's the specific show_alloc_mem test area */

        free( t1 );
        free( l1 );
        free( s2 );
        free( s1 );
        free( t2 );
    }

    /* ---------------------------------------------------------------------- */
    /* 6. Stress Test: Alloc/Free Loop (from test_free.c)                     */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Eval: Stress Loop (Alloc 1024 -> Free)" );
    {
        int count = 1000;
        t_stats before = get_stats();
        f_dprintf( 2, "-> Loop %d times: malloc(1024), write, free\n", count );
        f_dprintf( 2, "   Expected: tiny_alloc, small_alloc, large_alloc same as start\n" );
        for ( int i = 0; i < count; i++ ) {
            ptr = malloc( 1024 );
            if ( ptr ) {
                ptr[ 0 ] = 42;
                free( ptr );
            }
        }
        t_stats after = get_stats();
        if ( after.tiny_alloc == before.tiny_alloc && after.small_alloc == before.small_alloc && after.large_alloc == before.large_alloc )
            f_dprintf( 2, "   [OK] No leaked bytes after loop.\n" );
        else {
            f_dprintf( 2, "   [FAIL] Leak detected after loop: Start(%zu, %zu, %zu) -> End(%zu, %zu, %zu)\n", before.tiny_alloc, before.small_alloc, before.large_alloc, after.tiny_alloc, after.small_alloc, after.large_alloc );
            fail_count++;
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 7. Stress Test: Leak Loop (from test_malloc.c)                         */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Eval: Leak Loop (Alloc 1024 * 1024 times)" );
    {
        int count = 1024;
        t_stats before = get_stats();
        f_dprintf( 2, "-> Loop %d times: malloc(1024) WITHOUT free\n", count );
        f_dprintf( 2, "   Expected: small_alloc exactly %d bytes higher\n", count * ( int )align( 1024 ) );
        for ( int i = 0; i < count; i++ ) {
            ptr = malloc( 1024 );
            if ( ptr ) {
                ptr[ 0 ] = 42;
            }
        }
        t_stats after = get_stats();

        /* Note: malloc(1024) aligns to exactly 1024 or 1040? 1024 is already aligned to 16. */
        size_t expected_increase = ( size_t )count * align( 1024 );
        if ( after.small_alloc >= before.small_alloc + expected_increase )
            f_dprintf( 2, "   [OK] Allocated %zu bytes in SMALL zone.\n", after.small_alloc - before.small_alloc );
        else {
            f_dprintf( 2, "   [FAIL] Allocation mismatch: got %zu, expected at least %zu\n", after.small_alloc - before.small_alloc, expected_increase );
            fail_count++;
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 8. Alignment Verification                                              */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Extended: 16-Byte Alignment Check" );
    {
        f_dprintf( 2, "-> Checking alignment for sizes [1...2048]\n" );
        int all_aligned = 1;
        for ( size_t s = 1; s <= 2048; s++ ) {
            void * p = malloc( s );
            if ( p ) {
                if ( ( ( uintptr_t )p % 16 ) != 0 ) {
                    f_dprintf( 2, "   [FAIL] Size %zu returned non-aligned pointer %p\n", s, p );
                    all_aligned = 0;
                    fail_count++;
                }
                free( p );
            }
        }
        if ( all_aligned ) {
            f_dprintf( 2, "   [OK] All pointers 16-byte aligned.\n" );
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 9. Zone Boundaries                                                     */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Extended: Zone Boundaries" );
    {
        size_t b_sizes[] = { 128, 129, 1024, 1025 };
        char * b_names[] = { "TINY (max)", "SMALL (min)", "SMALL (max)", "LARGE (min)" };

        for ( int i = 0; i < 4; i++ ) {
            f_dprintf( 2, "-> Allocating %zu bytes (%s)\n", b_sizes[ i ], b_names[ i ] );
            ptr = malloc( b_sizes[ i ] );
            if ( ptr ) {
                if ( i == 0 && is_ptr_in_zone( ptr, g_heap_head.tiny ) ) f_dprintf( 2, "   [OK] 128B in TINY\n" );
                else if ( i == 1 && is_ptr_in_zone( ptr, g_heap_head.small ) ) f_dprintf( 2, "   [OK] 129B in SMALL\n" );
                else if ( i == 2 && is_ptr_in_zone( ptr, g_heap_head.small ) ) f_dprintf( 2, "   [OK] 1024B in SMALL\n" );
                else if ( i == 3 && is_ptr_in_zone( ptr, g_heap_head.large ) ) f_dprintf( 2, "   [OK] 1025B in LARGE\n" );
                else {
                    f_dprintf( 2, "   [FAIL] Boundary mismatch for size %zu\n", b_sizes[ i ] );
                    fail_count++;
                }
                free( ptr );
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 10. Realloc: In-place & Shrink                                         */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Extended: Realloc Optimized" );
    {
        f_dprintf( 2, "-> Testing In-place Expansion\n" );
        void * a = malloc( 64 );
        void * b = malloc( 64 ); /* Blocks 'a' from extending if it were at end of heap */
        void * c = malloc( 64 );
        free( b ); /* Create a hole after 'a' */

        void * a2 = realloc( a, 128 );
        if ( a2 == a ) {
            f_dprintf( 2, "   [OK] Realloc expanded in-place.\n" );
        } else {
            f_dprintf( 2, "   [INFO] Realloc moved pointer (may happen if 'b' wasn't enough space or alignment constraints).\n" );
        }

        f_dprintf( 2, "-> Testing Shrink & Split\n" );
        void * d = malloc( 512 );
        void * d2 = realloc( d, 64 );
        if ( d2 == d ) {
            f_dprintf( 2, "   [OK] Realloc shrunk in-place.\n" );
        }

        free( a2 );
        free( c );
        free( d2 );
    }

    /* ---------------------------------------------------------------------- */
    /* 11. Coalescing: Triple Merge                                           */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Extended: Triple Coalesce" );
    {
        /* We use small sizes to stay within the TINY zone even after coalesce */
        /* Size calculation: 3 chunks of 16 bytes + 2 headers of 32 bytes each = 112 bytes */
        /* TINY_ALLOC_SIZE is 128, so 112 stays in TINY. */
        f_dprintf( 2, "-> Allocating A, B, C (16 bytes each)\n" );
        void * a = malloc( 16 );
        void * b = malloc( 16 );
        void * c = malloc( 16 );

        if ( a && b && c ) {
            f_dprintf( 2, "   Pointers: A=%p, B=%p, C=%p\n", a, b, c );

            f_dprintf( 2, "-> Freeing A and C (Internal fragments created)\n" );
            free( a );
            free( c );

            f_dprintf( 2, "-> Freeing B (Should trigger triple coalesce)\n" );
            free( b );

            f_dprintf( 2, "-> Allocating D (112 bytes)\n" );
            /* 3 * (16 + sizeof(t_block)) - sizeof(t_block) = 3 * 48 - 32 = 144 - 32 = 112 */
            void * d = malloc( 112 );

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
            if ( d == a ) {
                f_dprintf( 2, "   [OK] Triple coalesce verified: D began at A's address (%p)\n", d );
            } else {
                f_dprintf( 2, "   [FAIL] Triple coalesce failed or was obstructed: D=%p, expected A=%p\n", d, a );
                fail_count++;
            }
#pragma GCC diagnostic pop
            if ( d )
                free( d );
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 12. Huge Allocation (Trigger mmap failure)                             */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Extended: Huge Allocation (mmap failure)" );
    {
        /* Attempt to allocate something so large it must fail */
        size_t huge = ( size_t )-1 / 2;
        f_dprintf( 2, "-> Attempting malloc(%zu)...\n", huge );
        void * fail_ptr = malloc( huge );
        if ( fail_ptr == NULL ) {
            f_dprintf( 2, "   [OK] Malloc returned NULL as expected.\n" );
        } else {
            f_dprintf( 2, "   [FAIL] Malloc somehow succeeded? %p\n", fail_ptr );
            fail_count++;
            free( fail_ptr );
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 13. Overflow Protection                                                */
    /* ---------------------------------------------------------------------- */
    print_test_header( "Extended: Overflow Protection" );
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuse-after-free"
#pragma GCC diagnostic ignored "-Walloc-size-larger-than="
    {
        f_dprintf( 2, "-> malloc(SIZE_MAX)\n" );
        void * p = malloc( ( size_t )-1 );
        if ( p == NULL )
            f_dprintf( 2, "   [OK] malloc(SIZE_MAX) returned NULL.\n" );
        else {
            f_dprintf( 2, "   [FAIL] malloc(SIZE_MAX) succeeded.\n" );
            fail_count++;
            free( p );
        }

        f_dprintf( 2, "-> malloc(SIZE_MAX - 5)\n" );
        p = malloc( ( size_t )-1 - 5 );
        if ( p == NULL )
            f_dprintf( 2, "   [OK] malloc(SIZE_MAX - 5) returned NULL.\n" );
        else {
            f_dprintf( 2, "   [FAIL] malloc(SIZE_MAX - 5) succeeded.\n" );
            fail_count++;
            free( p );
        }

        f_dprintf( 2, "-> malloc((size_t)-100)\n" );
        p = malloc( ( size_t )-100 );
        if ( p == NULL )
            f_dprintf( 2, "   [OK] malloc((size_t)-100) returned NULL.\n" );
        else {
            f_dprintf( 2, "   [FAIL] malloc((size_t)-100) succeeded.\n" );
            fail_count++;
            free( p );
        }

        f_dprintf( 2, "-> realloc(ptr, SIZE_MAX)\n" );
        void * p2 = malloc( 16 );
        if ( p2 ) {
            void * p3 = realloc( p2, ( size_t )-1 );
            if ( p3 == NULL )
                f_dprintf( 2, "   [OK] realloc(ptr, SIZE_MAX) returned NULL.\n" );
            else {
                f_dprintf( 2, "   [FAIL] realloc(ptr, SIZE_MAX) succeeded.\n" );
                fail_count++;
                free( p3 );
            }

            f_dprintf( 2, "-> realloc(ptr, (size_t)-100)\n" );
            p3 = realloc( p2, ( size_t )-100 );
            if ( p3 == NULL )
                f_dprintf( 2, "   [OK] realloc(ptr, (size_t)-100) returned NULL.\n" );
            else {
                f_dprintf( 2, "   [FAIL] realloc(ptr, (size_t)-100) succeeded.\n" );
                fail_count++;
                free( p3 );
            }
            /* note: if realloc failed, p2 is still valid but we don't need it */
            free( p2 );
        }
    }
#pragma GCC diagnostic pop

    if ( fail_count == 0 )
        f_dprintf( 2, "\n[SUCCESS] All tests passed!\n" );
    else
        f_dprintf( 2, "\n[FAILURE] %d test(s) failed.\n", fail_count );

    return ( fail_count > 0 );
}
