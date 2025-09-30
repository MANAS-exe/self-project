#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

enum class TxType { BUY, SELL };

struct Transaction {
    int             id = 0;
    std::string     symbol;      // e.g. AAPL
    TxType          type = TxType::BUY;
    int             quantity = 0;
    double          price = 0.0; // per share
    std::string     timestamp;   // ISO-8601 string from SQLite DEFAULT CURRENT_TIMESTAMP
};

struct Holding {
    std::string symbol;
    int         quantity = 0;        // remaining shares (after FIFO matching)
    double      avg_cost = 0.0;      // average cost of remaining lots
    double      last_price = 0.0;    // latest price stored in DB
    double      cost_basis = 0.0;    // quantity * avg_cost
    double      market_value = 0.0;  // quantity * last_price
    double      unrealized_pnl = 0.0;// (last_price - avg_cost) * quantity
    double      pnl_percent = 0.0;   // unrealized_pnl / cost_basis * 100 (if cost_basis>0)
};

struct Quote {
    std::string symbol;
    double      price = 0.0;
    std::string asOf;   // from API payload
    std::string source; // "GLOBAL_QUOTE" | "TIME_SERIES_DAILY" | "cached"
};
