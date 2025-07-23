//
// Created by scorpio-b on 7/23/25.
//

#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <mariadb/conncpp.hpp>
#include <string>
#include <memory>
#include <spdlog/spdlog.h>

class DatabaseManager {
public:
    using ResultSetPtr = std::unique_ptr<sql::ResultSet>;
    using StatementPtr = std::unique_ptr<sql::Statement>;
    using PreparedStatementPtr = std::unique_ptr<sql::PreparedStatement>;

    explicit DatabaseManager(const std::string& host,
                            const std::string& user,
                            const std::string& password,
                            const std::string& database,
                            int port);
    ~DatabaseManager();

    bool isConnected() const;
    void reconnect();
    std::pair<std::string, long> getLatestToken();
    void storeToken(const std::string& token, long expires);

    // 禁用拷贝
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

private:
    void initializeConnection();

    std::unique_ptr<sql::Connection> conn_;
    std::string host_;
    std::string user_;
    std::string password_;
    std::string database_;
    int port_;
};

#endif // DATABASE_MANAGER_H