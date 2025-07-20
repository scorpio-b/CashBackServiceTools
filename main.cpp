//
// Created by scorpio-b on 7/20/25.
//

#include <iostream>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <chrono>
#include <atomic>
#include <thread>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class WeChatAccessToken {
public:
    // 构造函数
    WeChatAccessToken(const std::string& appId, const std::string& appSecret)
        : m_appId(appId), m_appSecret(appSecret) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }

    // 析构函数
    ~WeChatAccessToken() {
        m_running = false;
        if (m_refreshThread.joinable()) {
            m_refreshThread.join();
        }
        curl_global_cleanup();
    }

    // 初始化并启动刷新线程
    void start() {
        fetchToken();
        m_running = true;
        m_refreshThread = std::thread(&WeChatAccessToken::refreshLoop, this);
    }

    // 获取当前有效的token
    std::string getToken() const {
        std::shared_lock lock(m_tokenMutex);
        return m_accessToken;
    }

    // 获取过期时间（毫秒时间戳）
    long long getExpiresAt() const {
        return m_expiresAt.load();
    }

private:
    // HTTP响应回调
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
        size_t totalSize = size * nmemb;
        data->append(static_cast<char*>(contents), totalSize);
        return totalSize;
    }

    // 获取新token
    void fetchToken() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }

        const std::string url = "https://api.weixin.qq.com/cgi-bin/token?grant_type=client_credential&appid=" +
                               m_appId + "&secret=" + m_appSecret;

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw std::runtime_error("CURL request failed: " + std::string(curl_easy_strerror(res)));
        }

        json result = json::parse(response);
        if (result.contains("errcode")) {
            int errorCode = result["errcode"];
            std::string errorMsg = result["errmsg"];
            throw std::runtime_error("WeChat API error: " + std::to_string(errorCode) + " - " + errorMsg);
        }

        std::unique_lock lock(m_tokenMutex);
        m_accessToken = result["access_token"];

        // 提前5分钟刷新（微信默认7200秒有效期）
        long expiresIn = result["expires_in"];
        auto expiresAt = std::chrono::system_clock::now() +
                         std::chrono::seconds(expiresIn) -
                         std::chrono::minutes(5);

        m_expiresAt.store(std::chrono::duration_cast<std::chrono::milliseconds>(
            expiresAt.time_since_epoch()).count());
    }

    // 刷新循环
    void refreshLoop() {
        while (m_running) {
            auto now = std::chrono::system_clock::now();
            auto expiresAt = std::chrono::system_clock::time_point(
                std::chrono::milliseconds(m_expiresAt.load()));

            // 计算下次刷新时间
            auto nextRefresh = expiresAt - std::chrono::minutes(5);
            if (nextRefresh < now) {
                nextRefresh = now + std::chrono::seconds(30); // 立即重试
            }

            // 等待到刷新时间
            std::this_thread::sleep_until(nextRefresh);

            try {
                fetchToken();
                std::cout << "Successfully refreshed access token" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "Token refresh failed: " << e.what() << std::endl;
            }
        }
    }

    const std::string m_appId;
    const std::string m_appSecret;

    mutable std::shared_mutex m_tokenMutex;
    std::string m_accessToken;
    std::atomic<long long> m_expiresAt{0}; // 毫秒时间戳

    std::atomic<bool> m_running{false};
    std::thread m_refreshThread;
};