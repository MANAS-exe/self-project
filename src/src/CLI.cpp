#include "CLI.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>   // for setw, setprecision
#ifdef _WIN32
#include <windows.h>
#endif

CLI::CLI(Portfolio& pf) : pf_(pf) {
#ifdef _WIN32
    // Enable ANSI escape codes in Windows terminal
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}


std::string CLI::to_upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
    return s;
}
bool CLI::to_int(const std::string& s, int& out) {
    try { size_t p; long v = std::stol(s, &p); if (p!=s.size()) return false; out = (int)v; return true; }
    catch(...) { return false; }
}
bool CLI::to_double(const std::string& s, double& out) {
    try { size_t p; double v = std::stod(s, &p); if (p!=s.size()) return false; out = v; return true; }
    catch(...) { return false; }
}

void CLI::print_help() const {
    std::cout <<
R"(Commands:
  help
  list stocks               # show known tickers and last price
  txs [TICKER]              # list transactions
  buy TICKER QTY PRICE      # record buy
  sell TICKER QTY PRICE     # record sell (validated)
  fetch TICKER              # fetch price from Alpha Vantage and cache
  holdings                  # show holdings & P&L
  value                     # portfolio totals
  exit
)";
}

// --- ASCII border helpers ---
void print_table_border(int col_widths[], int n) {
    std::cout << "+";
    for (int i=0;i<n;i++) {
        for (int j=0;j<col_widths[i];j++) std::cout << "-";
        std::cout << "+";
    }
    std::cout << "\n";
}

