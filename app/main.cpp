#include "LimitOrderBook.h"
#include "MatchingEngine.h" 
#include <fstream>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>

using namespace std;

int main() {
    ifstream file("../../12302019.NASDAQ_ITCH50", ios::binary);
    std::ofstream trade_file("aapl_trades.csv");
    trade_file << "seq,taker,maker,price,quantity" << endl;
    static uint64_t trade_seq = 0;

    if (!file) {
        cerr << "Failed to open the required ITCH file" << endl;
        return 1;
    }

    // TODO preprocess to find automatic min_price and max_price

    // hardcode for now
    LimitOrderBook lob(200, 350);
    MatchingEngine engine(lob);

    engine.setTradeCallback([&](const TradeEvent& ev) {
        std::cout << "TRADE taker=" << ev.taker_id
                << " maker=" << ev.maker_id
                << " qty="   << ev.quantity
                << " px="    << ev.price << '\n';

        trade_seq++;

        trade_file << trade_seq << ","
                << ev.taker_id << ","
                << ev.maker_id << ","
                << ev.price << ","
                << ev.quantity << endl;
    });


    uint16_t appl_locate_code;
    bool appl_loc_found = false;

    while (file) {
    // for (int i = 0; i < 10000000 && file; ++i) {
        uint16_t length;
        file.read(reinterpret_cast<char*>(&length), sizeof(length));

        // Big Endian conversion
        length = (length >> 8) | (length << 8);

        char type;
        file.read(&type, 1);

        // char payload[length - 1];
        vector<char> payload(length - 1);

        file.read(payload.data(), payload.size());

        uint16_t stock_locate = (payload[0] << 8) | payload[1];

        if (type == 'R' && !appl_loc_found) {
            char symbol[9]{};
            memcpy(symbol, payload.data() + 10, 8);
            symbol[8] = '\0';

            string stock(symbol);
            while (!stock.empty() && stock.back() == ' ')
                stock.pop_back();

            if (stock == "AAPL") {
                appl_locate_code = stock_locate;
                appl_loc_found = true;
            }
            continue;
        } 

        if (appl_loc_found && stock_locate == appl_locate_code) {
            // cout << "curr type: " << type << endl;
            if (type == 'A') {
                uint64_t order_id = 0;
                for (int i = 0; i < 8; i++)
                    order_id = (order_id << 8) | (unsigned char)payload[10 + i];

                OrderSide side = payload[18] == 'B' ? OrderSide::Buy : OrderSide::Sell;

                uint32_t shares =
                    ((unsigned char)payload[19] << 24) |
                    ((unsigned char)payload[20] << 16) |
                    ((unsigned char)payload[21] << 8) |
                    ((unsigned char)payload[22]);

                uint32_t price_raw =
                    ((unsigned char)payload[31] << 24) |
                    ((unsigned char)payload[32] << 16) |
                    ((unsigned char)payload[33] << 8) |
                    ((unsigned char)payload[34]);

                double price = price_raw / 10000.0;

                engine.submitLimit(order_id, side, price, shares);
            }
            else if (type == 'D') {
                uint64_t order_id = 0;
                for (int i = 0; i < 8; i++)
                    order_id = (order_id << 8) | (unsigned char)payload[10 + i];

                engine.cancel(order_id);
            }
            else if (type == 'X') {
                uint64_t order_id = 0;
                for (int i = 0; i < 8; i++)
                    order_id = (order_id << 8) | (unsigned char)payload[10 + i];

                uint32_t cancelled_shares = 0;
                for (int i = 0; i < 4; i++) {
                    cancelled_shares = (cancelled_shares << 8) | (unsigned char)payload[18 + i];
                }

                engine.reduce_order(order_id, cancelled_shares);
            }
            else if (type == 'U') {
                uint64_t old_order_id = 0;
                for (int i = 0; i < 8; i++)
                    old_order_id = (old_order_id << 8) | (unsigned char)payload[10 + i];

                uint64_t new_order_id = 0;
                for (int i = 0; i < 8; i++)
                    new_order_id = (new_order_id << 8) | (unsigned char)payload[18 + i];

                uint32_t qty = 0;
                for (int i = 0; i < 4; i++) {
                    qty = (qty << 8) | (unsigned char)payload[26 + i];
                }

                uint32_t price = 0;
                for (int i = 0; i < 4; i++) {
                    price = (price << 8) | (unsigned char)payload[30 + i];
                }

                double px = price / 10000.0;
                engine.order_replace(old_order_id, new_order_id, px, qty);
            }
            else if (type == 'E') {
                uint64_t order_id = 0;
                for (int i = 0; i < 8; i++)
                    order_id = (order_id << 8) | (unsigned char)payload[10 + i];

                uint32_t executed_shares = 0;
                for (int i = 0; i < 4; i++) {
                    executed_shares = (executed_shares << 8) | (unsigned char)payload[18 + i];
                }

                engine.reduce_order(order_id, executed_shares);
            }
        }
    }

    return 0;
}
