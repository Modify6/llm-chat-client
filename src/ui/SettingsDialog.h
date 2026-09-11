#pragma once

#include <QDialog>

class QLineEdit;
class QSpinBox;
class QDoubleSpinBox;

namespace httplib { class Client; }
class SSEParser;
class ConfigManager;

/**
 * @brief 连接配置对话框(host / port / model / temperature / max_tokens / timeout)
 */
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    /// 把对话框里的值应用到 ConfigManager
    void accept() override;

    /// 从 ConfigManager 加载当前配置填充对话框
    void reload();

private slots:
    void onTestConnection();

private:
    QLineEdit*     m_hostEdit;
    QSpinBox*      m_portSpin;
    QLineEdit*     m_modelEdit;
    QDoubleSpinBox* m_tempSpin;
    QSpinBox*      m_maxTokensSpin;
    QSpinBox*      m_timeoutSpin;
};
