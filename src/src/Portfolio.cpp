#include "Portfolio.h"
#include <algorithm>
#include <stdexcept>

Portfolio::Portfolio(Database& db, ApiClient* api) : db_(db), api_(api) {}

// buy
bool Portfolio::buy(const std::string& ticker, int qty, double price) {
    if (qty <= 0 || price < 0.0) {
        std::cerr << "[Portfolio] invalid buy input\n";
        return false;
    }
    // ensure stock exists in DB
    db_.ensure_stock(ticker);
    bool ok = db_.insert_transaction(ticker, "BUY", qty, price);
    if (!ok) return false;
    db_.update_stock_price_by_ticker(ticker, price);
    std::cout << "[Portfolio] BUY recorded: " << ticker << " x" << qty << " @ " << price << "\n";
    return true;
}

// sell
bool Portfolio::sell(const std::string& ticker, int qty, double price, std::string& reason) {
    if (qty <= 0 || price < 0.0) {
        reason = "invalid qty/price";
        return false;
    }
    db_.ensure_stock(ticker);
    int net = db_.get_net_quantity_for_stock(ticker);
    if (net < qty) {
        std::ostringstream os; os << "not enough shares: have " << net;
        reason = os.str();
        return false;
    }
    bool ok = db_.insert_transaction(ticker, "SELL", qty, price);
    if (!ok) { reason = "db insert failed"; return false; }
    db_.update_stock_price_by_ticker(ticker, price);
    std::cout << "[Portfolio] SELL recorded: " << ticker << " x" << qty << " @ " << price << "\n";
    return true;
}

Quote Portfolio::refreshPriceFromAPI(const std::string& ticker) {
    if (!api_) throw std::runtime_error("API client not configured");
    Quote q = api_->fetchLatestQuote(ticker);
    if (q.price > 0.0) {
        db_.ensure_stock(ticker);
        db_.update_stock_price_by_ticker(ticker, q.price);
    }
    return q;
}

void Portfolio::fifo_consume(std::deque<Lot>& lots, int sellQty, double& realized, double sellPrice) {
    int remaining = sellQty;
    while (remaining > 0 && !lots.empty()) {
        Lot &lot = lots.front();
        int take = std::min(remaining, lot.qty);
        realized += (sellPrice - lot.cost) * take;
        lot.qty -= take;
        remaining -= take;
        std::cout << "[FIFO] Consumed " << take << " @ cost " << lot.cost << " (sell " << sellPrice << ") -> PnL segment "
                  << (sellPrice - lot.cost) * take << "\n";
        if (lot.qty == 0) lots.pop_front();
    }
    if (remaining > 0) {
        std::cerr << "[FIFO] Warning: tried to consume more than available\n";
    }
}

double Portfolio::compute_realized_for_symbol(const std::vector<Transaction>& txs) {
    std::deque<Lot> lots;
    double realized = 0.0;
    for (const auto& t : txs) {
        if (t.type == "BUY") {
            lots.push_back({t.quantity, t.price});
        } else {
            fifo_consume(lots, t.quantity, realized, t.price);
        }
    }
    return realized;
}

std::vector<Portfolio::Holding> Portfolio::compute_holdings() const {
    std::vector<Holding> out;
    // get all transactions grouped by ticker
    auto txs = db_.get_transactions(std::nullopt);
    std::unordered_map<std::string, std::vector<Transaction>> bySym;
    for (const auto& t : txs) bySym[t.ticker].push_back(t);

    for (auto& kv : bySym) {
        const auto& ticker = kv.first;
        auto& v = kv.second;

        // compute remaining lots via FIFO
        std::deque<Lot> lots;
        for (const auto& t : v) {
            if (t.type == "BUY") lots.push_back({t.quantity, t.price});
            else fifo_consume(lots, t.quantity, /*ignored*/ *(new double(0.0)), t.price); // we will recompute realized separately
        }

        int netQty = 0;
        double totalCost = 0.0;
        for (const auto& l : lots) { netQty += l.qty; totalCost += l.qty * l.cost; }
        if (netQty == 0) continue;

        auto optPrice = db_.get_last_price(ticker);
        double last = optPrice.value_or(0.0);
        Holding h;
        h.ticker = ticker;
        h.quantity = netQty;
        h.avg_buy_price = (netQty > 0) ? (totalCost / netQty) : 0.0;
        h.last_price = last;
        h.pnl = (h.last_price - h.avg_buy_price) * h.quantity;
        h.pnl_percent = (h.avg_buy_price > 0.0) ? ((h.last_price - h.avg_buy_price) / h.avg_buy_price * 100.0) : 0.0;
        out.push_back(h);
    }
    // sort by market value desc
    std::sort(out.begin(), out.end(), [](const Holding& a, const Holding& b){
        return a.last_price * a.quantity > b.last_price * b.quantity;
    });
    return out;
}

double Portfolio::total_portfolio_value() const {
    double total = 0.0;
    auto h = compute_holdings();
    for (const auto& x : h) total += x.last_price * x.quantity;
    return total;
}

double Portfolio::total_unrealized_pnl() const {
    double total = 0.0;
    auto h = compute_holdings();
    for (const auto& x : h) total += x.pnl;
    return total;
}

std::vector<std::string> Portfolio::getAllStocks() const {
    std::vector<std::string> tickers;
    for (const auto& row : db_.get_all_stocks()) {
        tickers.push_back(row.ticker);  // assuming StockRow has a field "ticker"
    }
    return tickers;
}



double Portfolio::total_realized_pnl() const {
    // compute realized PnL by summing FIFO realized for each ticker
    double total = 0.0;
    auto txs = db_.get_transactions(std::nullopt);
    std::unordered_map<std::string, std::vector<Transaction>> bySym;
    for (const auto& t : txs) bySym[t.ticker].push_back(t);
    for (auto& kv : bySym) total += compute_realized_for_symbol(kv.second);
    return total;
}

std::vector<Transaction> Portfolio::list_transactions(const std::optional<std::string>& ticker) const {
    return db_.get_transactions(ticker);
}
