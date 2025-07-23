//
// Created by scorpio-b on 7/23/25.
//

#include "DatabaseManager.h"
#include <spdlog/spdlog.h>

DatabaseManager::DatabaseManager(const std::string& host,
                                const std::string& user,
                                const std::string& password,
                                const std::string& database,
                                int port)
    : m_host(host), m_user(user), m_password(password),
      m_database(database), m_port(port) {

    m_conn = mysql_init(nullptr);
    if (!m_conn) {
        spdlog::critical("MySQL初始化失败");
        throw std::runtime_error("MySQL initialization failed");
    }

    if (!mysql_real_connect(
        m_conn,
        m_host.c_str(),
        m_user.c_str(),
        m_password.c_str(),
        m_database.c_str(),
        m_port,
        nullptr, 0)) {

        std::string err = "数据库连接错误: ";
        err += mysql_error(m_conn);
        mysql_close(m_conn);
        spdlog::error(err);
        throw std::runtime_error(err);
    }

    spdlog::info("成功连接到数据库: {}:{}/{}", m_host, m_port, m_database);
}

DatabaseManager::~DatabaseManager() {
    if (m_conn) {
        mysql_close(m_conn);
        spdlog::info("数据库连接已关闭");
    }
}

bool DatabaseManager::isConnected() const {
    int status = mysql_ping(m_conn);
    if (status != 0) {
        spdlog::warn("数据库连接已断开: {}", mysql_error(m_conn));
    }
    return status == 0;
}

void DatabaseManager::reconnect() {
    spdlog::warn("尝试重新连接数据库...");
    mysql_close(m_conn);
    m_conn = mysql_init(nullptr);
    if (mysql_real_connect(
        m_conn,
        m_host.c_str(),
        m_user.c_str(),
        m_password.c_str(),
        m_database.c_str(),
        m_port,
        nullptr, 0)) {
        spdlog::info("数据库重新连接成功");
    } else {
        spdlog::error("数据库重新连接失败: {}", mysql_error(m_conn));
    }
}

std::pair<std::string, long> DatabaseManager::getLatestToken() {
    const char* query = "SELECT token, expires_at FROM access_tokens ORDER BY created_at DESC LIMIT 1";
    if (mysql_query(m_conn, query)) {
        spdlog::error("查询最新Token失败: {}", mysql_error(m_conn));
        return {"", 0};
    }

    MYSQL_RES* res = mysql_store_result(m_conn);
    if (!res) {
        spdlog::error("获取查询结果失败: {}", mysql_error(m_conn));
        return {"", 0};
    }

    if (mysql_num_rows(res) == 0) {
        mysql_free_result(res);
        spdlog::debug("数据库中没有Token记录");
        return {"", 0};
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    std::string token = row[0] ? row[0] : "";
    long expires = row[1] ? std::stol(row[1]) : 0;
    mysql_free_result(res);

    spdlog::debug("从数据库获取Token: {}**** (过期时间: {})", token.substr(0, 6), expires);
    return {token, expires};
}

void DatabaseManager::storeToken(const std::string& token, long expires) {
    char query[1024];
    snprintf(query, sizeof(query),
        "INSERT INTO access_tokens (token, expires_at) VALUES ('%s', %ld)",
        token.c_str(), expires);

    if (mysql_query(m_conn, query)) {
        spdlog::error("存储Token失败: {}", mysql_error(m_conn));
    } else {
        spdlog::info("Token已存储到数据库 (过期时间: {})", expires);
    }
}