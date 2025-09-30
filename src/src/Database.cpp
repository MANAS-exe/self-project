#include "Database.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

Database::Database(const std::string& path) : dbPath_(path), db_(nullptr) {}

Database::~Database() { close(); }

void Database::check_rc(int rc, sqlite3* db, const char* msg) {
    if (rc != SQLITE_OK && rc != SQLITE_DONE && rc != SQLITE_ROW) {
        std::string err = db ? sqlite3_errmsg(db) : "unknown";
        std::ostringstream os;
        if (msg) os << msg << " - ";
        os << "SQLite error: " << err;
        throw std::runtime_error(os.str());
    }
}

void Database::open() {
    if (db_) return;
    int rc = sqlite3_open(dbPath_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::string e = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Cannot open DB: " + e);
    }
    migrate();
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void Database::migrate() {
    const char* sql = R"SQL(
CREATE TABLE IF NOT EXISTS stocks (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  ticker TEXT UNIQUE NOT NULL,
  company TEXT DEFAULT '',
  last_price REAL DEFAULT 0.0
);

CREATE TABLE IF NOT EXISTS transactions (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  stock_id INTEGER NOT NULL,
  type TEXT NOT NULL CHECK(type IN ('BUY','SELL')),
  quantity INTEGER NOT NULL CHECK(quantity>0),
  price REAL NOT NULL CHECK(price>=0),
  timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY(stock_id) REFERENCES stocks(id)
);
)SQL";
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string em = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("migrate failed: " + em);
    }
}

void Database::seed_sample_stocks() {
    const char* sql = "INSERT OR IGNORE INTO stocks(ticker,company,last_price) VALUES (?,?,?,?,?,?,?,?,?);";
    // We'll insert via prepared statements but simpler: just insert with multiple VALUES
    const char* sql2 =
        "INSERT OR IGNORE INTO stocks(ticker,company,last_price) VALUES "
        "('AAPL','Apple Inc.',0.0),('MSFT','Microsoft Corporation',0.0),"
        "('GOOGL','Alphabet Inc.',0.0),('TSLA','Tesla Inc.',0.0),"
        "('AMZN','Amazon.com Inc.',0.0),('NVDA','NVIDIA Corporation',0.0),"
        "('META','Meta Platforms Inc.',0.0),('NFLX','Netflix Inc.',0.0);";
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql2, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        std::string em = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("seed failed: " + em);
    }
}

std::vector<StockRow> Database::get_all_stocks() const {
    std::vector<StockRow> out;
    const char* sql = "SELECT id,ticker,company,last_price FROM stocks ORDER BY ticker;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        StockRow s;
        s.id = sqlite3_column_int(stmt,0);
        s.ticker = reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
        s.company = reinterpret_cast<const char*>(sqlite3_column_text(stmt,2));
        s.last_price = sqlite3_column_double(stmt,3);
        out.push_back(s);
    }
    sqlite3_finalize(stmt);
    return out;
}

std::optional<int> Database::find_stock_id_by_ticker(const std::string& ticker) const {
    const char* sql = "SELECT id FROM stocks WHERE ticker = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
    sqlite3_bind_text(stmt,1,ticker.c_str(),-1,SQLITE_TRANSIENT);
    std::optional<int> out;
    if (sqlite3_step(stmt) == SQLITE_ROW) out = sqlite3_column_int(stmt,0);
    sqlite3_finalize(stmt);
    return out;
}

bool Database::ensure_stock(const std::string& ticker, const std::string& company) {
    auto id = find_stock_id_by_ticker(ticker);
    if (id) return true;
    const char* ins = "INSERT INTO stocks(ticker,company,last_price) VALUES(?,?,0.0);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, ins, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
    sqlite3_bind_text(stmt,1,ticker.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt,2,company.c_str(),-1,SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

bool Database::update_stock_price_by_ticker(const std::string& ticker, double price) {
    const char* upd = "UPDATE stocks SET last_price = ? WHERE ticker = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, upd, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
    sqlite3_bind_double(stmt,1,price);
    sqlite3_bind_text(stmt,2,ticker.c_str(),-1,SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::optional<double> Database::get_last_price(const std::string& ticker) const {
    const char* sel = "SELECT last_price FROM stocks WHERE ticker = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sel, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
    sqlite3_bind_text(stmt,1,ticker.c_str(),-1,SQLITE_TRANSIENT);
    std::optional<double> out;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt,0) != SQLITE_NULL) out = sqlite3_column_double(stmt,0);
    }
    sqlite3_finalize(stmt);
    return out;
}

bool Database::insert_transaction(const std::string& ticker, const std::string& type, int qty, double price) {
    // find stock id
    auto sid = find_stock_id_by_ticker(ticker);
    if (!sid) return false;
    const char* ins = "INSERT INTO transactions(stock_id,type,quantity,price) VALUES(?,?,?,?);";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, ins, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
    sqlite3_bind_int(stmt,1,*sid);
    sqlite3_bind_text(stmt,2,type.c_str(),-1,SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt,3,qty);
    sqlite3_bind_double(stmt,4,price);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return rc == SQLITE_DONE;
}

std::vector<Transaction> Database::get_transactions(const std::optional<std::string>& ticker) const {
    std::vector<Transaction> out;
    std::string sql =
        "SELECT t.id, s.ticker, t.type, t.quantity, t.price, t.timestamp "
        "FROM transactions t JOIN stocks s ON t.stock_id = s.id ";
    if (ticker) sql += "WHERE s.ticker = ? ";
    sql += "ORDER BY t.id ASC;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
    if (ticker) sqlite3_bind_text(stmt,1,ticker->c_str(),-1,SQLITE_TRANSIENT);
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Transaction t;
        t.id = sqlite3_column_int(stmt,0);
        t.ticker = reinterpret_cast<const char*>(sqlite3_column_text(stmt,1));
        t.type = reinterpret_cast<const char*>(sqlite3_column_text(stmt,2));
        t.quantity = sqlite3_column_int(stmt,3);
        t.price = sqlite3_column_double(stmt,4);
        t.timestamp = reinterpret_cast<const char*>(sqlite3_column_text(stmt,5));
        out.push_back(t);
    }
    sqlite3_finalize(stmt);
    return out;
}

int Database::get_net_quantity_for_stock(const std::string& ticker) const {
    const char* sql =
        "SELECT SUM(CASE WHEN t.type='BUY' THEN t.quantity ELSE -t.quantity END) "
        "FROM transactions t JOIN stocks s ON t.stock_id = s.id WHERE s.ticker = ?;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db_));
    sqlite3_bind_text(stmt,1,ticker.c_str(),-1,SQLITE_TRANSIENT);
    int net = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt,0) != SQLITE_NULL) net = sqlite3_column_int(stmt,0);
    }
    sqlite3_finalize(stmt);
    return net;
}
