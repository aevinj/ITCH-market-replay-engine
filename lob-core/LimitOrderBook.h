#pragma once

#include "Order.h"
#include <vector>
#include <set>
#include <list>
#include <functional>
#include <unordered_map>
#include <MemoryPool.h>
#include <cmath>
#include <optional>

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
    static constexpr double TICK_SIZE = 0.01;
    double min_price;
    double max_price;
    size_t num_levels;

    // Maps for bids (sorted descending) and asks (sorted ascending)
    std::vector<PriceLevel> price_levels;
    MemoryPool<Order> order_pool;

    static inline size_t total_trades = 0;

    std::set<size_t> active_bids; // indices of price levels with buy orders
    std::set<size_t> active_asks; // indices of price levels with sell orders

    size_t price_to_index(double price) const
    {
        // Snap to nearest tick
        const double normalized = (price - min_price) / TICK_SIZE;
        long long rounded = std::llround(normalized);

        // Clamp both ends
        if (rounded < 0)
            rounded = 0;
        if (rounded >= static_cast<long long>(num_levels))
            rounded = static_cast<long long>(num_levels) - 1;

        return static_cast<size_t>(rounded);
    }

    void match(Order *incoming,
               const std::function<void(const Order &, const Order &, double, int32_t)> &onTrade = nullptr);
    void insert_order(Order *incoming);

public:
    // For GUI feedback
    struct BestLevel {
        double price = 0.0;
        int32_t quantity = 0;
        bool valid = false;
    };

    std::unordered_map<int64_t, Order *> orders_by_id;
    explicit LimitOrderBook(double min_price, double max_price, size_t pool_size = 1'000'000)
        : min_price(min_price), max_price(max_price), num_levels((max_price - min_price) / TICK_SIZE + 1), price_levels(num_levels), order_pool(pool_size)
    {
        orders_by_id.reserve(100'000);
    }
    void process_order(int64_t order_id, double price, int32_t quantity, OrderSide side);

    // Overload WITH callback
    void process_order(int64_t order_id, double price, int32_t quantity, OrderSide side,
                       const std::function<void(const Order &, const Order &, double, int32_t)> &onTrade);

    void cancel_order(int64_t order_id);
    void reduce_order(int64_t order_id, int32_t cancelled_shares);
    std::optional<OrderSide> get_side(int64_t order_id);

    // Getter for vector of price levels
    const std::vector<PriceLevel> &get_price_levels() const { return price_levels; }

    size_t get_total_trades() const;
    void reset_trade_counter();

    BestLevel get_best_bid() const;
    BestLevel get_best_ask() const;
};