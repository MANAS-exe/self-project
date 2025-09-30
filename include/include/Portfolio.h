#pragma once
#include <string>
#include <unordered_map>
#include <deque>
#include <vector>
#include <iostream>
#include <iomanip>

#include "Database.h"
#include "ApiClient.h"

// Finance-focused portfolio with FIFO accounting
class Portfolio {
public:
    struct Lot { int qty; double cost; };

    struct Holding {
        std::string ticker;
        int quantity = 0;
        double avg_buy_price = 0.0;
        double last_price = 0.0;
        double pnl = 0.0;
        double pnl_percent = 0.0;
    };

    Portfolio(Database& db, ApiClient* api);

    // trades
    bool buy(const std::string& ticker, int qty, double price);
    bool sell(const std::string& ticker, int qty, double price, std::string& reason);

    // price
    Quote refreshPriceFromAPI(const std::string& ticker);

    // analytics
    std::vector<Holding> compute_holdings() const;
    double total_portfolio_value() const;
    double total_unrealized_pnl() const;
    double total_realized_pnl() const;

    // helper to list transactions
    std::vector<Transaction> list_transactions(const std::optional<std::string>& ticker = std::nullopt) const;
    std::vector<std::string> getAllStocks() const;
private:
    Database& db_;
    ApiClient* api_;

    // realized PnL stored in DB? We'll compute cumulative from transactions when needed.
    static void fifo_consume(std::deque<Lot>& lots, int sellQty, double& realized, double sellPrice);
    static double compute_realized_for_symbol(const std::vector<Transaction>& txs);
};
