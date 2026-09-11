#pragma once

#include <QWidget>

class QTextEdit;
class QPushButton;

/**
 * @brief 聊天输入区:多行文本框 + 发送/停止 + 清空按钮
 */
class InputBox : public QWidget {
    Q_OBJECT

public:
    explicit InputBox(QWidget* parent = nullptr);

    /// 设置发送按钮启用/禁用(请求期间禁用)
    void setSendEnabled(bool enabled);

    /// 切换按钮模式:true=显示"停止"按钮,false=显示"发送"按钮
    void setStopMode(bool stop);

signals:
    /// 用户请求发送(参数是当前输入文本,已 trim)
    void sendRequested(QString text);

    /// 用户请求停止流式生成
    void stopRequested();

private slots:
    void onSendClicked();
    void onClearClicked();

private:
    QTextEdit* m_textEdit;
    QPushButton* m_sendBtn;
    QPushButton* m_clearBtn;
    bool m_stopMode = false;
};
