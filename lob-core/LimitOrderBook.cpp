#include "LimitOrderBook.h"
#include <iostream>
#include <functional>
#include <optional>

void LimitOrderBook::process_order(int64_t order_id, double price, int32_t quantity, OrderSide side, 
    const std::function<void(const Order&, const Order&, double, int32_t)>& onTrade) {
    price = std::round(price / TICK_SIZE) * TICK_SIZE;
    Order* new_order_ptr = order_pool.allocate();
    new_order_ptr->order_id = order_id;
    new_order_ptr->price = price;
    new_order_ptr->quantity = quantity;
    new_order_ptr->side = side;
    orders_by_id[order_id] = new_order_ptr;
    
    // try to match
    match(new_order_ptr, onTrade);

    // if still has quantity, insert into book
    if (new_order_ptr->quantity > 0) {
        insert_order(new_order_ptr);
    } else {
        orders_by_id.erase(order_id);
        order_pool.deallocate(new_order_ptr);
    }
}

void LimitOrderBook::process_order(int64_t order_id, double price, int32_t quantity, OrderSide side)
{
    process_order(order_id, price, quantity, side, nullptr);
}

void LimitOrderBook::match(Order* incoming, const std::function<void(const Order&, const Order&, double, int32_t)>& onTrade) {
    if (incoming->side == OrderSide::Buy) {
        while (incoming->quantity > 0 && !active_asks.empty()) {
            size_t best_ask_idx = *active_asks.begin();
            double best_ask_price = min_price + best_ask_idx * TICK_SIZE;

            if (incoming->price + EPSILON < best_ask_price) break;

            auto& level = price_levels[best_ask_idx];
            auto& orders_vec = level.orders;

            for (auto it = orders_vec.begin(); it != orders_vec.end() && incoming->quantity > 0;) {
                Order* resting = *it;
                if (resting->side != OrderSide::Sell) break;

                int32_t trade_qty = std::min(incoming->quantity, resting->quantity);
                if (trade_qty > 0) {
                    incoming->quantity -= trade_qty;
                    resting->quantity -= trade_qty;
                    level.total_quantity -= trade_qty;
                    total_trades++;

                    if (onTrade) {
                        onTrade(*incoming, *resting, best_ask_price, trade_qty);
                    }
                }

                if (resting->quantity == 0) {
                    orders_by_id.erase(resting->order_id);
                    order_pool.deallocate(resting);
                    it = orders_vec.erase(it);
                } else {
                    ++it;
                }
            }

            if (orders_vec.empty()) {
                active_asks.erase(best_ask_idx);
            }
        }
    } else { // incoming->side == Sell
        while (incoming->quantity > 0 && !active_bids.empty()) {
            size_t best_bid_idx = *active_bids.rbegin();
            double best_bid_price = min_price + best_bid_idx * TICK_SIZE;

            if (incoming->price - EPSILON > best_bid_price) break;

            auto& level = price_levels[best_bid_idx];
            auto& orders_vec = level.orders;

            for (auto it = orders_vec.begin(); it != orders_vec.end() && incoming->quantity > 0;) {
                Order* resting = *it;
                if (resting->side != OrderSide::Buy) break;

                int32_t trade_qty = std::min(incoming->quantity, resting->quantity);
                if (trade_qty > 0) {
                    incoming->quantity -= trade_qty;
                    resting->quantity -= trade_qty;
                    level.total_quantity -= trade_qty;
                    total_trades++;

                    if (onTrade) {
                        onTrade(*incoming, *resting, best_bid_price, trade_qty);
                    }
                }

                if (resting->quantity == 0) {
                    orders_by_id.erase(resting->order_id);
                    order_pool.deallocate(resting);
                    it = orders_vec.erase(it);
                } else {
                    ++it;
                }
            }

            if (orders_vec.empty()) {
                active_bids.erase(best_bid_idx);
            }
        }
    }
}


void LimitOrderBook::insert_order(Order* incoming) {
    // The price_levels vector will only ever store one side at a time - if there
    // was a buy and sell at 1 price level, it would've already matched -- its basc
    // a backlog of orders waiting to be matched
    size_t idx = price_to_index(incoming->price);
    auto& level = price_levels[idx];

    if (level.orders.empty()) {
        if (incoming->side == OrderSide::Buy) active_bids.insert(idx);
        else active_asks.insert(idx);
    }

    level.orders.push_back(incoming);
    level.total_quantity += incoming->quantity;
}

void LimitOrderBook::cancel_order(int64_t order_id) {
    auto it = orders_by_id.find(order_id);
    if (it == orders_by_id.end()) return;

    Order* order_ptr = it->second;
    size_t idx = price_to_index(order_ptr->price);
    auto& level = price_levels[idx];

    level.total_quantity -= order_ptr->quantity;

    auto& orders_vec = level.orders;
    for (size_t i = 0; i < orders_vec.size(); ++i) {
        if (orders_vec[i]->order_id == order_id) {
            orders_vec.erase(orders_vec.begin() + i);
            break;
        }
    }

    if (orders_vec.empty()) {
        if (order_ptr->side == OrderSide::Buy) active_bids.erase(idx);
        else active_asks.erase(idx);
    }

    orders_by_id.erase(it);
    order_pool.deallocate(order_ptr);
}

void LimitOrderBook::reduce_order(int64_t order_id, int32_t cancelled_shares) {
    auto it = orders_by_id.find(order_id);
    if (it == orders_by_id.end()) return;

    Order* order_ptr = it->second;

    if (cancelled_shares >= order_ptr->quantity) {
        cancel_order(order_id);
        return;
    }

    order_ptr->quantity -= cancelled_shares;

    size_t idx = price_to_index(order_ptr->price);
    price_levels[idx].total_quantity -= cancelled_shares;
}

size_t LimitOrderBook::get_total_trades() const {
    return total_trades;
}

void LimitOrderBook::reset_trade_counter() {
    total_trades = 0;
}

std::optional<OrderSide> LimitOrderBook::get_side(int64_t order_id) {
    auto it = orders_by_id.find(order_id);
    if (it == orders_by_id.end() || it->second == nullptr)
        return std::nullopt;

    return it->second->side;
}

LimitOrderBook::BestLevel LimitOrderBook::get_best_ask() const {
    if (active_asks.empty()) {
        return {0.0, 0, false};
    }

    size_t best_ask_idx = *(active_asks.begin());
    double best_ask_price = min_price + best_ask_idx * TICK_SIZE;

    return {
        best_ask_price,
        price_levels[best_ask_idx].total_quantity,
        true
    };
}

LimitOrderBook::BestLevel LimitOrderBook::get_best_bid() const {
    if (active_bids.empty()) {
        return {0.0, 0, false};
    }

    size_t best_bid_idx = *(active_bids.rbegin());
    double best_bid_price = min_price + best_bid_idx * TICK_SIZE;

    return {
        best_bid_price,
        price_levels[best_bid_idx].total_quantity,
        true
    };
}