#pragma once
#include <string>

struct Transaction {
    int id;                // Add this
    std::string symbol;
    int quantity;
    double price;
    std::string timestamp;
};
