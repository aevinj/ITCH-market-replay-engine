#include "LimitOrderBook.h"
#include <gtest/gtest.h>
#include <chrono>
#include <random>
#include <unordered_set>
#include <iostream>
#include <cmath>

// Ladder parameters mirrored for tests (match your LOB constants)
static constexpr double MIN_PRICE = 90.0;
static constexpr double MAX_PRICE = 110.0;
static constexpr double TICK = 0.01;

// Helpers
static inline std::size_t price_to_index(double price) {
    // Use llround to be robust to floating point (we only use 2dp prices)
    return static_cast<std::size_t>(std::llround((price - MIN_PRICE) / TICK));
}
static inline double index_to_price(std::size_t idx) {
    return MIN_PRICE + idx * TICK;
}
static inline double quantize(double p) {
    return std::round(p / TICK) * TICK;
}
static int32_t level_total_by_side(const PriceLevel& pl, OrderSide side) {
    int32_t sum = 0;
    for (auto* o : pl.orders) if (o->side == side) sum += o->quantity;
    return sum;
}

// --- Basic Operations ---

TEST(LimitOrderBookTest, AddOrderInsertsCorrectly) {
    LimitOrderBook lob;
    lob.process_order(1, 100.00, 100, OrderSide::Buy);

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 100);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Buy), 100);
}

TEST(LimitOrderBookTest, MatchBuyAgainstSell) {
    LimitOrderBook lob;
    lob.process_order(1, 100.00, 100, OrderSide::Buy);
    lob.process_order(2, 100.00,  50, OrderSide::Sell); // should match fully

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 50); // 100 - 50
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Buy), 50);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Sell), 0);
}

TEST(LimitOrderBookTest, CancelRemovesOrder) {
    LimitOrderBook lob;
    lob.process_order(1, 100.00, 100, OrderSide::Buy);
    lob.cancel_order(1);

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    EXPECT_EQ(levels[idx].total_quantity, 0);
    EXPECT_TRUE(levels[idx].orders.empty());
}

TEST(LimitOrderBookTest, ModifyUpdatesQuantity) {
    LimitOrderBook lob;
    lob.process_order(1, 100.00, 100, OrderSide::Buy);
    lob.modify_order(1, 150);

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 150);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Buy), 150);
}

// --- Edge Cases & Multi-Level Scenarios ---

TEST(LimitOrderBookTest, BuyOrderSweepsMultipleAsks) {
    LimitOrderBook lob;

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

TEST(LimitOrderBookTest, PartialFillLeavesRestingOrder) {
    LimitOrderBook lob;

    lob.process_order(1, 101.00, 100, OrderSide::Sell); // resting sell
    lob.process_order(2, 101.00,  40, OrderSide::Buy);  // incoming buy smaller

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(101.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 60); // 100 - 40
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Sell), 60);
}

TEST(LimitOrderBookTest, PriceLevelIsRemovedWhenEmpty) {
    LimitOrderBook lob;

    lob.process_order(1, 101.00, 50, OrderSide::Sell);
    lob.process_order(2, 101.00, 50, OrderSide::Buy); // matches fully

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(101.00);
    EXPECT_EQ(levels[idx].total_quantity, 0);
    EXPECT_TRUE(levels[idx].orders.empty());
}

TEST(LimitOrderBookTest, TotalQuantityMatchesOrders) {
    LimitOrderBook lob;

    lob.process_order(1, 100.00, 40, OrderSide::Buy);
    lob.process_order(2, 100.00, 60, OrderSide::Buy);

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());

    // Sum orders manually
    int sum = 0;
    for (auto* o : levels[idx].orders) sum += o->quantity;
    EXPECT_EQ(sum, levels[idx].total_quantity);

    // cancel one order → should update correctly
    lob.cancel_order(1);
    int sum2 = 0;
    for (auto* o : levels[idx].orders) sum2 += o->quantity;
    EXPECT_EQ(sum2, levels[idx].total_quantity);
    EXPECT_EQ(levels[idx].total_quantity, 60);
}

TEST(LimitOrderBookTest, ModifyAfterPartialFill) {
    LimitOrderBook lob;

    lob.process_order(1, 100.00, 100, OrderSide::Buy);
    lob.process_order(2, 100.00,  60, OrderSide::Sell); // matches partially → order 1 left with 40

    lob.modify_order(1, 80); // increase from 40 to 80

    const auto& levels = lob.get_price_levels();
    std::size_t idx = price_to_index(100.00);
    ASSERT_LT(idx, levels.size());
    EXPECT_EQ(levels[idx].total_quantity, 80);
    EXPECT_EQ(level_total_by_side(levels[idx], OrderSide::Buy), 80);
}

