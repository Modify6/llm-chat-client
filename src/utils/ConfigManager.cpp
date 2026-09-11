#include "utils/ConfigManager.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>

const char* ConfigManager::kHost           = "connection/host";
const char* ConfigManager::kPort           = "connection/port";
const char* ConfigManager::kModel          = "connection/model";
const char* ConfigManager::kTemperature    = "generation/temperature";
const char* ConfigManager::kMaxTokens      = "generation/max_tokens";
const char* ConfigManager::kTimeoutSeconds = "connection/timeout_seconds";

ConfigManager& ConfigManager::instance() {
    static ConfigManager inst;
    return inst;
}

LLMClient::Config ConfigManager::load() const {
    LLMClient::Config cfg;

    // 确保配置目录存在
    QString cfgDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!cfgDir.isEmpty()) {
        QDir().mkpath(cfgDir);
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "llm-chat-client", "llm-chat-client");

    cfg.host            = settings.value(kHost,           QString::fromStdString(cfg.host)).toString().toStdString();
    cfg.port            = settings.value(kPort,           cfg.port).toInt();
    cfg.model           = settings.value(kModel,          QString::fromStdString(cfg.model)).toString().toStdString();
    cfg.temperature     = settings.value(kTemperature,     cfg.temperature).toDouble();
    cfg.max_tokens      = settings.value(kMaxTokens,      cfg.max_tokens).toInt();
    cfg.timeout_seconds = settings.value(kTimeoutSeconds,  cfg.timeout_seconds).toInt();

    return cfg;
}

void ConfigManager::save(const LLMClient::Config& cfg) const {
    QSettings settings(QSettings::IniFormat, QSettings::UserScope,
                       "llm-chat-client", "llm-chat-client");

    settings.setValue(kHost,           QString::fromStdString(cfg.host));
    settings.setValue(kPort,           cfg.port);
    settings.setValue(kModel,          QString::fromStdString(cfg.model));
    settings.setValue(kTemperature,     cfg.temperature);
    settings.setValue(kMaxTokens,      cfg.max_tokens);
    settings.setValue(kTimeoutSeconds,  cfg.timeout_seconds);

    settings.sync();
}