void CLI::run() {
    std::cout << "Portfolio CLI. Type 'help'.\n";
    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;
        std::istringstream is(line);
        std::string cmd; is >> cmd;
        if (cmd.empty()) continue;
        if (cmd == "help") { print_help(); continue; }
        if (cmd == "exit" || cmd == "quit") break;

        if (cmd == "list") {
            std::string what; is >> what;
            if (what == "stocks") {
                auto txs = pf_.list_transactions(std::nullopt);
                std::unordered_map<std::string,double> last;
                for (const auto& t : txs) {
                   Quote q = pf_.refreshPriceFromAPI(t.ticker);
                   last[t.ticker] = q.price;
                }

                int widths[] = {10, 12};
                print_table_border(widths, 2);
                std::cout << "|" << std::setw(widths[0]) << "Ticker" 
                          << "|" << std::setw(widths[1]) << "LastPrice" << "|\n";
                print_table_border(widths, 2);

                for (auto& kv : last) {
                    std::cout << "|" << std::setw(widths[0]) << kv.first 
                              << "|" << std::setw(widths[1]) << kv.second << "|\n";
                }
                print_table_border(widths, 2);
            } else {
                std::cout << "usage: list stocks\n";
            }
            continue;
        }

        if (cmd == "txs") {
            std::string ticker; is >> ticker;
            std::optional<std::string> opt;
            if (!ticker.empty()) opt = ticker;
            auto txs = pf_.list_transactions(opt);

            int widths[] = {5,10,10,8,10,20};
            print_table_border(widths, 6);
            std::cout << "|" << std::setw(widths[0]) << "ID"
                      << "|" << std::setw(widths[1]) << "Ticker"
                      << "|" << std::setw(widths[2]) << "Type"
                      << "|" << std::setw(widths[3]) << "Qty"
                      << "|" << std::setw(widths[4]) << "Price"
                      << "|" << std::setw(widths[5]) << "Timestamp" << "|\n";
            print_table_border(widths, 6);

            for (auto& t : txs) {
                std::string type = (t.type=="BUY") ? "\033[32mBUY\033[0m" : "\033[31mSELL\033[0m";
                std::cout << "|" << std::setw(widths[0]) << t.id
                          << "|" << std::setw(widths[1]) << t.ticker
                          << "|" << std::setw(widths[2]) << type
                          << "|" << std::setw(widths[3]) << t.quantity
                          << "|" << std::setw(widths[4]) << t.price
                          << "|" << std::setw(widths[5]) << t.timestamp << "|\n";
            }
            print_table_border(widths, 6);
            continue;
        }

        if (cmd == "buy" || cmd == "sell") {
            std::string ticker, sqty, sprice; is >> ticker >> sqty >> sprice;
            int qty; double price;
            if (ticker.empty() || !to_int(sqty, qty) || !to_double(sprice, price)) {
                std::cout << "usage: " << cmd << " TICKER QTY PRICE\n"; continue;
            }
            if (cmd == "buy") {
                bool ok = pf_.buy(to_upper(ticker), qty, price);
                std::cout << (ok ? "\033[32m✔ Buy recorded\033[0m\n" : "\033[31m✘ Buy failed\033[0m\n");
            } else {
                std::string reason;
                bool ok = pf_.sell(to_upper(ticker), qty, price, reason);
                if (ok) std::cout << "\033[32m✔ Sell recorded\033[0m\n";
                else std::cout << "\033[31m✘ Sell failed: " << reason << "\033[0m\n";
            }
            continue;
        }

        if (cmd == "fetch") {
            std::string ticker; is >> ticker;
            if (ticker.empty()) { std::cout << "usage: fetch TICKER\n"; continue; }
            try {
                auto q = pf_.refreshPriceFromAPI(to_upper(ticker));
                std::cout << "\033[36mFetched " << q.symbol << " = " << q.price << " (source " << q.source << ")\033[0m\n";
            } catch (const std::exception& e) { std::cout << "Fetch failed: " << e.what() << "\n"; }
            continue;
        }

        if (cmd == "holdings") {
            auto h = pf_.compute_holdings();
            int widths[] = {10,8,10,10,12,10,5};
            print_table_border(widths, 7);
            std::cout << "|" << std::setw(widths[0]) << "Ticker"
                      << "|" << std::setw(widths[1]) << "Qty"
                      << "|" << std::setw(widths[2]) << "AvgBuy"
                      << "|" << std::setw(widths[3]) << "Last"
                      << "|" << std::setw(widths[4]) << "P&L"
                      << "|" << std::setw(widths[5]) << "P&L%"
                      << "|" << std::setw(widths[6]) << "✔✘" << "|\n";
            print_table_border(widths, 7);

            for (auto& it : h) {
                bool profit = it.pnl >= 0;
                std::string symbol = profit ? "\033[32m✔\033[0m" : "\033[31m✘\033[0m";
                std::string pnl_col = profit ? "\033[32m" : "\033[31m";

                std::cout << "|" << std::setw(widths[0]) << it.ticker
                          << "|" << std::setw(widths[1]) << it.quantity
                          << "|" << std::setw(widths[2]) << it.avg_buy_price
                          << "|" << std::setw(widths[3]) << it.last_price
                          << "|" << pnl_col << std::setw(widths[4]) << std::fixed << std::setprecision(2) << it.pnl << "\033[0m"
                          << "|" << pnl_col << std::setw(widths[5]) << it.pnl_percent << "%" << "\033[0m"
                          << "|" << std::setw(widths[6]) << symbol << "|\n";
            }
            print_table_border(widths, 7);
            continue;
        }

        if (cmd == "value") {
            double total = pf_.total_portfolio_value();
            double ur = pf_.total_unrealized_pnl();
            double rr = pf_.total_realized_pnl();

            std::cout << "\n\033[36mPortfolio Summary\033[0m\n";
            std::cout << "------------------\n";
            std::cout << "Market Value   : \033[36m" << total << "\033[0m\n";
            std::cout << "Unrealized PnL : " << (ur>=0 ? "\033[32m" : "\033[31m") << ur << "\033[0m\n";
            std::cout << "Realized PnL   : " << (rr>=0 ? "\033[32m" : "\033[31m") << rr << "\033[0m\n";

            // ASCII bar chart
            auto bar = [](double val) {
                int len = (int)(val / 100); if (len < 0) len = 0; if (len > 50) len = 50;
                return std::string(len, '#');
            };
            std::cout << "\n[Value Chart]\n";
            std::cout << "Total      | \033[36m" << bar(total) << "\033[0m (" << total << ")\n";
            std::cout << "Unrealized | " << (ur>=0 ? "\033[32m" : "\033[31m") << bar(ur) << "\033[0m (" << ur << ")\n";
            std::cout << "Realized   | " << (rr>=0 ? "\033[32m" : "\033[31m") << bar(rr) << "\033[0m (" << rr << ")\n";
            continue;
        }

        std::cout << "Unknown command. Type help.\n";
    }
}