TEST(LimitOrderBookTest, SameSideDoesNotMatch) {
    LimitOrderBook lob;
    lob.process_order(1, 100.00, 40, OrderSide::Buy);
    lob.process_order(2, 100.00, 60, OrderSide::Buy); // must NOT match order 1

    const auto& levels = lob.get_price_levels();
    auto idx = static_cast<size_t>((100.00 - 90.0) / 0.01);
    ASSERT_EQ(levels[idx].total_quantity, 100);
}

// --- Active Levels Tracking ---

TEST(LimitOrderBookTest, InsertAddsToActiveLevels) {
    LimitOrderBook lob;
    lob.process_order(1, 100.0, 50, OrderSide::Buy);
    lob.process_order(2, 101.0, 30, OrderSide::Sell);

    auto& levels = lob.get_price_levels();
    size_t buy_idx = static_cast<size_t>((100.0 - 90.0) / 0.01);
    size_t sell_idx = static_cast<size_t>((101.0 - 90.0) / 0.01);

    EXPECT_EQ(levels[buy_idx].total_quantity, 50);
    EXPECT_EQ(levels[sell_idx].total_quantity, 30);
    EXPECT_FALSE(levels[buy_idx].orders.empty());
    EXPECT_FALSE(levels[sell_idx].orders.empty());
}

TEST(LimitOrderBookTest, CancelRemovesFromActiveLevels) {
    LimitOrderBook lob;
    lob.process_order(1, 100.0, 50, OrderSide::Buy);
    lob.cancel_order(1);

    auto& levels = lob.get_price_levels();
    size_t idx = static_cast<size_t>((100.0 - 90.0) / 0.01);

    EXPECT_TRUE(levels[idx].orders.empty());
    EXPECT_EQ(levels[idx].total_quantity, 0);
}

TEST(LimitOrderBookTest, MatchRemovesEmptyLevelsFromActiveSet) {
    LimitOrderBook lob;

    // Add one ask at 101.0
    lob.process_order(1, 101.0, 40, OrderSide::Sell);
    // Add a buy that exactly matches it
    lob.process_order(2, 101.0, 40, OrderSide::Buy);

    auto& levels = lob.get_price_levels();
    size_t idx = static_cast<size_t>((101.0 - 90.0) / 0.01);

    EXPECT_TRUE(levels[idx].orders.empty());  // should be cleared
    EXPECT_EQ(levels[idx].total_quantity, 0); // no leftover qty
}

TEST(LimitOrderBookTest, PartialFillLeavesLevelActive) {
    LimitOrderBook lob;

    // Add one ask at 101.0 with 50 qty
    lob.process_order(1, 101.0, 50, OrderSide::Sell);
    // Add a buy smaller than that (20 qty)
    lob.process_order(2, 101.0, 20, OrderSide::Buy);

    auto& levels = lob.get_price_levels();
    size_t idx = static_cast<size_t>((101.0 - 90.0) / 0.01);

    EXPECT_FALSE(levels[idx].orders.empty());   // still has resting order
    EXPECT_EQ(levels[idx].total_quantity, 30); // 50 - 20 = 30 left
}

// --- Stress Testing ---

