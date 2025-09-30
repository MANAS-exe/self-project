#pragma once
#include <string>
#include <json/json.h> // jsoncpp
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>
#include <iostream>

struct Quote {
    std::string symbol;
    double price = 0.0;
    std::string asOf;
    std::string source;
};

class ApiClient {
public:
    // Hard-coded key as requested
    ApiClient();

    // Fetch latest quote using Alpha Vantage (GLOBAL_QUOTE first, fallback to TIME_SERIES_DAILY)
    Quote fetchLatestQuote(const std::string& symbol) const;

private:
    std::string apiKey_;

    static size_t curlWrite(void* contents, size_t size, size_t nmemb, void* userp);
    std::string httpGet(const std::string& url) const;

    static double parseGlobalQuote(const std::string& body, std::string& asOf);
    static double parseDailyClose(const std::string& body, std::string& asOf);
};
