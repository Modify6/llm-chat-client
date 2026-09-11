#pragma once

#include <QFrame>
#include <QLabel>

/**
 * @brief 单条聊天气泡
 *
 * AI 气泡用 QLabel + 自定义 markdown→HTML 转换器,
 * 避免 QTextBrowser viewport 裁剪内容的问题。
 * 用户气泡用纯文本 QLabel。
 */
class ChatBubble : public QFrame {
    Q_OBJECT

public:
    enum class Role { User, Assistant };

    static ChatBubble* user(const QString& text, QWidget* parent = nullptr);
    static ChatBubble* assistant(const QString& text, QWidget* parent = nullptr);

    /// 追加原始文本(流式逐 token 追加,自动重渲)
    void appendText(const QString& text);

private:
    explicit ChatBubble(Role role, const QString& text, QWidget* parent);
    void renderContent();

    Role m_role;
    QLabel* m_label;
    QString m_rawText;   ///< 累积的原始文本
};
