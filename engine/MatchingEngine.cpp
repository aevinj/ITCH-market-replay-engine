#include "MatchingEngine.h"
#include <iostream>

// --- Public API ---
void MatchingEngine::submitLimit(int64_t order_id, OrderSide side, double price, int32_t qty)
{
    // Forward the order to the LOB, passing the trade callback
    book_.process_order(order_id, price, qty, side,
                        [&](const Order &taker, const Order &maker, double trade_price, int32_t trade_qty)
                        {
                            if (onTrade_)
                            {
                                TradeEvent ev{taker.order_id, maker.order_id, trade_price, trade_qty};
                                onTrade_(ev);
                            }
                        });
}

void MatchingEngine::cancel(int64_t order_id)
{
    book_.cancel_order(order_id);
}

void MatchingEngine::reduce_order(int64_t order_id, int32_t cancelled_shares)
{
    book_.reduce_order(order_id, cancelled_shares);
}

void MatchingEngine::order_replace(int64_t old_order_id, int64_t new_order_id, double price, int32_t qty) {
    auto side_opt = book_.get_side(old_order_id);
    if (!side_opt.has_value()) {
        return;
    }

    OrderSide old_side = side_opt.value();
    book_.cancel_order(old_order_id);

    submitLimit(new_order_id, old_side, price, qty);

}