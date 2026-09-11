#pragma once

#include <QScrollArea>
#include <QWidget>
#include <QVBoxLayout>

#include "ui/ChatBubble.h"

/**
 * @brief 聊天消息展示区(QScrollArea + 垂直布局)
 */
class ChatView : public QScrollArea {
    Q_OBJECT

public:
    explicit ChatView(QWidget* parent = nullptr);

    /// 添加一条消息气泡
    void addMessage(ChatBubble::Role role, const QString& text);

    /// 找到最后一个 AI 气泡,追加 token(流式场景)
    /// 没有 AI 气泡时自动创建一个空的
    void appendToLastAssistant(const QString& token);

    /// 清空所有气泡
    void clearAll();

    /// 是否有未完成的 AI 回复(用于流式状态判断)
    bool hasActiveAssistant() const { return m_activeAssistant != nullptr; }

    /// 标记当前流式 AI 气泡结束(从 active 置空)
    void finishAssistant() { m_activeAssistant = nullptr; }

private:
    QWidget* m_content;              ///< scroll area 内部内容 widget
    QVBoxLayout* m_layout;           ///< 垂直布局
    ChatBubble* m_activeAssistant;   ///< 当前正在流式追加的 AI 气泡(非流式时为 nullptr)

    void scrollToBottom();
};
