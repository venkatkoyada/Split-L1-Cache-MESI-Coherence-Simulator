#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cache.h"

/* ──────────────────────────────────────────────────────
 *  Usage:  ./cache_sim <trace_file> [mode]
 *
 *  mode 0  (default) — print only statistics and 9-responses
 *  mode 1            — mode 0 + L2 communication messages
 * ────────────────────────────────────────────────────── */

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <trace_file> [mode]\n", prog);
    fprintf(stderr, "  mode 0 (default): statistics only\n");
    fprintf(stderr, "  mode 1          : +L2 communication messages\n");
    exit(1);
}

static void print_all_stats(Cache *ic, Cache *dc)
{
    cache_print_stats(ic);
    cache_print_stats(dc);
}

static void print_all_contents(Cache *ic, Cache *dc)
{
    cache_print_contents(ic);
    cache_print_contents(dc);
}

int main(int argc, char *argv[])
{
    if (argc < 2) usage(argv[0]);

    const char *trace_file = argv[1];
    g_mode = (argc >= 3) ? atoi(argv[2]) : 0;

    if (g_mode < 0 || g_mode > 1) {
        fprintf(stderr, "Error: mode must be 0 or 1\n");
        usage(argv[0]);
    }

    /* Initialise caches */
    Cache icache, dcache;
    cache_init(&icache, ICACHE_WAYS, "Instruction");
    cache_init(&dcache, DCACHE_WAYS, "Data");

    /* Open trace file */
    FILE *fp = fopen(trace_file, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open trace file '%s'\n", trace_file);
        cache_destroy(&icache);
        cache_destroy(&dcache);
        return 1;
    }

    char line[64];
    while (fgets(line, sizeof(line), fp)) {
        /* Skip blank lines and comment lines */
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '#') continue;

        int op;
        unsigned int addr_u;

        /* Op 8 and 9 may or may not have an address in the file */
        int fields = sscanf(line, "%d %x", &op, &addr_u);
        if (fields < 1) continue;

        uint32_t addr = (uint32_t)addr_u;

        switch (op) {

        case 0: /* Read from L1 data cache */
            dcache_read(&dcache, addr);
            break;

        case 1: /* Write to L1 data cache */
            dcache_write(&dcache, addr);
            break;

        case 2: /* Instruction fetch (L1 instruction cache) */
            icache_read(&icache, addr);
            break;

        case 3: /* Invalidate command from L2 — affects both caches */
            dcache_invalidate(&dcache, addr);
            icache_invalidate(&icache, addr);
            break;

        case 4: /* Data request from L2 (snoop) */
            dcache_snoop(&dcache, addr);
            icache_snoop(&icache, addr);
            break;

        case 8: /* Clear caches and reset statistics */
            cache_clear(&icache);
            cache_clear(&dcache);
            break;

        case 9: /* Print contents and statistics */
            print_all_contents(&icache, &dcache);
            print_all_stats(&icache, &dcache);
            break;

        default:
            fprintf(stderr, "Warning: unknown operation %d, skipping\n", op);
            break;
        }
    }

    fclose(fp);

    /* Final statistics summary */
    printf("\n==========================================\n");
    printf("  Final Cache Statistics\n");
    printf("==========================================\n");
    print_all_stats(&icache, &dcache);

    cache_destroy(&icache);
    cache_destroy(&dcache);
    return 0;
}
