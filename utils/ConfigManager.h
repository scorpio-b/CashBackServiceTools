//
// Created by scorpio-b on 7/23/25.
//

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <string>
#include <nlohmann/json.hpp>

struct AppConfig {
    std::string appId;
    std::string appSecret;
    std::string dbHost;
    std::string dbUser;
    std::string dbPass;
    std::string dbName;
    int dbPort;
    std::string logFile;
    std::string logLevel;
};

class ConfigManager {
public:
    static AppConfig load(const std::string& configPath);
};

#endif // CONFIG_MANAGER_H