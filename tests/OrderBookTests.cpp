#include "LimitOrderBook.h"
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <unordered_set>
#include <iostream>
#include <cmath>

// These must match how you construct the LOB in main()
static constexpr double TEST_MIN_PRICE = 0.0;
static constexpr double TEST_MAX_PRICE = 1000.0;
static constexpr double TICK         = 0.01;

// Helpers to mirror the LOB's price indexing
static inline std::size_t price_to_index(double price) {
    return static_cast<std::size_t>(
        std::llround((price - TEST_MIN_PRICE) / TICK)
    );
}

static int32_t level_total_by_side(const PriceLevel& pl, OrderSide side) {
    int32_t sum = 0;
    for (auto* o : pl.orders)
        if (o->side == side)
            sum += o->quantity;
    return sum;
}

// ---------- Basic operations ----------

TEST(LimitOrderBookBasic, AddOrderInsertsCorrectly) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);
    lob.process_order(1, 100.00, 100, OrderSide::Buy);

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 100);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Buy), 100);
}

TEST(LimitOrderBookBasic, MatchBuyAgainstSell) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);
    lob.process_order(1, 100.00, 100, OrderSide::Buy);
    lob.process_order(2, 100.00,  50, OrderSide::Sell); // should match fully

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 50); // 100 - 50
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Buy), 50);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Sell), 0);
}

TEST(LimitOrderBookBasic, CancelRemovesOrder) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);
    lob.process_order(1, 100.00, 100, OrderSide::Buy);
    lob.cancel_order(1);

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 0);
    EXPECT_TRUE(levels[idx].orders.empty());
}

TEST(LimitOrderBookBasic, ReduceOrderDecreasesQuantity) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);
    lob.process_order(1, 100.00, 100, OrderSide::Buy);

    lob.reduce_order(1, 40);  // partial cancel → 60 remaining

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 60);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Buy), 60);
}

TEST(LimitOrderBookBasic, ReduceOrderCancelsWhenTooLarge) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);
    lob.process_order(1, 100.00, 50, OrderSide::Buy);

    // cancel >= remaining quantity → behaves like full cancel
    lob.reduce_order(1, 100);

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 0);
    EXPECT_TRUE(levels[idx].orders.empty());
}

// ---------- Edge cases & multi-level behaviour ----------

TEST(LimitOrderBookMatching, BuyOrderSweepsMultipleAsks) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);

    // Add asks at increasing prices
    lob.process_order(1, 101.00,  50, OrderSide::Sell);
    lob.process_order(2, 102.00,  75, OrderSide::Sell);
    lob.process_order(3, 103.00, 100, OrderSide::Sell);

    // Large buy order should sweep across levels
    lob.process_order(4, 103.00, 200, OrderSide::Buy);

    const auto& levels = lob.get_price_levels();

    // 50 + 75 + (100 -> 75 filled) leaves 25 at 103.00
    std::size_t idx103 = price_to_index(103.00);
    ASSERT_LT(idx103, levels.size());
    EXPECT_EQ(levels[idx103].total_quantity, 25);

    // Lower ask levels emptied
    EXPECT_EQ(levels[price_to_index(101.00)].total_quantity, 0);
    EXPECT_EQ(levels[price_to_index(102.00)].total_quantity, 0);
}

TEST(LimitOrderBookMatching, PartialFillLeavesRestingOrder) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);

    lob.process_order(1, 101.00, 100, OrderSide::Sell); // resting sell
    lob.process_order(2, 101.00,  40, OrderSide::Buy);  // incoming buy smaller

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(101.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 60); // 100 - 40
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Sell), 60);
}

TEST(LimitOrderBookMatching, PriceLevelEmptiedAfterFullCross) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);

    lob.process_order(1, 101.00, 50, OrderSide::Sell);
    lob.process_order(2, 101.00, 50, OrderSide::Buy); // matches fully

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(101.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 0);
    EXPECT_TRUE(levels[idx].orders.empty());
}

