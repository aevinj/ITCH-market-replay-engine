#include "LimitOrderBook.h"
#include "MatchingEngine.h"
#include "TerminalDashboard.h"

#include <fstream>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <memory>

using namespace std;

int main()
{
    ifstream file("../../12302019.NASDAQ_ITCH50", ios::binary);
    std::ofstream trade_file("trades.csv");
    trade_file << "seq,symbol,taker,maker,price,quantity" << endl;
    static uint64_t trade_seq = 0;

    if (!file)
    {
        cerr << "Failed to open the required ITCH file" << endl;
        return 1;
    }

    TerminalDashboard dashboard({
        "AAPL","MSFT","AMZN","GOOGL","META","NVDA","TSLA","ORCL","INTC","AMD",
        "JPM","BAC","GS","MS","WMT","COST","TGT","NFLX","DIS","NKE"
    });

    const unordered_set<string> tracked_symbols = {
        "AAPL", "MSFT", "AMZN", "GOOGL", "META", "NVDA", "TSLA", "ORCL", "INTC", "AMD",
        "JPM", "BAC", "GS", "MS", "WMT", "COST", "TGT", "NFLX", "DIS", "NKE"};

    unordered_map<uint16_t, string> locate_to_symbol;

    unordered_map<uint16_t, unique_ptr<MatchingEngine>> locate_to_engine;
    size_t tracked_symbols_count = 0;

    static int buy_adds = 0;
    static int sell_adds = 0;

    while (file)
    {
        uint16_t length_be;
        if (!file.read(reinterpret_cast<char *>(&length_be), sizeof(length_be)))
        {
            break; // EOF or error
        }

        uint16_t length = (length_be >> 8) | (length_be << 8);
        if (length < 1)
        {
            std::cerr << "Invalid length: " << length << "\n";
            break;
        }

        char type;
        if (!file.read(&type, 1))
        {
            cerr << "Failed to read type\n";
            break;
        }

        // not using char array due to VLA (variable length assignment - silent error)
        vector<char> payload(length - 1);
        if (!file.read(payload.data(), payload.size()))
        {
            cerr << "Failed to read payload, len=" << (length - 1) << "\n";
            break;
        }

        uint16_t stock_locate = (payload[0] << 8) | payload[1];

        if (type == 'R')
        {
            char symbol[9]{};
            memcpy(symbol, payload.data() + 10, 8);
            symbol[8] = '\0';

            string stock(symbol);
            while (!stock.empty() && stock.back() == ' ')
                stock.pop_back();

            if (stock.empty() || !tracked_symbols.count(stock) || locate_to_engine.count(stock_locate))
                continue;

            char market_category = payload[18];
            char financial_status = payload[19];
            char issue_classification = payload[25];
            char authenticity = payload[28];

            // filter: production, common stock, normal financial status
            if (authenticity != 'P')
                continue;
            if (issue_classification != 'C')
                continue;
            if (financial_status != 'N' && financial_status != ' ')
                continue;

            locate_to_symbol[stock_locate] = stock;

            auto lob = std::make_unique<LimitOrderBook>(0, 10000);
            auto engine_uptr = std::make_unique<MatchingEngine>(std::move(lob));
            MatchingEngine* engine_raw = engine_uptr.get();

            string stock_copy = stock;
            
            engine_raw->setTradeCallback(
                [&trade_file, stock_copy, &dashboard, engine_raw](const TradeEvent& ev)
                {
                    // seq counter
                    trade_seq++;

                    // write to CSV
                    // trade_file << trade_seq << ","
                    //         << stock_copy << ","
                    //         << ev.taker_id << ","
                    //         << ev.maker_id << ","
                    //         << ev.price << ","
                    //         << ev.quantity << std::endl;

                    // update last trade in dashboard
                    dashboard.updateTrade(stock_copy, ev.price, ev.quantity);

                    // get best bid/ask from this engine's LOB
                    auto bid = engine_raw->get_book()->get_best_bid();
                    auto ask = engine_raw->get_book()->get_best_ask();

                    dashboard.updateBook(
                        stock_copy,
                        bid.price,
                        bid.quantity,
                        ask.price,
                        ask.quantity,
                        bid.valid,
                        ask.valid
                    );

                    dashboard.render();
                }
            );

            locate_to_engine[stock_locate] = move(engine_uptr);
            continue;
        }

        if (locate_to_engine.count(stock_locate))
        {
            unique_ptr<MatchingEngine>& engine = locate_to_engine[stock_locate];
            if (type == 'A')
            {
                uint64_t order_id = 0;
                for (int i = 0; i < 8; i++)
                    order_id = (order_id << 8) | (unsigned char)payload[10 + i];

                OrderSide side = payload[18] == 'B' ? OrderSide::Buy : OrderSide::Sell;
                // system("clear");
                // if (payload[18] == 'B') {
                //     buy_adds++;
                // } else if (payload[18] == 'S') {
                //     sell_adds++;
                // }
                // cout << "buys: " << buy_adds << " and sells: " << sell_adds << endl;

                uint32_t shares =
                    (static_cast<unsigned char>(payload[19]) << 24) |
                    (static_cast<unsigned char>(payload[20]) << 16) |
                    (static_cast<unsigned char>(payload[21]) << 8) |
                    (static_cast<unsigned char>(payload[22]));

                uint32_t price_raw =
                    (static_cast<unsigned char>(payload[31]) << 24) |
                    (static_cast<unsigned char>(payload[32]) << 16) |
                    (static_cast<unsigned char>(payload[33]) << 8) |
                    (static_cast<unsigned char>(payload[34]));

                double price = price_raw / 10000.0;

                engine->submitLimit(order_id, side, price, shares);
            }
            else if (type == 'D')
            {
                uint64_t order_id = 0;
                for (int i = 0; i < 8; i++)
                    order_id = (order_id << 8) | static_cast<unsigned char>(payload[10 + i]);

                engine->cancel(order_id);
            }
            else if (type == 'X')
            {
                uint64_t order_id = 0;
                for (int i = 0; i < 8; i++)
                    order_id = (order_id << 8) | static_cast<unsigned char>(payload[10 + i]);

                uint32_t cancelled_shares = 0;
                for (int i = 0; i < 4; i++)
                {
                    cancelled_shares = (cancelled_shares << 8) | static_cast<unsigned char>(payload[18 + i]);
                }

                engine->reduce_order(order_id, cancelled_shares);
            }
            else if (type == 'U')
            {
                uint64_t old_order_id = 0;
                for (int i = 0; i < 8; i++)
                    old_order_id = (old_order_id << 8) | static_cast<unsigned char>(payload[10 + i]);

                uint64_t new_order_id = 0;
                for (int i = 0; i < 8; i++)
                    new_order_id = (new_order_id << 8) | static_cast<unsigned char>(payload[18 + i]);

                uint32_t qty = 0;
                for (int i = 0; i < 4; i++)
                    qty = (qty << 8) | static_cast<unsigned char>(payload[26 + i]);

                uint32_t price = 0;
                for (int i = 0; i < 4; i++)
                    price = (price << 8) | static_cast<unsigned char>(payload[30 + i]);

                double px = price / 10000.0;
                engine->order_replace(old_order_id, new_order_id, px, qty);
            }
            else if (type == 'E')
            {
                uint64_t order_id = 0;
                for (int i = 0; i < 8; i++)
                    order_id = (order_id << 8) | static_cast<unsigned char>(payload[10 + i]);

                uint32_t executed_shares = 0;
                for (int i = 0; i < 4; i++)
                {
                    executed_shares = (executed_shares << 8) | static_cast<unsigned char>(payload[18 + i]);
                }

                engine->reduce_order(order_id, executed_shares);
            }
        }
    }

    return 0;
}
