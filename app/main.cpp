#include <iostream>
#include <iomanip>
#include "LimitOrderBook.h"

// Helpers to work with your fixed ladder
static constexpr double MIN_PRICE = 90.0;
static constexpr double TICK = 0.01;
static double index_to_price(std::size_t idx) { return MIN_PRICE + idx * TICK; }

// Pretty print the book by scanning all price levels.
// We’ll show levels that have any quantity, and list orders with side.
void print_book(const LimitOrderBook& lob) {
    const auto& levels = lob.get_price_levels();

    std::cout << "\n--- Order Book (non-empty levels) ---\n";
    std::cout << std::fixed << std::setprecision(2);

    for (std::size_t i = 0; i < levels.size(); ++i) {
        const auto& pl = levels[i];
        if (pl.total_quantity == 0 || pl.orders.empty()) continue;

        double price = index_to_price(i);

        int32_t bid_qty = 0, ask_qty = 0;
        // Split quantities by side (helpful visual)
        for (auto* o : pl.orders) {
            if (o->side == OrderSide::Buy) bid_qty += o->quantity;
            else ask_qty += o->quantity;
        }

        std::cout << "Price: " << price
                  << " | Total: " << pl.total_quantity
                  << " | Bids@" << price << ": " << bid_qty
                  << " | Asks@" << price << ": " << ask_qty
                  << " | Orders: ";
        for (auto* o : pl.orders) {
            char s = (o->side == OrderSide::Buy ? 'B' : 'S');
            std::cout << "(" << s << "#" << o->order_id << ", " << o->quantity << ") ";
        }
        std::cout << "\n";
    }
}

int main() {
    LimitOrderBook lob;

    std::cout << "=== Add initial orders ===\n";
    lob.process_order(1, 100.00, 100, OrderSide::Buy);   // bid @ 100.00
    lob.process_order(2, 101.00,  50, OrderSide::Buy);   // bid @ 101.00
    lob.process_order(3, 102.00,  75, OrderSide::Sell);  // ask @ 102.00
    lob.process_order(4, 103.00, 120, OrderSide::Sell);  // ask @ 103.00
    print_book(lob);

    std::cout << "\n=== Add crossing order (Buy 80 @ 103.00) ===\n";
    // Should match fully with 75 @ 102.00 and 5 with 103.00
    lob.process_order(5, 103.00, 80, OrderSide::Buy);
    print_book(lob);

    std::cout << "\n=== Add crossing order (Sell 120 @ 100.00) ===\n";
    // Should hit 101.00 (50) then 100.00 (70), leaving 30 @ 100.00
    lob.process_order(6, 100.00, 120, OrderSide::Sell);
    print_book(lob);

    std::cout << "\n=== Cancel an order (order 4 if still alive) ===\n";
    lob.cancel_order(4);
    print_book(lob);

    std::cout << "\n=== Modify an order (reduce order 1 to 50 if still alive) ===\n";
    lob.modify_order(1, 50);
    print_book(lob);

    std::cout << "\n=== Modify an order (increase order 2 to 200 if still alive) ===\n";
    lob.modify_order(2, 200);
    print_book(lob);

    return 0;
}
