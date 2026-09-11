#include "ui/ChatBubble.h"

#include <QHBoxLayout>

ChatBubble* ChatBubble::user(const QString& text, QWidget* parent) {
    return new ChatBubble(Role::User, text, parent);
}

ChatBubble* ChatBubble::assistant(const QString& text, QWidget* parent) {
    return new ChatBubble(Role::Assistant, text, parent);
}

ChatBubble::ChatBubble(Role role, const QString& text, QWidget* parent)
    : QFrame(parent)
    , m_role(role)
    , m_label(new QLabel(text, this)) {

    m_label->setWordWrap(true);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // 限制气泡最大宽度为容器 70%,防止超长行撑满
    setMaximumWidth(parent ? static_cast<int>(parent->width() * 0.7) : 600);

    // 用属性选择器让 style.qss 区分 user / assistant
    setProperty("role", (role == Role::User) ? "user" : "assistant");

    // 布局:user 右对齐,assistant 左对齐
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    if (role == Role::User) {
        layout->setAlignment(m_label, Qt::AlignRight);
        setLayoutDirection(Qt::RightToLeft);
    } else {
        layout->setAlignment(m_label, Qt::AlignLeft);
    }
}

void ChatBubble::appendText(const QString& text) {
    m_label->setText(m_label->text() + text);
}
