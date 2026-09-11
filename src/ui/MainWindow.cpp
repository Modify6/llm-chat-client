#include "ui/MainWindow.h"
#include "ui/ChatView.h"
#include "ui/InputBox.h"
#include "ui/SettingsDialog.h"
#include "ui/StressPanel.h"
#include "core/LLMClient.h"
#include "core/Message.h"
#include "utils/ConfigManager.h"

#include <QVBoxLayout>
#include <QStatusBar>
#include <QLabel>
#include <QtConcurrent>
#include <QTimer>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDockWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_chatView(new ChatView(this))
    , m_inputBox(new InputBox(this))
    , m_client(std::make_unique<LLMClient>(this))
    , m_statusLabel(new QLabel("● 检查中...", this))
    , m_statusTimer(new QTimer(this)) {

    setWindowTitle("LLM Chat Client");
    resize(800, 600);

    // === 菜单栏 ===
    setupMenuBar();

    // === 中心布局 ===
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_chatView, 1);
    layout->addWidget(m_inputBox);
    setCentralWidget(central);

    // === 状态栏 ===
    statusBar()->addWidget(m_statusLabel);

    // === 应用已保存的配置 ===
    auto savedCfg = ConfigManager::instance().load();
    m_client->setConfig(savedCfg);

    // === 信号连接 ===
    connect(m_inputBox, &InputBox::sendRequested,
            this, &MainWindow::onSendRequested);
    connect(m_inputBox, &InputBox::stopRequested,
            this, &MainWindow::onStopRequested);

    connect(m_client.get(), &LLMClient::tokenReceived,
            this, &MainWindow::onTokenReceived,
            Qt::QueuedConnection);

    // === 定期连接检查 ===
    // 启动时立即检查一次,然后每 30 秒轮询
    onConnectionCheck();
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::onConnectionCheck);
    m_statusTimer->start(30000);

    // === 压测面板(DockWidget,默认隐藏) ===
    m_stressPanel = new StressPanel(m_client.get(), this);
    auto* dock = new QDockWidget("并发压测", this);
    dock->setWidget(m_stressPanel);
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();  // 默认隐藏,用户通过菜单打开
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenuBar() {
    auto* menuBar = this->menuBar();
    auto* settingsMenu = menuBar->addMenu("设置(&S)");

    auto* openSettings = new QAction("连接配置...", this);
    openSettings->setShortcut(QKeySequence::Preferences);
    connect(openSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);
    settingsMenu->addAction(openSettings);

    // 压测菜单
    auto* toolsMenu = menuBar->addMenu("工具(&T)");
    auto* openStress = new QAction("并发压测...", this);
    openStress->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(openStress, &QAction::triggered, this, &MainWindow::onOpenStressPanel);
    toolsMenu->addAction(openStress);
}

void MainWindow::onOpenStressPanel() {
    // 找到承载 m_stressPanel 的 DockWidget 并显示
    for (auto* dock : findChildren<QDockWidget*>()) {
        if (dock->widget() == m_stressPanel) {
            dock->show();
            dock->raise();
            dock->setFocus();
            return;
        }
    }
}

void MainWindow::onOpenSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        // 用户点了确定,配置已保存到 ConfigManager
        auto newCfg = ConfigManager::instance().load();
        m_client->setConfig(newCfg);
        // 立即做一次连接检查
        onConnectionCheck();
    }
}

void MainWindow::updateStatusLabel(bool connected) {
    if (connected) {
        m_statusLabel->setText("● 已连接");
        m_statusLabel->setStyleSheet("color: #2ecc71;");
    } else {
        m_statusLabel->setText("● 未连接");
        m_statusLabel->setStyleSheet("color: #e74c3c;");
    }
}

void MainWindow::onConnectionCheck() {
    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this,
        [this, watcher]() {
            updateStatusLabel(watcher->result());
            watcher->deleteLater();
        });

    LLMClient::Config cfg = m_client->config();
    watcher->setFuture(QtConcurrent::run([cfg]() -> bool {
        LLMClient probe;
        probe.setConfig(cfg);
        bool ok = false;
        probe.healthCheck(
            [&](const std::string&) { ok = true; },
            [&](const std::string&) { ok = false; }
        );
        return ok;
    }));
}

// ============================================================
// 对话逻辑(同阶段 5,不变)
// ============================================================

void MainWindow::onSendRequested(QString text) {
    if (m_streaming) return;

    m_streaming = true;
    m_inputBox->setStopMode(true);
    m_inputBox->setSendEnabled(false);

    m_chatView->addMessage(ChatBubble::Role::User, text);
    m_chatView->appendToLastAssistant("");

    std::vector<Message> history;
    history.push_back(Message::system("You are a helpful assistant."));
    history.push_back(Message::user(text.toStdString()));

    QtConcurrent::run([this, history]() {
        m_client->chatStream(history,
            [this](const std::string& token) {
                emit m_client->tokenReceived(QString::fromUtf8(token.c_str(),
                                                               static_cast<int>(token.size())));
            },
            [this]() {
                QMetaObject::invokeMethod(this, [this]() {
                    m_streaming = false;
                    m_inputBox->setStopMode(false);
                    m_inputBox->setSendEnabled(true);
                    m_chatView->finishAssistant();
                }, Qt::QueuedConnection);
            },
            [this](const std::string& err) {
                QMetaObject::invokeMethod(this, [this, err]() {
                    m_streaming = false;
                    m_inputBox->setStopMode(false);
                    m_inputBox->setSendEnabled(true);
                    if (!err.empty() && err.find("取消") == std::string::npos) {
                        m_chatView->appendToLastAssistant(
                            QString("\n[Error] %1").arg(QString::fromStdString(err)));
                    }
                    m_chatView->finishAssistant();
                }, Qt::QueuedConnection);
            }
        );
    });
}

void MainWindow::onStopRequested() {
    if (!m_streaming) return;
    m_client->cancel();
}

void MainWindow::onTokenReceived(QString token) {
    m_chatView->appendToLastAssistant(token);
}