TEST(LimitOrderBookMatching, SameSideOrdersDoNotMatch) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);
    lob.process_order(1, 100.00, 40, OrderSide::Buy);
    lob.process_order(2, 100.00, 60, OrderSide::Buy); // must NOT match order 1

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 100);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Buy), 100);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Sell), 0);
}

// ---------- Quantisation / tick handling ----------

TEST(LimitOrderBookPrice, FractionalPriceIsRoundedToNearestTick) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);

    // Price slightly above a tick
    lob.process_order(1, 100.011, 100, OrderSide::Buy);

    const auto& levels = lob.get_price_levels();

    std::size_t idx_exact   = price_to_index(100.01);
    std::size_t idx_trunc   = price_to_index(100.00);

    ASSERT_LT(idx_exact, levels.size());
    EXPECT_EQ(levels[idx_exact].total_quantity, 100);
    EXPECT_TRUE(levels[idx_trunc].orders.empty());
}

// ---------- Invariants under randomised operations ----------

TEST(LimitOrderBookStress, RandomisedAddCancelReduceKeepsTotalsConsistent) {
    LimitOrderBook lob(TEST_MIN_PRICE, TEST_MAX_PRICE);
    std::mt19937 rng(42); // fixed seed for reproducibility

    std::uniform_real_distribution<double> price_dist(100.0, 300.0);
    std::uniform_int_distribution<int32_t> qty_dist(1, 200);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> op_dist(0, 9); // 0–5 = add, 6–7 = cancel, 8 = reduce, 9 = no-op

    std::vector<int64_t> active_ids;
    active_ids.reserve(100000);
    int64_t next_order_id = 1;

    const int NUM_OPS = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPS; ++i) {
        int op = op_dist(rng);

        if (op <= 5) {
            // --- Add order ---
            int64_t id = next_order_id++;
            double price = std::round(price_dist(rng) / TICK) * TICK;
            int32_t qty = qty_dist(rng);
            OrderSide side = side_dist(rng) == 0 ? OrderSide::Buy : OrderSide::Sell;

            lob.process_order(id, price, qty, side);
            active_ids.push_back(id);

        } else if (op <= 7 && !active_ids.empty()) {
            // --- Cancel random order ---
            std::size_t idx = rng() % active_ids.size();
            int64_t victim = active_ids[idx];

            lob.cancel_order(victim);
            active_ids[idx] = active_ids.back();
            active_ids.pop_back();

        } else if (op == 8 && !active_ids.empty()) {
            // --- Reduce random order (partial cancel) ---
            std::size_t idx = rng() % active_ids.size();
            int64_t id = active_ids[idx];

            auto it = lob.orders_by_id.find(id);
            if (it != lob.orders_by_id.end()) {
                Order* o = it->second;
                if (o->quantity > 0) {
                    std::uniform_int_distribution<int32_t> cancel_dist(1, o->quantity);
                    int32_t cancelled = cancel_dist(rng);
                    lob.reduce_order(id, cancelled);
                    if (cancelled >= o->quantity) {
                        active_ids[idx] = active_ids.back();
                        active_ids.pop_back();
                    }
                }
            } else {
                // Already matched or cancelled → drop from active set
                active_ids[idx] = active_ids.back();
                active_ids.pop_back();
            }
        }
        // op == 9 → no-op
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double ops_per_sec = (NUM_OPS / elapsed_ms) * 1000.0;

    std::cout << "Performed " << NUM_OPS << " operations in " << elapsed_ms
              << " ms (" << ops_per_sec << " ops/sec)\n";

    // --- Invariant: level.total_quantity equals sum of order quantities ---
    const auto& levels = lob.get_price_levels();
    for (std::size_t idx = 0; idx < levels.size(); ++idx) {
        const auto& level = levels[idx];
        int sum = 0;
        for (auto* order : level.orders) {
            sum += order->quantity;
        }
        EXPECT_EQ(sum, level.total_quantity) << " at price index " << idx;
    }
}
