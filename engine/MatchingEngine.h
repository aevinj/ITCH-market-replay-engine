#pragma once

#include "LimitOrderBook.h"
#include <functional>
#include <cstdint>
#include <memory>

struct TradeEvent {
    int64_t taker_id;
    int64_t maker_id;
    double  price;
    int32_t quantity;
};

class MatchingEngine {
public:
    using TradeCallback = std::function<void(const TradeEvent&)>;

    explicit MatchingEngine(std::unique_ptr<LimitOrderBook> book)
        : book_(std::move(book)) {};

    void setTradeCallback(TradeCallback cb) { onTrade_ = std::move(cb); }

    void submitLimit(int64_t order_id, OrderSide side, double price, int32_t qty);
    void cancel(int64_t order_id);
    void reduce_order(int64_t order_id, int32_t cancelled_shares);
    void order_replace(int64_t old_order_id, int64_t new_order_id, double price, int32_t qty);
    std::unique_ptr<LimitOrderBook>& get_book() { return book_; };

private:
    std::unique_ptr<LimitOrderBook> book_;
    TradeCallback onTrade_;
};
