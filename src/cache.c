#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"

int g_mode = 0;

 /*  Init / Destroy / Clear*/


void cache_init(Cache *c, int ways, const char *name)
{
    c->ways = ways;
    c->name = name;
    memset(&c->stats, 0, sizeof(Stats));

    c->sets = (CacheSet *)calloc(NUM_SETS, sizeof(CacheSet));
    if (!c->sets) {
        fprintf(stderr, "Error: out of memory allocating cache sets\n");
        exit(1);
    }

    /* Initialise LRU order: way 0 = LRU, way ways-1 = MRU */
    for (int i = 0; i < NUM_SETS; i++) {
        for (int w = 0; w < ways; w++) {
            c->sets[i].lru_order[w] = w;
            c->sets[i].lines[w].state = INVALID;
        }
    }
}

void cache_destroy(Cache *c)
{
    free(c->sets);
    c->sets = NULL;
}

void cache_clear(Cache *c)
{
    for (int i = 0; i < NUM_SETS; i++) {
        for (int w = 0; w < c->ways; w++) {
            c->sets[i].lines[w].state = INVALID;
            c->sets[i].lines[w].tag   = 0;
            c->sets[i].lru_order[w]   = w;
        }
    }
    memset(&c->stats, 0, sizeof(Stats));
}

/*
 *  LRU Management
 *
 *  lru_order[0]       = LRU (evict first)
 *  lru_order[ways-1]  = MRU (most recently used)
*/

void cache_update_lru(Cache *c, uint32_t index, int way)
{
    CacheSet *set = &c->sets[index];
    int ways = c->ways;

    /* Find current position of 'way' in the order array */
    int pos = -1;
    for (int i = 0; i < ways; i++) {
        if (set->lru_order[i] == way) { pos = i; break; }
    }
    if (pos < 0) return; /* shouldn't happen */

    /* Shift everything below pos one position to the left */
    for (int i = pos; i < ways - 1; i++) {
        set->lru_order[i] = set->lru_order[i + 1];
    }
    /* Place 'way' at MRU position */
    set->lru_order[ways - 1] = way;
}

int cache_lru_way(Cache *c, uint32_t index)
{
    /* First prefer an INVALID slot (free way) to avoid unnecessary eviction */
    CacheSet *set = &c->sets[index];
    for (int i = 0; i < c->ways; i++) {
        int w = set->lru_order[i];
        if (set->lines[w].state == INVALID) return w;
    }
    /* All ways occupied return true LRU */
    return set->lru_order[0];
}

/* 
 *  Lookup / Install / Evict
*/

int cache_lookup(Cache *c, uint32_t index, uint32_t tag)
{
    CacheSet *set = &c->sets[index];
    for (int w = 0; w < c->ways; w++) {
        if (set->lines[w].state != INVALID && set->lines[w].tag == tag) {
            return w;
        }
    }
    return -1;
}

void cache_evict_lru(Cache *c, uint32_t index,
                     uint32_t *evicted_tag, MESIState *evicted_state)
{
    int way = cache_lru_way(c, index);
    CacheSet *set = &c->sets[index];
    *evicted_tag   = set->lines[way].tag;
    *evicted_state = set->lines[way].state;
    set->lines[way].state = INVALID;
}

void cache_install(Cache *c, uint32_t index, uint32_t tag, MESIState state)
{
    int way = cache_lru_way(c, index);
    CacheSet *set = &c->sets[index];
    set->lines[way].tag   = tag;
    set->lines[way].state = state;
    cache_update_lru(c, index, way);
}

void cache_invalidate_way(Cache *c, uint32_t index, int way)
{
    c->sets[index].lines[way].state = INVALID;
}

/* 
 *  Reporting
  */

