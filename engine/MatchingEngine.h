#pragma once

#include <functional>
#include <cstdint>
#include "LimitOrderBook.h"

struct TradeEvent {
    int64_t taker_id;
    int64_t maker_id;
    double  price;
    int32_t quantity;
};

class MatchingEngine {
public:
    using TradeCallback = std::function<void(const TradeEvent&)>;

    explicit MatchingEngine(LimitOrderBook& book)
        : book_(book) {}

    void setTradeCallback(TradeCallback cb) { onTrade_ = std::move(cb); }

    void submitLimit(int64_t order_id, OrderSide side, double price, int32_t qty);
    void cancel(int64_t order_id);
    void modify(int64_t order_id, int32_t new_qty);

private:
    LimitOrderBook& book_;
    TradeCallback onTrade_;
};
