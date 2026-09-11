#pragma once

#include <QScrollArea>
#include <QWidget>
#include <QVBoxLayout>
#include <QResizeEvent>

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

    /// 是否有未完成的 AI 回复
    bool hasActiveAssistant() const { return m_activeAssistant != nullptr; }

    /// 标记当前流式 AI 气泡结束
    void finishAssistant() { m_activeAssistant = nullptr; }

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* m_content;
    QVBoxLayout* m_layout;
    ChatBubble* m_activeAssistant;

    void scrollToBottom();
    void updateAllBubbleWidths();
};
