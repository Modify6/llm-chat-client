#pragma once

#include <QFrame>
#include <QLabel>

/**
 * @brief 单条聊天气泡
 *
 * AI 气泡用 QLabel + 自定义 markdown→HTML 转换器,
 * 避免 QTextBrowser viewport 裁剪内容的问题。
 * 用户气泡用纯文本 QLabel。
 *
 * 【复制功能】
 * - 右键任意气泡 → "复制消息" 复制整条原始文本
 * - 右键 AI 气泡 → 额外出现 "复制所有代码块"(从 m_rawText 里提取 ```...```)
 * - ChatView::allBubbles() 可遍历所有气泡,用于 MainWindow "复制全部对话"
 */
class ChatBubble : public QFrame {
    Q_OBJECT

public:
    enum class Role { User, Assistant };

    static ChatBubble* user(const QString& text, QWidget* parent = nullptr);
    static ChatBubble* assistant(const QString& text, QWidget* parent = nullptr);

    /// 追加原始文本(流式逐 token 追加,自动重渲)
    void appendText(const QString& text);

    /// 获取气泡角色(复制全部时区分 用户/AI 标签)
    Role role() const { return m_role; }

    /// 获取原始 Markdown/纯文本(复制功能用)
    const QString& rawText() const { return m_rawText; }

protected:
    /// 右键菜单入口:复制消息 / 复制代码块
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    explicit ChatBubble(Role role, const QString& text, QWidget* parent);
    void renderContent();

    Role m_role;
    QLabel* m_label;
    QString m_rawText;   ///< 累积的原始文本(Markdown for AI, plain text for User)
};
