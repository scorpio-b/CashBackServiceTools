//
// Created by scorpio-b on 7/23/25.
//

#include "TokenManager.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <sstream>
#include <chrono>

using json = nlohmann::json;

TokenManager::TokenManager(const std::string& appId,
                          const std::string& appSecret,
                          std::shared_ptr<DatabaseManager> db)
    : m_appId(appId), m_appSecret(appSecret), m_db(db) {
    spdlog::info("初始化Token管理器: AppID={}****", appId.substr(0, 4));
}

bool TokenManager::fetchToken() {
    spdlog::debug("开始获取微信AccessToken");

    std::ostringstream urlStream;
    urlStream << "https://api.weixin.qq.com/cgi-bin/token"
              << "?grant_type=client_credential"
              << "&appid=" << m_appId
              << "&secret=" << m_appSecret;

    std::string url = urlStream.str();
    std::string response;

    CURL* curl = curl_easy_init();
    if (!curl) {
        spdlog::error("CURL初始化失败");
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    spdlog::debug("发送请求到微信API: {}", url);
    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        spdlog::error("CURL请求失败: {}", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        return false;
    }

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (httpCode != 200) {
        spdlog::error("HTTP错误: {}", httpCode);
        return false;
    }

    spdlog::debug("收到微信API响应: {}", response);

    try {
        auto jsonResponse = json::parse(response);

        if (jsonResponse.contains("errcode")) {
            spdlog::error("微信API错误: {} - {}",
                          jsonResponse["errcode"].get<int>(),
                          jsonResponse["errmsg"].get<std::string>());
            return false;
        }

        m_currentToken = jsonResponse["access_token"].get<std::string>();
        long expiresIn = jsonResponse["expires_in"].get<long>();
        m_tokenExpiry = getCurrentTimestamp() + expiresIn - 300; // 提前5分钟过期

        spdlog::info("成功获取Token: {}**** (有效期: {}秒)",
                    m_currentToken.substr(0, 6), expiresIn);

        m_db->storeToken(m_currentToken, m_tokenExpiry);

        if (m_tokenCallback) {
            m_tokenCallback(m_currentToken, m_tokenExpiry);
        }

        return true;
    } catch (const std::exception& e) {
        spdlog::error("JSON解析错误: {}", e.what());
        return false;
    }
}

bool TokenManager::isValid() const {
    bool valid = !m_currentToken.empty() && getCurrentTimestamp() < m_tokenExpiry;
    if (!valid) {
        spdlog::debug("Token无效或已过期");
    }
    return valid;
}

void TokenManager::setCurrentToken(const std::string& token, long expiry) {
    m_currentToken = token;
    m_tokenExpiry = expiry;
    spdlog::debug("设置当前Token (过期时间: {})", expiry);
}

long TokenManager::getNextFetchDelay() const {
    long current = getCurrentTimestamp();
    long delay = (m_tokenExpiry > current) ? (m_tokenExpiry - current) * 1000 : 0;
    spdlog::debug("计算下次获取延迟: {}ms", delay);
    return delay;
}

void TokenManager::setTokenUpdateCallback(TokenCallback callback) {
    m_tokenCallback = callback;
    spdlog::debug("设置Token更新回调");
}

size_t TokenManager::curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t newLength = size * nmemb;
    try {
        s->append(static_cast<char*>(contents), newLength);
        return newLength;
    } catch(...) {
        return 0;
    }
}

long TokenManager::getCurrentTimestamp() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
