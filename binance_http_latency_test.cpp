#include <curl/curl.h>
#include <chrono>
#include <iostream>
#include <string>

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

int main() {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    // 初始化
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // 设置 URL
        const char* url = "https://api.binance.com/api/v3/depth?symbol=BTCUSDT&limit=5";
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // 写入响应回调
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // 开启 TCP_NODELAY、超时设置
        curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 3000L);

        // 记录开始时间
        auto start = std::chrono::high_resolution_clock::now();

        // 执行请求
        res = curl_easy_perform(curl);

        // 记录结束时间
        auto end = std::chrono::high_resolution_clock::now();
        auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << "\n";
        } else {
            std::cout << "Latency: " << latency_ms << " ms\n";
            std::cout << "Response:\n" << readBuffer << "\n";
        }

        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return 0;
}