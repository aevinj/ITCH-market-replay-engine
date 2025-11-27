// #include <iostream>
// #include <iomanip>
// #include "LimitOrderBook.h"
// #include "MatchingEngine.h" 

// // Helpers to work with fixed ladder
// static constexpr double MIN_PRICE = 90.0;
// static constexpr double TICK = 0.01;
// static double index_to_price(std::size_t idx) { return MIN_PRICE + idx * TICK; }

// // Pretty print the book by scanning all price levels.
// // show levels that have any quantity, and list orders with side.
// void print_book(const LimitOrderBook& lob) {
//     const auto& levels = lob.get_price_levels();

//     std::cout << "\n--- Order Book (non-empty levels) ---\n";
//     std::cout << std::fixed << std::setprecision(2);

//     for (std::size_t i = 0; i < levels.size(); ++i) {
//         const auto& pl = levels[i];
//         if (pl.total_quantity == 0 || pl.orders.empty()) continue;

//         double price = index_to_price(i);

//         int32_t bid_qty = 0, ask_qty = 0;
//         // Split quantities by side (helpful visual)
//         for (auto* o : pl.orders) {
//             if (o->side == OrderSide::Buy) bid_qty += o->quantity;
//             else ask_qty += o->quantity;
//         }

//         std::cout << "Price: " << price
//                   << " | Total: " << pl.total_quantity
//                   << " | Bids@" << price << ": " << bid_qty
//                   << " | Asks@" << price << ": " << ask_qty
//                   << " | Orders: ";
//         for (auto* o : pl.orders) {
//             char s = (o->side == OrderSide::Buy ? 'B' : 'S');
//             std::cout << "(" << s << "#" << o->order_id << ", " << o->quantity << ") ";
//         }
//         std::cout << "\n";
//     }
// }

// void printTrade(const TradeEvent& ev) {
//     std::cout << "TRADE: taker#" << ev.taker_id
//               << " maker#" << ev.maker_id
//               << " price=" << ev.price
//               << " qty="   << ev.quantity
//               << "\n";
// }

// int main() {
//     LimitOrderBook lob;
//     MatchingEngine engine(lob);

//     engine.setTradeCallback(printTrade);

//     engine.submitLimit(1, OrderSide::Buy, 100.00, 100);
//     engine.submitLimit(2, OrderSide::Buy, 101.00, 50);
//     engine.submitLimit(3, OrderSide::Sell, 102.00, 75);
//     engine.submitLimit(4, OrderSide::Sell, 103.00, 120);

//     std::cout << "\n=== Add crossing order (Buy 80 @ 103) ===\n";
//     engine.submitLimit(5, OrderSide::Buy, 103.00, 80);

//     return 0;

// }
