#include "ui/SettingsDialog.h"
#include "utils/ConfigManager.h"
#include "core/LLMClient.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QLabel>
#include <QtConcurrent>
#include <QFutureWatcher>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent) {

    setWindowTitle("连接配置");
    resize(420, 340);

    // 表单布局
    auto* form = new QFormLayout();

    m_hostEdit = new QLineEdit(this);
    form->addRow("主机:", m_hostEdit);

    m_portSpin = new QSpinBox(this);
    m_portSpin->setRange(1, 65535);
    form->addRow("端口:", m_portSpin);

    m_modelEdit = new QLineEdit(this);
    m_modelEdit->setPlaceholderText("qwen2.5-7b");
    form->addRow("模型名:", m_modelEdit);

    m_tempSpin = new QDoubleSpinBox(this);
    m_tempSpin->setRange(0.0, 2.0);
    m_tempSpin->setSingleStep(0.1);
    m_tempSpin->setDecimals(2);
    form->addRow("Temperature:", m_tempSpin);

    m_maxTokensSpin = new QSpinBox(this);
    m_maxTokensSpin->setRange(1, 32768);
    m_maxTokensSpin->setSingleStep(256);
    form->addRow("Max Tokens:", m_maxTokensSpin);

    m_timeoutSpin = new QSpinBox(this);
    m_timeoutSpin->setRange(5, 3600);
    m_timeoutSpin->setSingleStep(30);
    form->addRow("超时(秒):", m_timeoutSpin);

    // 测试连接按钮
    auto* testBtn = new QPushButton("测试连接", this);
    connect(testBtn, &QPushButton::clicked, this, &SettingsDialog::onTestConnection);

    // 按钮框
    auto* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // 主布局
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(form);

    auto* bottomRow = new QHBoxLayout();
    bottomRow->addWidget(testBtn);
    bottomRow->addStretch();
    mainLayout->addLayout(bottomRow);

    mainLayout->addWidget(buttonBox);

    // 加载当前配置
    reload();
}

void SettingsDialog::reload() {
    auto cfg = ConfigManager::instance().load();
    m_hostEdit->setText(QString::fromStdString(cfg.host));
    m_portSpin->setValue(cfg.port);
    m_modelEdit->setText(QString::fromStdString(cfg.model));
    m_tempSpin->setValue(cfg.temperature);
    m_maxTokensSpin->setValue(cfg.max_tokens);
    m_timeoutSpin->setValue(cfg.timeout_seconds);
}

void SettingsDialog::accept() {
    // 校验
    QString host = m_hostEdit->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, "输入校验", "主机地址不能为空");
        return;
    }

    LLMClient::Config cfg;
    cfg.host            = host.toStdString();
    cfg.port            = m_portSpin->value();
    cfg.model           = m_modelEdit->text().trimmed().toStdString();
    cfg.temperature     = m_tempSpin->value();
    cfg.max_tokens      = m_maxTokensSpin->value();
    cfg.timeout_seconds = m_timeoutSpin->value();

    ConfigManager::instance().save(cfg);

    QDialog::accept();
}

void SettingsDialog::onTestConnection() {
    LLMClient::Config cfg;
    cfg.host = m_hostEdit->text().trimmed().toStdString();
    cfg.port = m_portSpin->value();
    cfg.timeout_seconds = 5;

    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
        bool ok = watcher->result();
        if (ok) {
            QMessageBox::information(this, "连接测试", "✓ 连接成功!");
        } else {
            QMessageBox::warning(this, "连接测试", "✗ 连接失败,请检查地址和端口");
        }
        watcher->deleteLater();
    });

    watcher->setFuture(QtConcurrent::run([cfg]() -> bool {
        LLMClient client;
        client.setConfig(cfg);
        bool ok = false;
        client.healthCheck(
            [&](const std::string&) { ok = true; },
            [&](const std::string&) { ok = false; }
        );
        return ok;
    }));
}
