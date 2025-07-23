//
// Created by scorpio-b on 7/23/25.
//

#ifndef TOKEN_MANAGER_H
#define TOKEN_MANAGER_H

#include <string>
#include <memory>
#include <functional>
#include "../utils/DatabaseManager.h"

class TokenManager {
public:
    using TokenCallback = std::function<void(const std::string&, long)>;

    TokenManager(const std::string& appId,
                const std::string& appSecret,
                std::shared_ptr<DatabaseManager> db);

    bool fetchToken();
    bool isValid() const;
    void setCurrentToken(const std::string& token, long expiry);
    long getNextFetchDelay() const;
    void setTokenUpdateCallback(TokenCallback callback);
    long getCurrentTimestamp() const;

private:
    std::string m_appId;
    std::string m_appSecret;
    std::shared_ptr<DatabaseManager> m_db;
    std::string m_currentToken;
    long m_tokenExpiry = 0;
    TokenCallback m_tokenCallback;

    static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* s);
};

#endif // TOKEN_MANAGER_H