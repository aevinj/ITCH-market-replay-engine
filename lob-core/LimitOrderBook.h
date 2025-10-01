#ifndef ORDERBOOK_LIMITORDERBOOK_H
#define ORDERBOOK_LIMITORDERBOOK_H

#include "Order.h"
#include <vector>
#include <set>
#include <list>
// #include <absl/container/flat_hash_map.h>
#include <unordered_map>
#include <MemoryPool.h>

// Represents a collection of orders at a single price level
class PriceLevel {
public:
    // Use a list to maintain Price-Time Priority
    std::vector<Order*> orders;
    int32_t total_quantity = 0;
};

class LimitOrderBook {
private:
    static constexpr double MIN_PRICE = 90.0;
    static constexpr double MAX_PRICE = 110.0;
    static constexpr double TICK_SIZE = 0.01;
    static constexpr size_t NUM_LEVELS = static_cast<size_t>((MAX_PRICE - MIN_PRICE) / TICK_SIZE) + 1;

    // Maps for bids (sorted descending) and asks (sorted ascending)
    std::vector<PriceLevel> price_levels;
    // std::unordered_map<int64_t, Order*> orders_by_id;
    // absl::flat_hash_map<int64_t, Order*> orders_by_id; // For quick order lookup by ID
    MemoryPool<Order> order_pool;

    std::set<size_t> active_bids; // indices of price levels with buy orders
    std::set<size_t> active_asks; // indices of price levels with sell orders

    size_t price_to_index(double price) const {
        return static_cast<size_t>((price - MIN_PRICE) / TICK_SIZE);
    }
    void match(Order* incoming);
    void insert_order(Order* incoming);
public:
    std::unordered_map<int64_t, Order*> orders_by_id;
    // absl::flat_hash_map<int64_t, Order*> orders_by_id; // For quick order lookup by ID
    explicit LimitOrderBook(size_t pool_size = 1'000'000)
        : price_levels(NUM_LEVELS), order_pool(pool_size) {
            orders_by_id.reserve(100'000);
        }
    // void add_order(int64_t order_id, int64_t price, int32_t quantity, OrderSide side);
    void process_order(int64_t order_id, int64_t price, int32_t quantity, OrderSide side);
    void cancel_order(int64_t order_id);
    void modify_order(int64_t order_id, int32_t new_quantity);

    // Getter for vector of price levels
    const std::vector<PriceLevel>& get_price_levels() const { return price_levels; }
};

#endif // ORDERBOOK_LIMITORDERBOOK_H