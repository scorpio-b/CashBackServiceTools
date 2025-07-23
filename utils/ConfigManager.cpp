//
// Created by scorpio-b on 7/23/25.
//

#include "ConfigManager.h"
#include <fstream>
#include <stdexcept>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

AppConfig ConfigManager::load(const std::string& configPath) {
    std::ifstream configFile(configPath);
    if (!configFile) {
        throw std::runtime_error("配置文件未找到: " + configPath);
    }

    json configJson;
    configFile >> configJson;

    spdlog::info("加载配置文件: {}", configPath);

    return {
        configJson.value("appid", ""),
        configJson.value("secret", ""),
        configJson.value("db_host", "localhost"),
        configJson.value("db_user", "root"),
        configJson.value("db_pass", ""),
        configJson.value("db_name", "wechat_db"),
        configJson.value("db_port", 3306),
        configJson.value("log_file", "token_service.log"),
        configJson.value("log_level", "info")
    };
}