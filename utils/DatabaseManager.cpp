//
// Created by scorpio-b on 7/23/25.
//

#include "DatabaseManager.h"
#include <mariadb/conncpp/Driver.hpp>

DatabaseManager::DatabaseManager(const std::string& host,
                                const std::string& user,
                                const std::string& password,
                                const std::string& database,
                                int port)
    : host_(host), user_(user), password_(password),
      database_(database), port_(port) {

    initializeConnection();
}

DatabaseManager::~DatabaseManager() {
    if (conn_ && !conn_->isClosed()) {
        try {
            conn_->close();
            spdlog::info("数据库连接已关闭");
        } catch (const sql::SQLException& e) {
            spdlog::error("关闭数据库连接时出错: {}", e.what());
        }
    }
}

void DatabaseManager::initializeConnection() {
    try {
        // 获取驱动实例
        sql::Driver* driver = sql::mariadb::get_driver_instance();

        // 创建连接属性
        sql::SQLString url = "jdbc:mariadb://" + host_ + ":" +
                             std::to_string(port_) + "/" + database_;

        sql::Properties properties;
        properties["user"] = user_;
        properties["password"] = password_;

        // 创建连接
        conn_.reset(driver->connect(url, properties));

        spdlog::info("成功连接到数据库: {}:{}/{}", host_, port_, database_);
    }
    catch (const sql::SQLException& e) {
        spdlog::error("数据库连接失败: {} [错误代码: {}, SQL状态: {}]",
                     e.what(), e.getErrorCode(), e.getSQLState());
        throw std::runtime_error("数据库连接失败: " + std::string(e.what()));
    }
}

bool DatabaseManager::isConnected() const {
    if (!conn_) return false;

    try {
        return !conn_->isClosed();
    }
    catch (const sql::SQLException& e) {
        spdlog::error("检查连接状态失败: {}", e.what());
        return false;
    }
}

void DatabaseManager::reconnect() {
    spdlog::warn("尝试重新连接数据库...");
    if (conn_ && !conn_->isClosed()) {
        try {
            conn_->close();
        } catch (...) {}
    }
    initializeConnection();
}

std::pair<std::string, long> DatabaseManager::getLatestToken() {
    if (!isConnected()) {
        throw std::runtime_error("尝试查询但数据库未连接");
    }

    try {
        PreparedStatementPtr stmt(conn_->prepareStatement(
            "SELECT token, expires_at FROM access_tokens ORDER BY created_at DESC LIMIT 1"
        ));

        ResultSetPtr res(stmt->executeQuery());

        if (!res->next()) {
            spdlog::debug("数据库中没有Token记录");
            return {"", 0};
        }

        std::string token = res->getString("token");
        long expires = res->getInt64("expires_at");

        spdlog::debug("从数据库获取Token: {}**** (过期时间: {})",
                      token.substr(0, 6), expires);
        return {token, expires};
    }
    catch (const sql::SQLException& e) {
        spdlog::error("查询最新Token失败: {} [错误代码: {}, SQL状态: {}]",
                     e.what(), e.getErrorCode(), e.getSQLState());
        return {"", 0};
    }
}

void DatabaseManager::storeToken(const std::string& token, long expires) {
    if (!isConnected()) {
        throw std::runtime_error("尝试存储Token但数据库未连接");
    }

    try {
        PreparedStatementPtr stmt(conn_->prepareStatement(
            "INSERT INTO access_tokens (token, expires_at) VALUES (?, ?)"
        ));

        stmt->setString(1, token);
        stmt->setInt64(2, expires);

        int affected = stmt->executeUpdate();
        spdlog::info("Token已存储到数据库 (过期时间: {}), 影响行数: {}", expires, affected);
    }
    catch (const sql::SQLException& e) {
        spdlog::error("存储Token失败: {} [错误代码: {}, SQL状态: {}]",
                     e.what(), e.getErrorCode(), e.getSQLState());
    }
}