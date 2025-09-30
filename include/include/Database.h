#pragma once
#include <string>
#include <vector>
#include <optional>
#include <iostream>
#include <sqlite3.h>

struct Transaction {
    int id = 0;
    std::string ticker;
    std::string type; // "BUY" or "SELL"
    int quantity = 0;
    double price = 0.0;
    std::string timestamp;
};

struct StockRow {
    int id = 0;
    std::string ticker;
    std::string company;
    double last_price = 0.0;
};

class Database {
public:
    explicit Database(const std::string& path = "portfolio.db");
    ~Database();

    void open();
    void close();

    void migrate(); // create schema if needed
    void seed_sample_stocks();

    // Stocks
    std::vector<StockRow> get_all_stocks() const;
    std::optional<int> find_stock_id_by_ticker(const std::string& ticker) const;
    bool ensure_stock(const std::string& ticker, const std::string& company = "");
    bool update_stock_price_by_ticker(const std::string& ticker, double price);
    std::optional<double> get_last_price(const std::string& ticker) const;

    // Transactions
    bool insert_transaction(const std::string& ticker, const std::string& type, int qty, double price);
    std::vector<Transaction> get_transactions(const std::optional<std::string>& ticker = std::nullopt) const;
    int get_net_quantity_for_stock(const std::string& ticker) const;

private:
    std::string dbPath_;
    sqlite3* db_ = nullptr;

    static void check_rc(int rc, sqlite3* db, const char* msg = nullptr);
};
