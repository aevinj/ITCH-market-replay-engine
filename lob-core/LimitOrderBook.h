#pragma once

#include "Order.h"
#include <vector>
#include <set>
#include <list>
#include <functional>
#include <unordered_map>
#include <MemoryPool.h>
#include <cmath>

// Represents a collection of orders at a single price level
class PriceLevel
{
public:
    // Use a list to maintain Price-Time Priority
    std::vector<Order *> orders;
    int32_t total_quantity = 0;
};

class LimitOrderBook
{
private:
    static constexpr double EPSILON = 1e-9;
    static constexpr double MIN_PRICE = 90.0;
    static constexpr double MAX_PRICE = 110.0;
    static constexpr double TICK_SIZE = 0.01;
    static constexpr size_t NUM_LEVELS = static_cast<size_t>((MAX_PRICE - MIN_PRICE) / TICK_SIZE) + 1;

    // Maps for bids (sorted descending) and asks (sorted ascending)
    std::vector<PriceLevel> price_levels;
    MemoryPool<Order> order_pool;

    static inline size_t total_trades = 0;

    std::set<size_t> active_bids; // indices of price levels with buy orders
    std::set<size_t> active_asks; // indices of price levels with sell orders

    size_t price_to_index(double price) const
    {
        // Snap to nearest tick
        const double normalized = (price - MIN_PRICE) / TICK_SIZE;
        long long rounded = std::llround(normalized);

        // Clamp both ends
        if (rounded < 0)
            rounded = 0;
        if (rounded >= static_cast<long long>(NUM_LEVELS))
            rounded = static_cast<long long>(NUM_LEVELS) - 1;

        return static_cast<size_t>(rounded);
    }

    void match(Order *incoming,
               const std::function<void(const Order &, const Order &, double, int32_t)> &onTrade = nullptr);
    void insert_order(Order *incoming);

public:
    std::unordered_map<int64_t, Order *> orders_by_id;
    explicit LimitOrderBook(size_t pool_size = 1'000'000)
        : price_levels(NUM_LEVELS), order_pool(pool_size)
    {
        orders_by_id.reserve(100'000);
    }
    void process_order(int64_t order_id, double price, int32_t quantity, OrderSide side);

    // Overload WITH callback
    void process_order(int64_t order_id, double price, int32_t quantity, OrderSide side,
                       const std::function<void(const Order&, const Order&, double, int32_t)>& onTrade);

    void cancel_order(int64_t order_id);
    void modify_order(int64_t order_id, int32_t new_quantity);

    // Getter for vector of price levels
    const std::vector<PriceLevel> &get_price_levels() const { return price_levels; }

    size_t get_total_trades() const;
    void reset_trade_counter();
};