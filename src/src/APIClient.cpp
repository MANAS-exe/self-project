#include "ApiClient.h"

// Hard-coded key (as requested)
ApiClient::ApiClient() : apiKey_("YOLW0MFHJTCCJUUK") {}

size_t ApiClient::curlWrite(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realSize = size * nmemb;
    auto* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), realSize);
    return realSize;
}

std::string ApiClient::httpGet(const std::string& url) const {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("curl_easy_init failed");

    std::string resp;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &ApiClient::curlWrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Explicitly set CA certificate bundle for MSYS2/Windows
    curl_easy_setopt(curl, CURLOPT_CAINFO, "C:/msys64/ucrt64/etc/ssl/certs/ca-bundle.crt");
    curl_easy_setopt(curl, CURLOPT_CAPATH, "C:/msys64/ucrt64/etc/ssl/certs");

    // Verify SSL peer (can set to 0L for debugging if certs not found)
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

    CURLcode rc = curl_easy_perform(curl);
    if (rc != CURLE_OK) {
        std::string err = curl_easy_strerror(rc);
        curl_easy_cleanup(curl);
        throw std::runtime_error("curl error: " + err);
    }

    curl_easy_cleanup(curl);
    return resp;
}


double ApiClient::parseGlobalQuote(const std::string& body, std::string& asOf) {
    Json::CharReaderBuilder b;
    Json::Value root;
    std::string errs;
    std::istringstream ss(body);
    if (!Json::parseFromStream(b, ss, &root, &errs)) {
        throw std::runtime_error("JSON parse error (GLOBAL_QUOTE): " + errs);
    }
    if (root.isMember("Note")) {
        throw std::runtime_error("Alpha Vantage NOTE: " + root["Note"].asString());
    }
    if (!root.isMember("Global Quote")) throw std::runtime_error("Global Quote missing");
    auto gq = root["Global Quote"];
    std::string p = gq.get("05. price", "0").asString();
    asOf = gq.get("07. latest trading day", "").asString();
    return std::stod(p);
}

double ApiClient::parseDailyClose(const std::string& body, std::string& asOf) {
    Json::CharReaderBuilder b;
    Json::Value root;
    std::string errs;
    std::istringstream ss(body);
    if (!Json::parseFromStream(b, ss, &root, &errs)) {
        throw std::runtime_error("JSON parse error (DAILY): " + errs);
    }
    if (root.isMember("Note")) {
        throw std::runtime_error("Alpha Vantage NOTE: " + root["Note"].asString());
    }
    if (!root.isMember("Time Series (Daily)")) throw std::runtime_error("Time Series missing");
    auto ts = root["Time Series (Daily)"];
    auto names = ts.getMemberNames();
    if (names.empty()) throw std::runtime_error("No daily entries");
    asOf = names.front();
    auto day = ts[asOf];
    std::string p = day.get("4. close", "0").asString();
    return std::stod(p);
}

Quote ApiClient::fetchLatestQuote(const std::string& symbol) const {
    Quote q; q.symbol = symbol;
    try {
        std::ostringstream url;
        url << "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol="
            << symbol << "&apikey=" << apiKey_;
        std::cout << "[API] Requesting GLOBAL_QUOTE for " << symbol << "\n";
        std::string body = httpGet(url.str());
        std::string asOf;
        double p = parseGlobalQuote(body, asOf);
        q.price = p; q.asOf = asOf; q.source = "GLOBAL_QUOTE";
        std::cout << "[API] GLOBAL_QUOTE " << symbol << " -> " << p << " asOf " << asOf << "\n";
        return q;
    } catch (const std::exception& e) {
        std::cerr << "[API] GLOBAL_QUOTE failed: " << e.what() << " — falling back to DAILY\n";
        // fallback
    }

    // Fallback to TIME_SERIES_DAILY
    {
        std::ostringstream url;
        url << "https://www.alphavantage.co/query?function=TIME_SERIES_DAILY&symbol="
            << symbol << "&apikey=" << apiKey_ << "&outputsize=compact";
        std::cout << "[API] Requesting TIME_SERIES_DAILY for " << symbol << "\n";
        std::string body = httpGet(url.str());
        std::string asOf;
        double p = parseDailyClose(body, asOf);
        q.price = p; q.asOf = asOf; q.source = "TIME_SERIES_DAILY";
        std::cout << "[API] DAILY " << symbol << " -> " << p << " asOf " << asOf << "\n";
        return q;
    }
}
