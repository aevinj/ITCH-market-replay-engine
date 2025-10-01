#include "LimitOrderBook.h"
#include <iostream>

void LimitOrderBook::process_order(int64_t order_id, int64_t price, int32_t quantity, OrderSide side) {
    Order* new_order_ptr = order_pool.allocate();
    *new_order_ptr = Order{order_id, price, quantity, side};
    orders_by_id[order_id] = new_order_ptr;
    
    // try to match
    match(new_order_ptr);

    // if still has quantity, insert into book
    if (new_order_ptr->quantity > 0) {
        insert_order(new_order_ptr);
    } else {
        orders_by_id.erase(order_id);
        order_pool.deallocate(new_order_ptr);
    }
}

void LimitOrderBook::match(Order* incoming) {
    if (incoming->side == OrderSide::Buy) {
        while (incoming->quantity > 0 && !active_asks.empty()) {
            size_t best_ask_idx = *active_asks.begin();
            double best_ask_price = MIN_PRICE + best_ask_idx * TICK_SIZE;

            if (incoming->price < best_ask_price) break;

            auto& level = price_levels[best_ask_idx];
            auto& orders_vec = level.orders;

            for (auto it = orders_vec.begin(); it != orders_vec.end() && incoming->quantity > 0;) {
                Order* resting = *it;
                if (resting->side != OrderSide::Sell) break;

                int32_t trade_qty = std::min(incoming->quantity, resting->quantity);
                incoming->quantity -= trade_qty;
                resting->quantity -= trade_qty;
                level.total_quantity -= trade_qty;

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
            double best_bid_price = MIN_PRICE + best_bid_idx * TICK_SIZE;

            if (incoming->price > best_bid_price) break;

            auto& level = price_levels[best_bid_idx];
            auto& orders_vec = level.orders;

            for (auto it = orders_vec.begin(); it != orders_vec.end() && incoming->quantity > 0;) {
                Order* resting = *it;
                if (resting->side != OrderSide::Buy) break;

                int32_t trade_qty = std::min(incoming->quantity, resting->quantity);
                incoming->quantity -= trade_qty;
                resting->quantity -= trade_qty;
                level.total_quantity -= trade_qty;

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

void LimitOrderBook::modify_order(int64_t order_id, int32_t new_quantity) {
    auto order_by_id_it = orders_by_id.find(order_id);
    if (order_by_id_it == orders_by_id.end()) {
        // std::cout << "Could not find order: " << order_id << std::endl;
        return; // Return immediately if not found
    }

    if (new_quantity <= 0) {
        std::cout << "Quantity must be positive. Cancelling order " << order_id << " instead." << std::endl;
        cancel_order(order_id);
        return;
    }

    auto& order_ptr = order_by_id_it->second;
    auto diff = new_quantity - order_ptr->quantity;
    order_ptr->quantity = new_quantity;

    size_t idx = price_to_index(order_ptr->price);
    price_levels[idx].total_quantity += diff;
}