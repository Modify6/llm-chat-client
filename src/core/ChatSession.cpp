#include "core/ChatSession.h"
#include "json.hpp"

#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

using json = nlohmann::json;

ChatSession::ChatSession(QObject* parent)
    : QObject(parent) {}

void ChatSession::addMessage(const Message& msg) {
    m_messages.push_back(msg);
    emit messageAdded(msg);
}

void ChatSession::setSystemPrompt(const std::string& prompt) {
    // 替换或添加 system 消息
    for (int i = 0; i < m_messages.size(); ++i) {
        if (m_messages[i].role == Message::Role::System) {
            m_messages[i].content = prompt;
            emit messageAdded(m_messages[i]);
            return;
        }
    }
    m_messages.insert(m_messages.begin(), Message::system(prompt));
    emit messageAdded(m_messages[0]);
}

std::vector<Message> ChatSession::history() const {
    // 返回不带 system 的历史(LLMClient 里会单独加 system)
    std::vector<Message> result;
    for (const auto& m : m_messages) {
        if (m.role != Message::Role::System) {
            result.push_back(m);
        }
    }
    return result;
}

void ChatSession::clear() {
    m_messages.clear();
    emit cleared();
}

bool ChatSession::save(const QString& path) const {
    json arr = json::array();
    for (const auto& m : m_messages) {
        const char* roleStr = "user";
        switch (m.role) {
            case Message::Role::User:      roleStr = "user";      break;
            case Message::Role::Assistant: roleStr = "assistant"; break;
            case Message::Role::System:    roleStr = "system";    break;
        }
        arr.push_back({
            {"role", roleStr},
            {"content", m.content}
        });
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    QTextStream ts(&file);
    ts << QString::fromStdString(arr.dump(2));
    file.close();
    return true;
}

bool ChatSession::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
    QTextStream ts(&file);
    std::string jsonStr = ts.readAll().toStdString();
    file.close();

    try {
        auto j = json::parse(jsonStr);
        if (!j.is_array()) return false;

        m_messages.clear();
        for (const auto& item : j) {
            if (!item.contains("role") || !item.contains("content")) continue;
            std::string role = item["role"].get<std::string>();
            std::string content = item["content"].get<std::string>();
            Message::Role r = Message::Role::User;
            if (role == "assistant") r = Message::Role::Assistant;
            else if (role == "system") r = Message::Role::System;

            Message msg;
            msg.role = r;
            msg.content = content;
            msg.timestamp = std::chrono::system_clock::now();
            m_messages.push_back(msg);
        }
        return true;
    } catch (...) {
        return false;
    }
}