void cache_print_stats(const Cache *c)
{
    double ratio = 0.0;
    unsigned long accesses = c->stats.hits + c->stats.misses;
    if (accesses > 0) ratio = (double)c->stats.hits / (double)accesses;

    printf("\n=== %s Cache Statistics ===\n", c->name);
    printf("  Reads:     %lu\n", c->stats.reads);
    printf("  Writes:    %lu\n", c->stats.writes);
    printf("  Hits:      %lu\n", c->stats.hits);
    printf("  Misses:    %lu\n", c->stats.misses);
    printf("  Hit Ratio: %.4f (%.2f%%)\n", ratio, ratio * 100.0);
}

void cache_print_contents(const Cache *c)
{
    printf("\n--- %s Cache Contents (valid lines only) ---\n", c->name);
    int printed = 0;
    for (int i = 0; i < NUM_SETS; i++) {
        CacheSet *set = &c->sets[i];
        for (int w = 0; w < c->ways; w++) {
            if (set->lines[w].state != INVALID) {
                /* Determine LRU rank for this way (0=LRU, ways-1=MRU) */
                int lru_rank = -1;
                for (int r = 0; r < c->ways; r++) {
                    if (set->lru_order[r] == w) { lru_rank = r; break; }
                }
                printf("  Set: %5d  Way: %d  Tag: 0x%03X  State: %c  LRU-rank: %d\n",
                       i, w, set->lines[w].tag,
                       mesi_char(set->lines[w].state),
                       lru_rank);
                printed++;
            }
        }
    }
    if (printed == 0) printf("  (empty)\n");
    printf("  Total valid lines: %d\n", printed);
}

/* 
 *  Helper: reconstruct line address from tag+index
 *  (returns the base address of the cache line)
 */
static uint32_t make_line_addr(uint32_t tag, uint32_t index)
{
    return (tag << (LINE_BITS + SET_BITS)) | (index << LINE_BITS);
}

/* 
 *  Instruction Cache Operations
*/

void icache_read(Cache *ic, uint32_t addr)
{
    uint32_t index = ADDR_INDEX(addr);
    uint32_t tag   = ADDR_TAG(addr);

    ic->stats.reads++;

    int way = cache_lookup(ic, index, tag);
    if (way >= 0) {
        /* Hit */
        ic->stats.hits++;
        cache_update_lru(ic, index, way);
    } else {
        /* Miss */
        ic->stats.misses++;

        /* Evict LRU if needed */
        uint32_t ev_tag;
        MESIState ev_state;
        cache_evict_lru(ic, index, &ev_tag, &ev_state);
        /* Instruction lines are never Modified, so no write-back needed */

        /* Fetch from L2 */
        if (g_mode >= 1) {
            printf("Read from L2 0x%X\n", addr & ~(LINE_SIZE - 1));
        }

        /* Install as Exclusive */
        cache_install(ic, index, tag, EXCLUSIVE);
    }
}

void icache_invalidate(Cache *ic, uint32_t addr)
{
    uint32_t index = ADDR_INDEX(addr);
    uint32_t tag   = ADDR_TAG(addr);

    int way = cache_lookup(ic, index, tag);
    if (way >= 0) {
        cache_invalidate_way(ic, index, way);
    }
}

void icache_snoop(Cache *ic, uint32_t addr)
{
    /* Instruction lines are never Modified, so nothing to return */
    (void)ic; (void)addr;
}

/* 
 *  Data Cache Operations
*/

void dcache_read(Cache *dc, uint32_t addr)
{
    uint32_t index = ADDR_INDEX(addr);
    uint32_t tag   = ADDR_TAG(addr);

    dc->stats.reads++;

    int way = cache_lookup(dc, index, tag);
    if (way >= 0) {
        /* Hit any MESI state (M/E/S) is a valid read hit */
        dc->stats.hits++;
        cache_update_lru(dc, index, way);
    } else {
        /* Miss */
        dc->stats.misses++;

        /* Evict LRU */
        uint32_t ev_tag;
        MESIState ev_state;
        cache_evict_lru(dc, index, &ev_tag, &ev_state);

        /* Write back dirty evicted line */
        if (ev_state == MODIFIED) {
            uint32_t ev_addr = make_line_addr(ev_tag, index);
            if (g_mode >= 1) {
                printf("Write to L2 0x%X\n", ev_addr);
            }
        }

        /* Fetch from L2 */
        if (g_mode >= 1) {
            printf("Read from L2 0x%X\n", addr & ~(LINE_SIZE - 1));
        }

        /* Install as Exclusive */
        cache_install(dc, index, tag, EXCLUSIVE);
    }
}

