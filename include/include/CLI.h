#pragma once
#include "Portfolio.h"
#include <string>
#include <sstream>

class CLI {
public:
    CLI(Portfolio& pf);
    void run();

private:
    Portfolio& pf_;

    static std::string to_upper(std::string s);
    static bool to_int(const std::string& s, int& out);
    static bool to_double(const std::string& s, double& out);

    void print_help() const;
};
