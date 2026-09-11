#include "ui/ChatView.h"
#include "ui/ChatBubble.h"

#include <QScrollBar>

ChatView::ChatView(QWidget* parent)
    : QScrollArea(parent)
    , m_content(new QWidget(this))
    , m_layout(new QVBoxLayout(m_content))
    , m_activeAssistant(nullptr) {

    setWidget(m_content);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setObjectName("chatView");  // 让全局 QSS 用 #chatView 选择器
    m_content->setObjectName("chatContent");

    m_layout->setAlignment(Qt::AlignTop);
    m_layout->setSpacing(8);
    m_layout->setContentsMargins(12, 12, 12, 12);

    // 底部加 stretch,让消息往上顶
    m_layout->addStretch();
}

void ChatView::addMessage(ChatBubble::Role role, const QString& text) {
    // 移除底部 stretch,插入气泡后再补回来
    // 策略:addWidget 到倒数第 2 个位置(stretch 之前)
    int stretchIndex = m_layout->count() - 1;

    ChatBubble* bubble = (role == ChatBubble::Role::User)
        ? ChatBubble::user(text, m_content)
        : ChatBubble::assistant(text, m_content);

    m_layout->insertWidget(stretchIndex, bubble);

    // 非流式:每条 AI 消息结束后 activeAssistant 置空
    if (role == ChatBubble::Role::Assistant) {
        m_activeAssistant = nullptr;
    }

    scrollToBottom();
}

void ChatView::appendToLastAssistant(const QString& token) {
    if (!m_activeAssistant) {
        // 没有活跃的 AI 气泡,先创建一个空的
        int stretchIndex = m_layout->count() - 1;
        m_activeAssistant = ChatBubble::assistant("", m_content);
        m_layout->insertWidget(stretchIndex, m_activeAssistant);
    }
    m_activeAssistant->appendText(token);
    scrollToBottom();
}

void ChatView::clearAll() {
    QLayoutItem* item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }
    m_activeAssistant = nullptr;
    m_layout->addStretch();
}

void ChatView::scrollToBottom() {
    QScrollBar* sb = verticalScrollBar();
    sb->setValue(sb->maximum());
}
