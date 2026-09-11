#include "ui/ChatView.h"
#include "ui/ChatBubble.h"

#include <QScrollBar>
#include <QHBoxLayout>

ChatView::ChatView(QWidget* parent)
    : QScrollArea(parent)
    , m_content(new QWidget(this))
    , m_layout(new QVBoxLayout(m_content))
    , m_activeAssistant(nullptr) {

    setWidget(m_content);
    setWidgetResizable(true);
    setFrameShape(QFrame::NoFrame);
    setStyleSheet("QScrollArea { border: none; background: #ECECEC; }");

    m_layout->setAlignment(Qt::AlignTop);
    m_layout->setSpacing(8);
    m_layout->setContentsMargins(12, 12, 12, 12);
}

void ChatView::addMessage(ChatBubble::Role role, const QString& text) {
    auto* row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);

    ChatBubble* bubble = (role == ChatBubble::Role::User)
        ? ChatBubble::user(text, m_content)
        : ChatBubble::assistant(text, m_content);

    // 设 maximumWidth
    int maxBubbleW = static_cast<int>(m_content->width() * 0.7);
    if (maxBubbleW < 100) maxBubbleW = 600;
    bubble->setMaximumWidth(maxBubbleW);

    if (role == ChatBubble::Role::User) {
        row->addWidget(bubble, 0, Qt::AlignRight | Qt::AlignTop);
    } else {
        row->addWidget(bubble, 0, Qt::AlignLeft | Qt::AlignTop);
    }

    m_layout->addLayout(row);

    if (role == ChatBubble::Role::Assistant) {
        m_activeAssistant = bubble;
    }

    scrollToBottom();
}

void ChatView::appendToLastAssistant(const QString& token) {
    if (!m_activeAssistant) {
        auto* row = new QHBoxLayout();
        row->setContentsMargins(0, 0, 0, 0);
        row->setSpacing(0);

        m_activeAssistant = ChatBubble::assistant("", m_content);

        int maxBubbleW = static_cast<int>(m_content->width() * 0.7);
        if (maxBubbleW < 100) maxBubbleW = 600;
        m_activeAssistant->setMaximumWidth(maxBubbleW);

        row->addWidget(m_activeAssistant, 0, Qt::AlignLeft | Qt::AlignTop);

        m_layout->addLayout(row);
    }
    m_activeAssistant->appendText(token);
    scrollToBottom();
}

void ChatView::clearAll() {
    QLayoutItem* item;
    while ((item = m_layout->takeAt(0)) != nullptr) {
        QWidget* w = item->widget();
        if (w) {
            w->deleteLater();
        } else {
            QLayout* subLayout = item->layout();
            if (subLayout) {
                QLayoutItem* sub;
                while ((sub = subLayout->takeAt(0)) != nullptr) {
                    if (sub->widget()) sub->widget()->deleteLater();
                    delete sub;
                }
            }
        }
        delete item;
    }
    m_activeAssistant = nullptr;
}

/**
 * 窗口大小变化时,更新所有气泡的 maximumWidth
 * 并滚动到底部(如果用户正在看最新消息)
 */
void ChatView::resizeEvent(QResizeEvent* event) {
    QScrollArea::resizeEvent(event);
    updateAllBubbleWidths();
}

void ChatView::updateAllBubbleWidths() {
    int maxBubbleW = static_cast<int>(m_content->width() * 0.7);
    if (maxBubbleW < 100) return;  // 还没布局好

    // 遍历所有 row layout 里的 bubble
    for (int i = 0; i < m_layout->count(); ++i) {
        QLayoutItem* rowItem = m_layout->itemAt(i);
        if (!rowItem) continue;
        QLayout* rowLayout = rowItem->layout();
        if (!rowLayout) continue;

        for (int j = 0; j < rowLayout->count(); ++j) {
            QLayoutItem* bubbleItem = rowLayout->itemAt(j);
            if (!bubbleItem) continue;
            QWidget* w = bubbleItem->widget();
            if (!w) continue;
            auto* bubble = qobject_cast<ChatBubble*>(w);
            if (!bubble) continue;
            bubble->setMaximumWidth(maxBubbleW);
        }
    }
}

void ChatView::scrollToBottom() {
    QScrollBar* sb = verticalScrollBar();
    sb->setValue(sb->maximum());
}
