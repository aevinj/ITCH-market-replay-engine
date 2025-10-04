# High-Performance Limit Order Book (LOB)

A C++ implementation of a Limit Order Book designed as a foundation for exploring
**ultra-low-latency trading system components**.  

This repository documents the journey from a simple, correct baseline into an **optimized, cache-friendly, high-performance engine**, inspired by the design principles of High-Frequency Trading (HFT) systems

---

## Features (Current Implementation)

- ✅ Add, cancel, and modify limit orders
- ✅ Matching engine with:
   - Partial and full fills
   - Multi-level sweeps
   - Price–time priority
- ✅ Stress test framework with invariant checks
- ✅ Unit test suite (GoogleTest) — 10+ functional tests
- ✅ Profiling support (gperftools)
- ✅ Custom memory pool for deterministic order allocation (no heap allocs in hot path)
- ✅ Fixed tick size (0.01) and bounded price range (90–110)
- ✅ Price-indexed vector of levels instead of std::map (removes red–black tree overhead)
- ✅ Active level tracking (best bid/ask lookup in `O(1)`)
- Reserved capacity for orders_by_id (avoids costly rehashing)

---

## Benchmark Results (Baseline)
**Environment:** macOS, Apple M2, Release build

**Workload:** Random 100K operations (add / cancel / modify / match)

| Stage | Throughput | Notes |
|-------|------------|-------|
|Baseline| ~53,769 ops/sec | ```std::map``` + ```std::list``` + heap allocation |
|Memory Pool| ~50-55k ops/sec | Allocation costs removed, container overhead still dominant |
|Price-Indexed Vector + Active Level Sets| ~563,683 ops/sec | 10× jump — contiguous levels, integer tick indexing |
|+ Reserved Hash Table Capacity| ~785,000 ops/sec | Avoids rehashing during stress workload |
|Post Bug Fix (Validated)| ≈ 5.3 M ops/sec (100 K ops in 18.7 ms) | Matching logic corrected; metrics now reflect true workload |

**Overall: ~100× improvement from baseline by aligning data structures with hardware realities.**

---

## What Changed & Why It Matters

Earlier throughput figures (~700 k ops/sec) were measured on a version where the matching path was inadvertently executing extra redundant comparisons, artificially inflating operation counts.
After refactoring to correct the matching logic and convert prices to `double` precision with proper tick quantisation:

- The system now reflects **true end-to-end throughput** — no phantom work.
- A trade counter shows that roughly **54 % of all insertions result in real matches**, confirming that we **do not** suffer from a sparse-book issue.
- Each order operation now completes in roughly **≈ 370 ns**, which is a realistic figure for an in-memory single-threaded LOB on modern CPUs.

This milestone marks the transition from *synthetic speed to verified low-latency performance*.

---

## Architecture Evolution

```mermaid
flowchart TD
    subgraph Baseline["Baseline (Naive)"]
        A["std::map (red-black tree)"] --> B["std::list (per price level)"]
        B --> C["Heap-allocated Order objects"]
    end

    subgraph Optimised["Optimised (Current)"]
        X["Vector of Price Levels (tick-indexed)"] --> Y["Vector<Order*> (FIFO per level)"]
        Y --> Z["Memory Pool (pre-allocated Orders)"]
        X --> W["Active Bid/Ask Sets (track non-empty levels)"]
    end
```

---
## Performance Evolution

```mermaid
flowchart LR
    A["Baseline<br/>(~53,769 ops/sec)"] 
    B["Memory Pool<br/>(~55,000 ops/sec)"] 
    C["Vector Levels<br/>(~563,683 ops/sec)"] 
    D["Vector+Reserve<br/>(~785,000 ops/sec)"]

    A --> B --> C --> D
```
---

## Development Roadmap
Each stage is a milestone in systems-programming design:

1. **Baseline Correctness**
   - `std::map` for bids/asks
   - `std::list` for price-time order queues
   - Naive heap allocation
   - Stress testing + profiling setup

2. **Optimisations (Current)**
   - Memory pool allocator
   - Price-indexed vector levels (integer tick indexing)
   - Active level tracking sets
   - Pre-reserved `orders_by_id` capacity

3. **Future Extensions**
   - Ring buffer for `O(1)` cancels
   - Flat hash maps for order lookups under high churn
   - Backtesting integration
   - Persistent logging of trades

---

## Profiling Insights (Baseline)
Profiling (via gperftools pprof) evolution:

- **Baseline**: ~65% CPU in `libc++` (`std::map`, `std::list`, allocator churn).
- **With Memory Pool**: Allocation costs gone, but list traversal & tree operations remain bottleneck.
- **With Vector Levels + Active Sets**: Matching logic dominates runtime; cache locality greatly improved.
- **With Reserved Hash Table**: Reduced rehashing → smoother throughput in stress tests.

---

## How to Build & Run
### Build
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Run demo app
```bash
./build/OrderBookApp
```
### Run tests
```bash
ctest --test-dir build --output-on-failure
```
### Run with profiler
```bash
CPUPROFILE=profile.out ./build/OrderBookTests --gtest_filter=LimitOrderBookStressTest.RandomizedOperationsWithTiming
pprof --pdf ./build/OrderBookTests profile.out > profile.pdf
pprof --text ./build/OrderBookTests profile.out | head -40
```
### References
- Performance-Aware Programming (Fermilab, 2019)
- High-Frequency Trading Systems Design (various sources)
- Google Performance Tools (gperftools)

### Author's Note
This project demonstrates a full optimisation journey: from naive STL containers to a cache-friendly, low-latency order book that achieves nearly 800k ops/sec under stress workloads.

Key takeaways:

- Hardware-aware data structure design yields order-of-magnitude performance gains.
- Not all optimisations pay off (e.g. pre-reserving per-level storage hurt performance).
- Measuring, profiling, and iterating are more important than guessing.

This project is portfolio-ready and showcases my ability to apply low-level systems design and performance engineering principles to a real-world trading component.