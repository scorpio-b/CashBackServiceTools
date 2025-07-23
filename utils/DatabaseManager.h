//
// Created by scorpio-b on 7/23/25.
//

#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <mariadb/mysql.h>
#include <string>
#include <utility>
#include <memory>

class DatabaseManager {
public:
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
    MYSQL* m_conn;
    std::string m_host;
    std::string m_user;
    std::string m_password;
    std::string m_database;
    int m_port;
};

#endif // DATABASE_MANAGER_H