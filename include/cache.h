#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>

/* ─────────────── Cache Geometry ─────────────── */
#define LINE_SIZE       64          /* bytes per cache line           */
#define LINE_BITS       6           /* log2(LINE_SIZE)                */
#define NUM_SETS        16384       /* 16K sets                       */
#define SET_BITS        14          /* log2(NUM_SETS)                 */
#define ICACHE_WAYS     4           /* instruction cache associativity */
#define DCACHE_WAYS     8           /* data cache associativity       */

/* Address field extraction */
#define ADDR_OFFSET(a)  ((a) & 0x3F)
#define ADDR_INDEX(a)   (((a) >> LINE_BITS) & 0x3FFF)
#define ADDR_TAG(a)     ((a) >> (LINE_BITS + SET_BITS))

/* ─────────────── MESI States ─────────────── */
typedef enum {
    INVALID   = 0,
    SHARED    = 1,
    EXCLUSIVE = 2,
    MODIFIED  = 3
} MESIState;

static inline char mesi_char(MESIState s) {
    switch (s) {
        case MODIFIED:  return 'M';
        case EXCLUSIVE: return 'E';
        case SHARED:    return 'S';
        default:        return 'I';
    }
}

/* ─────────────── Cache Line ─────────────── */
typedef struct {
    uint32_t  tag;
    MESIState state;   /* INVALID means the line is not present */
} CacheLine;

/* ─────────────── Cache Set ─────────────── */
/* lru_order[0] = MRU way, lru_order[WAYS-1] = LRU way */
typedef struct {
    CacheLine lines[DCACHE_WAYS];   /* over-allocated; only WAYS entries used */
    int       lru_order[DCACHE_WAYS];
} CacheSet;

/* ─────────────── Statistics ─────────────── */
typedef struct {
    unsigned long reads;
    unsigned long writes;
    unsigned long hits;
    unsigned long misses;
} Stats;

/* ─────────────── Cache Object ─────────────── */
typedef struct {
    CacheSet *sets;     /* array of NUM_SETS sets    */
    int       ways;     /* associativity (4 or 8)    */
    Stats     stats;
    const char *name;   /* "Instruction" or "Data"   */
} Cache;

/* ─────────────── Mode (output verbosity) ─────────────── */
extern int g_mode;   /* 0 = stats only, 1 = +L2 messages */

/* ─────────────── Function Declarations ─────────────── */

/* Initialise / destroy */
void cache_init(Cache *c, int ways, const char *name);
void cache_destroy(Cache *c);
void cache_clear(Cache *c);

/* Lookup – returns way index (0..ways-1) or -1 if miss */
int  cache_lookup(Cache *c, uint32_t index, uint32_t tag);

/* LRU helpers */
int  cache_lru_way(Cache *c, uint32_t index);
void cache_update_lru(Cache *c, uint32_t index, int way);

/* Evict the LRU line from a set; returns evicted tag & state via out-params */
void cache_evict_lru(Cache *c, uint32_t index,
                     uint32_t *evicted_tag, MESIState *evicted_state);

/* Install a new line (after eviction, if any) */
void cache_install(Cache *c, uint32_t index, uint32_t tag, MESIState state);

/* Invalidate a specific way */
void cache_invalidate_way(Cache *c, uint32_t index, int way);

/* Print stats */
void cache_print_stats(const Cache *c);

/* Print valid lines (response to trace op 9) */
void cache_print_contents(const Cache *c);

/* ─────────────── High-level Operations ─────────────── */

/* Instruction cache */
void icache_read(Cache *ic, uint32_t addr);
void icache_invalidate(Cache *ic, uint32_t addr);
void icache_snoop(Cache *ic, uint32_t addr);

/* Data cache */
void dcache_read(Cache *dc, uint32_t addr);
void dcache_write(Cache *dc, uint32_t addr);
void dcache_invalidate(Cache *dc, uint32_t addr);
void dcache_snoop(Cache *dc, uint32_t addr);

#endif /* CACHE_H */
