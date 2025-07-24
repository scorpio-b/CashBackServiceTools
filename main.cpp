//
// Created by scorpio-b on 7/20/25.
//

#include "utils/ConfigManager.h"
#include "utils/DatabaseManager.h"
#include "wechat/TokenManager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <iostream>
#include <memory>
#include <thread>
#include <functional>

int main() {
    try {
        // 加载配置
        AppConfig config = ConfigManager::load("configs/wechat_configs.json");

        // 初始化日志系统
        std::vector<spdlog::sink_ptr> sinks;
        sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
        sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            config.logFile, 1024 * 1024 * 5, 3));

        auto logger = std::make_shared<spdlog::logger>("main", begin(sinks), end(sinks));
        spdlog::set_default_logger(logger);

        // 设置日志级别
        if (config.logLevel == "trace")
        {
            spdlog::set_level(spdlog::level::trace);
        }
        else if (config.logLevel == "debug")
        {
            spdlog::set_level(spdlog::level::debug);
        }
        else if (config.logLevel == "warn")
        {
            spdlog::set_level(spdlog::level::warn);
        }
        else if (config.logLevel == "error")
        {
            spdlog::set_level(spdlog::level::err);
        }
        else if (config.logLevel == "critical")
        {
            spdlog::set_level(spdlog::level::critical);
        }
        else
        {
            spdlog::set_level(spdlog::level::info);
        }

        // 设置日志格式
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [thread %t] %v");

        spdlog::info("启动微信Token服务");
        spdlog::debug("日志级别: {}", config.logLevel);

        // 初始化数据库连接
        auto db = std::make_shared<DatabaseManager>(
            config.dbHost, config.dbUser, config.dbPass,
            config.dbName, config.dbPort
        );

        // 确保数据库连接
        while (!db->isConnected()) {
            spdlog::warn("数据库连接失败，5秒后重试...");
            std::this_thread::sleep_for(std::chrono::seconds(5));
            db->reconnect();
        }

        // 初始化Token管理器
        TokenManager tokenManager(config.appId, config.appSecret, db);

        // 设置Token更新回调
        tokenManager.setTokenUpdateCallback([&](const std::string& token, long expiry) {
            spdlog::info("Token已更新: {}****", token.substr(0, 6));
        });

        spdlog::info("进入主循环");

        // 主循环
        while (true) {
            // 从数据库获取最新Token
            auto [dbToken, dbExpiry] = db->getLatestToken();

            if (!dbToken.empty() && dbExpiry > tokenManager.getCurrentTimestamp()) {
                tokenManager.setCurrentToken(dbToken, dbExpiry);
                spdlog::info("从数据库加载有效Token");
            } else {
                // 重试机制
                int retries = 3;
                bool success = false;

                while (retries-- > 0) {
                    if (tokenManager.fetchToken()) {
                        success = true;
                        break;
                    }
                    spdlog::warn("获取Token失败，2秒后重试...");
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }

                if (!success) {
                    spdlog::critical("无法获取有效Token，服务终止");
                    return 1;
                }
            }

            // 计算下一次执行延迟
            long delay = tokenManager.getNextFetchDelay();
            if (delay <= 0) {
                spdlog::warn("无效的延迟时间: {}ms，使用默认60秒", delay);
                delay = 60000;
            }

            spdlog::info("下次Token获取将在 {} 秒后", delay/1000);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    } catch (const std::exception& e) {
        spdlog::critical("致命错误: {}", e.what());
        return 1;
    }

    spdlog::info("服务正常退出");
    spdlog::shutdown();
    return 0;
}