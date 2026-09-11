#pragma once

#include "core/Message.h"

#include <QObject>
#include <QString>
#include <QVector>

/**
 * @brief 对话会话:管理消息历史 + JSON 持久化
 *
 * 用法:
 *   ChatSession session;
 *   session.addMessage(Message::system("..."));
 *   session.addMessage(Message::user("你好"));
 *   session.addMessage(Message::assistant("你好啊"));
 *   session.save("chat_history.json");
 *   session.load("chat_history.json");
 */
class ChatSession : public QObject {
    Q_OBJECT

public:
    explicit ChatSession(QObject* parent = nullptr);

    /// 添加一条消息到历史
    void addMessage(const Message& msg);

    /// 添加 system 消息(快捷方法)
    void setSystemPrompt(const std::string& prompt);

    /// 获取历史(不含 system,传给 LLM)
    std::vector<Message> history() const;

    /// 清空历史
    void clear();

    /// 保存到 JSON 文件,返回 true=成功
    bool save(const QString& path) const;

    /// 从 JSON 文件加载,返回 true=成功
    bool load(const QString& path);

    /// 消息数量
    int size() const { return static_cast<int>(m_messages.size()); }

signals:
    void messageAdded(const Message& msg);
    void cleared();

private:
    QVector<Message> m_messages;
};
