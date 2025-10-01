#ifndef ORDERBOOK_ORDER_H
#define ORDERBOOK_ORDER_H

#include <cstdint>

enum class OrderSide {
    Buy,
    Sell
};

struct Order {
    int64_t order_id;
    int64_t price;
    int32_t quantity;
    OrderSide side;
};

#endif // ORDERBOOK_ORDER_H