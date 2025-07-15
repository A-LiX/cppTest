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
    const char* url = "https://api.binance.com/api/v3/depth?symbol=BTCUSDT&limit=5";
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

            long long latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            if (res != CURLE_OK) {
                std::cerr << "[# " << i + 1 << "] curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
            } else {
                std::cout << "[# " << i + 1 << "] Latency: " << latency_ms << " ms\n";
                latencies.push_back(latency_ms);
            }

            curl_easy_cleanup(curl);
        }

    }

    curl_global_cleanup();

    // 统计结果
    if (!latencies.empty()) {
        auto [min_it, max_it] = std::minmax_element(latencies.begin(), latencies.end());
        double avg = std::accumulate(latencies.begin(), latencies.end(), 0LL) / static_cast<double>(latencies.size());

        std::cout << "\n=== Latency Summary ===\n";
        std::cout << "Requests: " << latencies.size() << "\n";
        std::cout << "Min: " << *min_it << " ms\n";
        std::cout << "Max: " << *max_it << " ms\n";
        std::cout << "Avg: " << avg << " ms\n";
    }

    return 0;
}