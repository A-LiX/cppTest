#include <curl/curl.h>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int main() {
    constexpr int NUM_REQUESTS = 10;
    const char* url = "https://fapi.binance.com/fapi/v1/depth?symbol=BTCUSDT&limit=5";
    std::vector<long long> latencies;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    for (int i = 0; i < NUM_REQUESTS; ++i) {
        CURL* curl = curl_easy_init();
        std::string readBuffer;

        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
            curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 3000L);

            auto start = std::chrono::high_resolution_clock::now();
            CURLcode res = curl_easy_perform(curl);
            auto end = std::chrono::high_resolution_clock::now();
            std::cout << "start: " << start.time_since_epoch().count() << std::endl;
                        if (res == CURLE_OK) {
                std::cout << "raw json: " << readBuffer << std::endl;
            }
            std::cout << "end: " << end.time_since_epoch().count() << std::endl;
            long long latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            std::cout << "latency_ms: " << latency_ms << std::endl;

            curl_easy_cleanup(curl);
        }

    }

    return 0;
}