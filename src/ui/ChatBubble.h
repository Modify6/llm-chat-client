#pragma once

#include <QFrame>
#include <QLabel>

/**
 * @brief 单条聊天气泡
 *
 * 用法:
 *   auto* bubble = ChatBubble::user("你好");
 *   auto* bubble = ChatBubble::assistant("你好啊!");
 */
class ChatBubble : public QFrame {
    Q_OBJECT

public:
    enum class Role { User, Assistant };

    static ChatBubble* user(const QString& text, QWidget* parent = nullptr);
    static ChatBubble* assistant(const QString& text, QWidget* parent = nullptr);

    /// 获取内部文本标签(用于流式追加 token)
    QLabel* textLabel() const { return m_label; }

    /// 追加文本(流式场景逐 token 追加)
    void appendText(const QString& text);

private:
    explicit ChatBubble(Role role, const QString& text, QWidget* parent);

    Role m_role;
    QLabel* m_label;
};
