// TerminalDashboard.h
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <iomanip>
#include <cstdint>

class TerminalDashboard {
public:
    struct InstrumentView {
        double best_bid_price = 0.0;
        int32_t best_bid_qty   = 0;
        double best_ask_price = 0.0;
        int32_t best_ask_qty   = 0;
        double last_trade_price = 0.0;
        int32_t last_trade_qty  = 0;
        bool has_bid = false;
        bool has_ask = false;
        bool has_trade = false;
    };

    explicit TerminalDashboard(const std::vector<std::string>& symbols);

    // Update from LOB
    void updateBook(const std::string& symbol,
                    double bid_px, int32_t bid_qty,
                    double ask_px, int32_t ask_qty,
                    bool bid_valid, bool ask_valid);

    // Update when a trade occurs
    void updateTrade(const std::string& symbol,
                     double trade_price, int32_t trade_qty);

    // Redraw the whole table
    void render() const;

private:
    std::vector<std::string> symbols_;
    std::unordered_map<std::string, InstrumentView> views_;

    void clearScreen() const;
};