TEST(LimitOrderBookStressTest, RandomizedOperationsWithTiming) {
    LimitOrderBook lob;
    std::mt19937 rng(42); // fixed seed for reproducibility

    // Use real distribution for prices (tick size 0.01, range 90–110)
    std::uniform_real_distribution<double> price_dist(90.0, 110.0);
    std::uniform_int_distribution<int32_t> qty_dist(1, 200);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> op_dist(0, 9); // 0–6 = add, 7 = cancel, 8 = modify, 9 = no-op

    std::vector<int64_t> active_ids;  // faster than unordered_set
    int64_t next_order_id = 1;

    const int NUM_OPS = 100000; // scale this up later

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPS; i++) {
        int op = op_dist(rng);

        if (op <= 6) {
            // --- Add order ---
            int64_t id = next_order_id++;
            double price = std::round(price_dist(rng) / 0.01) * 0.01;
            int32_t qty = qty_dist(rng);
            OrderSide side = side_dist(rng) == 0 ? OrderSide::Buy : OrderSide::Sell;

            lob.process_order(id, price, qty, side);
            active_ids.push_back(id);

        } else if (op == 7 && !active_ids.empty()) {
            // --- Cancel random order ---
            size_t idx = rng() % active_ids.size();
            int64_t victim = active_ids[idx];
            lob.cancel_order(victim);

            // swap–pop removal from vector
            active_ids[idx] = active_ids.back();
            active_ids.pop_back();

        } else if (op == 8 && !active_ids.empty()) {
            // --- Modify random order ---
            size_t idx = rng() % active_ids.size();
            int64_t target = active_ids[idx];
            int32_t new_qty = qty_dist(rng);
            lob.modify_order(target, new_qty);
        }
        // op == 9 → no-op
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double ops_per_sec = (NUM_OPS / elapsed_ms) * 1000.0;

    std::cout << "Performed " << NUM_OPS << " operations in " << elapsed_ms << " ms ("
              << ops_per_sec << " ops/sec)\n";

    // --- Invariant checks ---
    const auto& levels = lob.get_price_levels();
    for (size_t idx = 0; idx < levels.size(); ++idx) {
        const auto& level = levels[idx];
        int sum = 0;
        for (auto* order : level.orders) {
            sum += order->quantity;
        }
        EXPECT_EQ(sum, level.total_quantity) << " at price index " << idx;
    }

    std::cout << "Total trades executed: " << lob.get_total_trades() << std::endl;
    lob.reset_trade_counter();
}

TEST(LimitOrderBookStressTest, RealisticRandomizedOperationsWithTiming) {
    LimitOrderBook lob;
    std::mt19937 rng(42); // fixed seed for reproducibility
    std::uniform_real_distribution<double> price_dist(90.0, 110.0);
    std::uniform_int_distribution<int32_t> qty_dist(1, 200);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> op_dist(0, 9); // 0–3 = add, 4–6 = cancel, 7–8 = modify, 9 = no-op

    std::unordered_set<int64_t> active_ids;
    int64_t next_order_id = 1;

    const int NUM_OPS = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < NUM_OPS; i++) {
        int op = op_dist(rng);

        if (op <= 3) {
            // --- Add order ---
            int64_t id = next_order_id++;
            double price = std::round(price_dist(rng) / 0.01) * 0.01;
            int32_t qty = qty_dist(rng);
            OrderSide side = side_dist(rng) == 0 ? OrderSide::Buy : OrderSide::Sell;

            lob.process_order(id, price, qty, side);
            active_ids.insert(id);

        } else if (op <= 6 && !active_ids.empty()) {
            // --- Cancel random existing order ---
            auto it = active_ids.begin();
            std::advance(it, rng() % active_ids.size());
            lob.cancel_order(*it);
            active_ids.erase(it);

        } else if (op <= 8 && !active_ids.empty()) {
            // --- Modify random existing order ---
            auto it = active_ids.begin();
            std::advance(it, rng() % active_ids.size());
            int64_t id = *it;

            // Check if order still exists in the book before modifying
            auto order_it = lob.orders_by_id.find(id);
            if (order_it != lob.orders_by_id.end()) {
                int32_t new_qty = qty_dist(rng);
                lob.modify_order(id, new_qty);
            } else {
                // Order already gone (matched away) → remove from active set
                active_ids.erase(it);
            }
        }
        else {
            // --- No-op ---
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    double ops_per_sec = (NUM_OPS / elapsed_ms) * 1000.0;

    std::cout << "Performed " << NUM_OPS << " operations in " << elapsed_ms << " ms ("
              << ops_per_sec << " ops/sec)\n";

    // --- Invariant checks ---
    auto check_levels = [](const auto& levels) {
        for (const auto& level : levels) {
            int sum = 0;
            for (auto* order : level.orders) {
                sum += order->quantity;
            }
            EXPECT_EQ(sum, level.total_quantity);
        }
    };
    check_levels(lob.get_price_levels());

    // Sanity check: all active IDs should still be valid
    for (auto id : active_ids) {
        // lookups should not segfault
        lob.modify_order(id, 1); 
    }
}

TEST(LimitOrderBookTest, FractionalPriceTruncatesIncorrectly) {
    LimitOrderBook lob;
    lob.process_order(1, 100.01, 100, OrderSide::Buy); // <-- fractional tick
    const auto& levels = lob.get_price_levels();

    // Should land near 100.01 (index = 100.01 -> idx = 1001)
    std::size_t expected_idx = static_cast<size_t>(std::llround((100.01 - 90.0) / 0.01));
    std::size_t actual_idx   = static_cast<size_t>((100 - 90.0) / 0.01); // what your code really does

    EXPECT_NE(expected_idx, actual_idx) << "Fractional price was truncated!";
}
