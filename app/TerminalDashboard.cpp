#include "TerminalDashboard.h"
#include <sstream>

TerminalDashboard::TerminalDashboard(const std::vector<std::string>& symbols)
    : symbols_(symbols)
{
    for (const auto& s : symbols_) {
        views_.emplace(s, InstrumentView{});
    }
}

void TerminalDashboard::clearScreen() const {
    // ANSI clear + move cursor to home
    std::cout << "\x1b[2J\x1b[H";
}

void TerminalDashboard::updateBook(const std::string& symbol,
                                   double bid_px, int32_t bid_qty,
                                   double ask_px, int32_t ask_qty,
                                   bool bid_valid, bool ask_valid)
{
    auto it = views_.find(symbol);
    if (it == views_.end()) return;

    auto& v = it->second;
    if (bid_valid) {
        v.best_bid_price = bid_px;
        v.best_bid_qty   = bid_qty;
        v.has_bid        = true;
    } else {
        v.has_bid = false;
        v.best_bid_price = 0.0;
        v.best_bid_qty   = 0;
    }

    if (ask_valid) {
        v.best_ask_price = ask_px;
        v.best_ask_qty   = ask_qty;
        v.has_ask        = true;
    } else {
        v.has_ask = false;
        v.best_ask_price = 0.0;
        v.best_ask_qty   = 0;
    }
}

void TerminalDashboard::updateTrade(const std::string& symbol,
                                    double trade_price, int32_t trade_qty)
{
    auto it = views_.find(symbol);
    if (it == views_.end()) return;

    auto& v = it->second;
    v.last_trade_price = trade_price;
    v.last_trade_qty   = trade_qty;
    v.has_trade        = true;
}

void TerminalDashboard::render() const {
    clearScreen();

    std::cout << std::left
              << std::setw(8)  << "SYMBOL"
              << std::setw(22) << "BID (px x qty)"
              << std::setw(22) << "ASK (px x qty)"
              << std::setw(22) << "LAST TRADE"
              << "\n";

    std::cout << std::string(8 + 22 + 22 + 22, '-') << "\n";

    for (const auto& sym : symbols_) {
        auto it = views_.find(sym);
        if (it == views_.end()) continue;
        const auto& v = it->second;

        std::cout << std::left << std::setw(8) << sym;

        // BID
        if (v.has_bid) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2)
               << v.best_bid_price << " x " << v.best_bid_qty;
            std::cout << std::setw(22) << ss.str();
        } else {
            std::cout << std::setw(22) << "-";
        }

        // ASK
        if (v.has_ask) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2)
               << v.best_ask_price << " x " << v.best_ask_qty;
            std::cout << std::setw(22) << ss.str();
        } else {
            std::cout << std::setw(22) << "-";
        }

        // LAST TRADE
        if (v.has_trade) {
            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2)
               << v.last_trade_price << " x " << v.last_trade_qty;
            std::cout << std::setw(22) << ss.str();
        } else {
            std::cout << std::setw(22) << "-";
        }

        std::cout << "\n";
    }

    std::cout.flush();
}
