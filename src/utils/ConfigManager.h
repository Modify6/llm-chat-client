#pragma once

#include "core/LLMClient.h"

#include <QString>

/**
 * @brief QSettings 封装:LLMClient::Config 的持久化
 *
 * 存储位置:QStandardPaths::AppConfigLocation/llm-chat-client/config.ini
 */
class ConfigManager {
public:
    static ConfigManager& instance();

    /// 加载配置(失败返回默认值)
    LLMClient::Config load() const;

    /// 保存配置
    void save(const LLMClient::Config& cfg) const;

private:
    ConfigManager() = default;

    // QSettings 键名常量
    static const char* kHost;
    static const char* kPort;
    static const char* kModel;
    static const char* kTemperature;
    static const char* kMaxTokens;
    static const char* kTimeoutSeconds;
};
