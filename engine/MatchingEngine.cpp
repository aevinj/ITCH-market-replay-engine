#include "MatchingEngine.h"
#include <iostream>

// --- Public API ---
void MatchingEngine::submitLimit(int64_t order_id,
                                 OrderSide side,
                                 double price,
                                 int32_t qty)
{
    // Forward the order to the LOB, passing the trade callback
    book_.process_order(
        order_id,
        price,
        qty,
        side,
        [&](const Order &taker,
            const Order &maker,
            double trade_price,
            int32_t trade_qty)
        {
            if (onTrade_)
            {
                TradeEvent ev{
                    taker.order_id,
                    maker.order_id,
                    trade_price,
                    trade_qty};
                onTrade_(ev);
            }
        });
}

void MatchingEngine::cancel(int64_t order_id)
{
    book_.cancel_order(order_id);
}

void MatchingEngine::modify(int64_t order_id, int32_t new_qty)
{
    book_.modify_order(order_id, new_qty);
}
