# Design and Simulation of a Split L1 Cache with MESI Coherence

A C11 trace-driven simulator of a split Level-1 cache subsystem (separate Instruction Cache and Data Cache) for a 32-bit multiprocessor architecture, implementing the **MESI cache coherence protocol**. Built for ECE 585 (Microprocessor System Design) at Portland State University, Winter 2026.

Developed as a 4-person team project by Venkat Sai Sumanth Koyada, Sai Tarun Mokkala, Nikhil Swarna, and Mrudula Chekuri. Instructor: Prof. Yuchen Huang.

## Overview
Each simulated processor has separate L1 caches - an Instruction Cache (I$) for fetching instructions and a Data Cache (D$) for loads/stores — both interacting with a shared, inclusive L2 cache. The simulator processes a memory access trace and models cache behavior across four dimensions:

- **Geometry:** 64-byte lines, 16,384 sets, 4-way set-associative I-cache, 8-way set-associative D-cache
- **Replacement:** LRU (least recently used), with invalid lines prioritized for eviction before applying LRU
- **Coherence:** Full MESI protocol (Modified / Exclusive / Shared / Invalid) with state transitions driven by processor reads/writes and bus snoop operations
- **Reporting:** Per-cache hit/miss statistics and final valid cache-line contents (tag, state, LRU rank)

Full design rationale, address decoding details, results, and learning outcomes are documented in [`docs/PROJECT REPORT.pdf`](./docs/PROJECT%20REPORT.pdf).

## My Contribution
I implemented the **MESI cache coherence protocol**, managing the four cache states (Modified, Exclusive, Shared, Invalid) and designing the state transition logic for read, write, and snoop operations to maintain coherence across caches in a multiprocessor environment. I ensured state updates correctly followed protocol rules - including transitions triggered by processor requests versus bus transactions — and integrated MESI state handling with the cache read/write operations to guarantee data consistency and proper synchronization between caches.

## Team Contributions

| Member | Area | Responsibility |
|---|---|---|
| **Venkat Sai Sumanth Koyada** | MESI coherence protocol | State transition logic for read/write/snoop operations; protocol-compliant state updates for processor requests and bus transactions; integration with cache read/write operations |
| Sai Tarun Mokkala | Replacement policy | LRU replacement policy; invalid-line-first eviction priority; failure-scenario testing (conflicts, replacement edge cases) |
| Nikhil Swarna | Cache modules & snooping | Data Cache and Instruction Cache read/write operations; bus snooping mechanisms; coherence event handling and validation |
| Mrudula Chekuri | Cache architecture | Overall split I$/D$/L2 architecture; address decomposition (tag/index/offset extraction); cache parameter definitions (size, associativity, block size) |

Full role breakdown, design rationale, and results are documented in [`docs/PROJECT REPORT.pdf`](./docs/PROJECT%20REPORT.pdf).

## Project Structure
```
include/  → cache.h — cache geometry, MESI states, cache line/set/stats structs
src/      → cache.c (cache operations, MESI logic, LRU), main.c (trace-driven driver)
tests/    → Trace files (test1-test4, test6, stresstrace, trace)
docs/     → Full project report (design, address decoding, results, roles, learning outcomes)
Makefile  → Build configuration
```

## Build & Run
```
make
./cachesim <trace_file.txt>
```

Example:
```
./cachesim tests/test1.txt
```

Output includes final I-cache and D-cache contents (valid lines only, showing set, way, tag, MESI state, and LRU rank) plus per-cache read/write/hit/miss statistics and hit ratio.