void dcache_write(Cache *dc, uint32_t addr)
{
    uint32_t index = ADDR_INDEX(addr);
    uint32_t tag   = ADDR_TAG(addr);

    dc->stats.writes++;

    int way = cache_lookup(dc, index, tag);
    if (way >= 0) {
        /* Write hit */
        dc->stats.hits++;
        MESIState cur = dc->sets[index].lines[way].state;

        if (cur == MODIFIED) {
            /* Subsequent write â€” write-back policy, no L2 communication */
            /* State stays M */
        } else {
            /* First write (E or S) â€” write-through to L2 */
            if (g_mode >= 1) {
                printf("Write to L2 0x%X\n", addr & ~(LINE_SIZE - 1));
            }
            dc->sets[index].lines[way].state = MODIFIED;
        }
        cache_update_lru(dc, index, way);
    } else {
        /* Write miss  Read for Ownership */
        dc->stats.misses++;

        /* Evict LRU */
        uint32_t ev_tag;
        MESIState ev_state;
        cache_evict_lru(dc, index, &ev_tag, &ev_state);

        if (ev_state == MODIFIED) {
            uint32_t ev_addr = make_line_addr(ev_tag, index);
            if (g_mode >= 1) {
                printf("Write to L2 0x%X\n", ev_addr);
            }
        }

        /* Read for Ownership from L2 */
        if (g_mode >= 1) {
            printf("Read for Ownership from L2 0x%X\n", addr & ~(LINE_SIZE - 1));
        }

        /* Install line */
        cache_install(dc, index, tag, EXCLUSIVE);

        /* First write  write-through to L2 */
        if (g_mode >= 1) {
            printf("Write to L2 0x%X\n", addr & ~(LINE_SIZE - 1));
        }

        /* Mark as Modified */
        int new_way = cache_lookup(dc, index, tag);
        if (new_way >= 0) {
            dc->sets[index].lines[new_way].state = MODIFIED;
        }
    }
}

void dcache_invalidate(Cache *dc, uint32_t addr)
{
    uint32_t index = ADDR_INDEX(addr);
    uint32_t tag   = ADDR_TAG(addr);

    int way = cache_lookup(dc, index, tag);
    if (way >= 0) {
        MESIState cur = dc->sets[index].lines[way].state;
        if (cur == MODIFIED) {
            /* Write back dirty line before invalidating */
            if (g_mode >= 1) {
                printf("Write to L2 0x%X\n", addr & ~(LINE_SIZE - 1));
            }
        }
        cache_invalidate_way(dc, index, way);
    }
}

void dcache_snoop(Cache *dc, uint32_t addr)
{
    uint32_t index = ADDR_INDEX(addr);
    uint32_t tag   = ADDR_TAG(addr);

    int way = cache_lookup(dc, index, tag);
    if (way >= 0) {
        MESIState cur = dc->sets[index].lines[way].state;
        if (cur == MODIFIED) {
            /* Return dirty data to L2 */
            if (g_mode >= 1) {
                printf("Return data to L2 0x%X\n", addr & ~(LINE_SIZE - 1));
            }
            /* Transition M â†’ Shared (L2 now has a copy) */
            dc->sets[index].lines[way].state = SHARED;
        }
        /* E or S: L2 already has the data, no message needed */
    }
}

