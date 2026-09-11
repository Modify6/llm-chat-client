#include "ui/InputBox.h"

#include <QTextEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QShortcut>
#include <QKeySequence>

InputBox::InputBox(QWidget* parent)
    : QWidget(parent)
    , m_textEdit(new QTextEdit(this))
    , m_sendBtn(new QPushButton("发送", this))
    , m_clearBtn(new QPushButton("清空", this)) {

    m_textEdit->setPlaceholderText("输入消息...(Enter 发送,Shift+Enter 换行)");
    m_textEdit->setFixedHeight(80);
    m_textEdit->setAcceptRichText(false);

    // 按钮布局
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(m_clearBtn);
    btnLayout->addWidget(m_sendBtn);

    // 整体布局
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 8, 0, 0);
    mainLayout->setSpacing(6);
    mainLayout->addWidget(m_textEdit);
    mainLayout->addLayout(btnLayout);

    // Enter 键发送(拦截 Enter,Shift+Enter 放行)
    auto* sendShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    sendShortcut->setContext(Qt::WidgetShortcut);
    connect(sendShortcut, &QShortcut::activated, this, &InputBox::onSendClicked);

    connect(m_sendBtn, &QPushButton::clicked, this, &InputBox::onSendClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &InputBox::onClearClicked);
}

void InputBox::setSendEnabled(bool enabled) {
    // 停止模式下,按钮始终是"停止",不受 setSendEnabled 影响
    if (m_stopMode) return;
    m_sendBtn->setEnabled(enabled);
}

void InputBox::setStopMode(bool stop) {
    m_stopMode = stop;
    if (stop) {
        m_sendBtn->setText("停止");
        m_sendBtn->setEnabled(true);
        // 断开旧的 clicked → onSendClicked,连接到 stopRequested
        disconnect(m_sendBtn, &QPushButton::clicked, this, &InputBox::onSendClicked);
        connect(m_sendBtn, &QPushButton::clicked, this, [this]() { emit stopRequested(); });
    } else {
        m_sendBtn->setText("发送");
        m_sendBtn->setEnabled(true);
        disconnect(m_sendBtn, &QPushButton::clicked, nullptr, nullptr);
        connect(m_sendBtn, &QPushButton::clicked, this, &InputBox::onSendClicked);
    }
}

void InputBox::onSendClicked() {
    QString text = m_textEdit->toPlainText().trimmed();
    if (text.isEmpty()) return;
    m_textEdit->clear();
    emit sendRequested(text);
}

void InputBox::onClearClicked() {
    m_textEdit->clear();
}
