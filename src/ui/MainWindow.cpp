/**
 * @file MainWindow.cpp
 * @brief 主窗口实现 —— UI 布局、对话编排、历史管理
 *
 * ======= 改动说明 =======
 *
 * 【改动前的问题】
 * onSendRequested() 每次只传 2 条消息:
 *   system("You are a helpful assistant.") + user("帮我写个智能指针")
 * AI 每次都从零开始,完全不知道之前聊过什么 —— 多轮对话前言不搭后语。
 *
 * 【改动后的方案】
 * 接入了已经存在但没被使用的 ChatSession 类来持久保存对话历史:
 *
 *   发送时  →  m_session->history() 取历史 → 截断 → 加 system → 加当前 user → chatStream
 *   流式中  →  每个 token 累积到 m_pendingAssistantReply + 实时显示
 *   完成时  →  m_session->addMessage(assistant完整回复)
 *   中断时  →  m_session->addMessage(assistant半截回复 + [回复已中断]标记)
 *
 * 历史截断:默认保留最近 10 轮(user+assistant 对),防止 token 溢出 4096 context window。
 *
 * 【线程安全】
 * m_session 的 addMessage/history 都在 UI 线程调用:
 *   - onSendRequested      是 slot,UI 线程
 *   - onTokenReceived      通过 QueuedConnection 回到 UI 线程
 *   - onFinish/onError     用 QMetaObject::invokeMethod(QueuedConnection) 回到 UI 线程
 * 所以不需要锁。
 */

#include "ui/MainWindow.h"
#include "ui/ChatView.h"
#include "ui/InputBox.h"
#include "ui/SettingsDialog.h"
#include "ui/StressPanel.h"
#include "core/LLMClient.h"
#include "core/Message.h"
#include "core/ChatSession.h"   // 新增:对话历史管理
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
#include <QApplication>
#include <QClipboard>

// =====================================================================
// 构造 / 析构
// =====================================================================

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_chatView(new ChatView(this))
    , m_inputBox(new InputBox(this))
    , m_client(std::make_unique<LLMClient>(this))
    , m_session(new ChatSession(this))   // 新增:对话历史容器
    , m_statusLabel(new QLabel("● 检查中...", this))
    , m_statusTimer(new QTimer(this)) {

    setWindowTitle("LLM Chat Client");
    resize(800, 600);

    // === ChatSession 初始化 ===
    // setSystemPrompt 会把 system 消息放在 m_messages[0],后续 addMessage 追加在后面
    m_session->setSystemPrompt("You are a helpful assistant.");

    // === 菜单栏 ===
    setupMenuBar();

    // === 中心布局:上 ChatView,下 InputBox ===
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_chatView, 1);   // stretch=1,占满剩余空间
    layout->addWidget(m_inputBox);
    setCentralWidget(central);

    // === 状态栏 ===
    statusBar()->addWidget(m_statusLabel);

    // === 应用已保存的连接配置(host、model、api key) ===
    auto savedCfg = ConfigManager::instance().load();
    m_client->setConfig(savedCfg);

    // === 信号连接 ===
    connect(m_inputBox, &InputBox::sendRequested,
            this, &MainWindow::onSendRequested);
    connect(m_inputBox, &InputBox::stopRequested,
            this, &MainWindow::onStopRequested);

    // tokenReceived 在后台线程 emit,QueuedConnection 自动排到 UI 线程队列
    connect(m_client.get(), &LLMClient::tokenReceived,
            this, &MainWindow::onTokenReceived,
            Qt::QueuedConnection);

    // === 定期连接检查(后台线程 healthCheck) ===
    onConnectionCheck();
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::onConnectionCheck);
    m_statusTimer->start(30000);

    // === 压测面板 Dock(默认隐藏,Ctrl+T 呼出) ===
    m_stressPanel = new StressPanel(m_client.get(), this);
    auto* dock = new QDockWidget("并发压测", this);
    dock->setWidget(m_stressPanel);
    dock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->hide();
}

MainWindow::~MainWindow() = default;

// =====================================================================
// 菜单栏
// =====================================================================

void MainWindow::setupMenuBar() {
    auto* menuBar = this->menuBar();

    // === 编辑菜单 ===
    // 提供"复制全部对话"—— 一键把当前 ChatView 里所有气泡拼成 transcript
    auto* editMenu = menuBar->addMenu("编辑(&E)");

    auto* copyAllAction = new QAction("复制全部对话(A)", this);
    copyAllAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C));
    copyAllAction->setStatusTip("复制当前所有对话到剪贴板");
    connect(copyAllAction, &QAction::triggered, this, &MainWindow::onCopyAll);
    editMenu->addAction(copyAllAction);

    // === 设置菜单 ===
    auto* settingsMenu = menuBar->addMenu("设置(&S)");

    // 【新对话】—— 清空 session + ChatView
    auto* newChatAction = new QAction("新对话(N)", this);
    newChatAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    connect(newChatAction, &QAction::triggered, this, &MainWindow::onNewChat);
    settingsMenu->addAction(newChatAction);

    settingsMenu->addSeparator();

    auto* openSettings = new QAction("连接配置...", this);
    openSettings->setShortcut(QKeySequence::Preferences);
    connect(openSettings, &QAction::triggered, this, &MainWindow::onOpenSettings);
    settingsMenu->addAction(openSettings);

    auto* toolsMenu = menuBar->addMenu("工具(&T)");
    auto* openStress = new QAction("并发压测...", this);
    openStress->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(openStress, &QAction::triggered, this, &MainWindow::onOpenStressPanel);
    toolsMenu->addAction(openStress);
}

void MainWindow::onOpenStressPanel() {
    for (auto* dock : findChildren<QDockWidget*>()) {
        if (dock->widget() == m_stressPanel) {
            dock->show(); dock->raise(); dock->setFocus();
            return;
        }
    }
}

void MainWindow::onOpenSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        auto newCfg = ConfigManager::instance().load();
        m_client->setConfig(newCfg);
        onConnectionCheck();
    }
}

/**
 * @brief 新对话:清历史 + 清界面
 *
 * 注意:流式中(m_streaming==true)拒绝执行,否则会丢失正在累积的 assistant 回复。
 * 等用户点停止或自然结束后才能清空。
 */
void MainWindow::onNewChat() {
    if (m_streaming) return;
    m_session->clear();
    m_session->setSystemPrompt("You are a helpful assistant.");
    m_chatView->clearAll();
}

/**
 * @brief 复制全部对话到剪贴板
 *
 * 格式示例:
 *   【用户】
 *   帮我写一个 C++ 智能指针
 *
 *   【AI】
 *   好的,这里是一个简单的智能指针实现:...
 *   ```cpp
 *   template<typename T>
 *   class UniquePtr { ... };
 *   ```
 *
 * 实现:遍历 ChatView::allBubbles()(维护 row→bubble 结构),
 *       取每个 ChatBubble::rawText() 拼成 transcript。
 */
void MainWindow::onCopyAll() {
    auto bubbles = m_chatView->allBubbles();
    if (bubbles.isEmpty()) return;

    QStringList transcript;
    for (ChatBubble* bubble : bubbles) {
        QString roleLabel = (bubble->role() == ChatBubble::Role::User)
            ? QStringLiteral("用户")
            : QStringLiteral("AI");
        QString text = bubble->rawText().trimmed();
        if (text.isEmpty()) continue;   // 跳过流式中的空气泡
        transcript.append(QString("【%1】\n%2").arg(roleLabel, text));
    }

    if (transcript.isEmpty()) return;

    QApplication::clipboard()->setText(transcript.join("\n\n"));
}

// =====================================================================
// 状态栏
// =====================================================================

void MainWindow::updateStatusLabel(bool connected) {
    if (connected) {
        m_statusLabel->setText("● 已连接");
        m_statusLabel->setStyleSheet("color: #2ecc71;");   // 绿
    } else {
        m_statusLabel->setText("● 未连接");
        m_statusLabel->setStyleSheet("color: #e74c3c;");   // 红
    }
}

/**
 * @brief 后台线程 healthCheck,每 30s 触发
 *
 * 为什么用 QFutureWatcher + QtConcurrent?
 *   healthCheck 是 HTTP 请求,网络超时可能几秒,放 UI 线程会卡死界面。
 *   QtConcurrent::run 自动丢到全局线程池,QFutureWatcher 用信号通知 UI 线程。
 */
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

// =====================================================================
// 核心对话逻辑(带上下文记忆 + 历史截断)
// =====================================================================

/**
 * @brief 发送消息 —— 主流程
 *
 * 调用时机:InputBox 发出 sendRequested(text)
 *
 * 完整步骤:
 *  ① UI 状态切换:m_streaming=true,发送按钮→停止按钮
 *  ② ChatView 显示 user 气泡
 *  ③ 清空 m_pendingAssistantReply(准备接收新的流式回复)
 *  ④ 构建历史 messages:
 *     a. m_session->history()  → 取历史(不含 system,只含 user+assistant)
 *     b. 历史截断到最近 kMaxHistoryRounds * 2 条
 *     c. 前端加 system prompt,后端加当前 user 消息
 *  ⑤ 把当前 user 消息也 addMessage 回 session(保证持久化)
 *  ⑥ QtConcurrent::run 启动后台线程调 chatStream:
 *     - onToken:  emit tokenReceived → UI 线程累积 + 显示
 *     - onFinish: 存 assistant 完整回复 → 恢复 UI 状态
 *     - onError:  存 assistant 半截回复(加 [回复已中断] 标记) → 恢复 UI 状态
 */
void MainWindow::onSendRequested(QString text) {
    if (m_streaming) return;

    m_streaming = true;
    m_inputBox->setStopMode(true);
    m_inputBox->setSendEnabled(false);

    m_chatView->addMessage(ChatBubble::Role::User, text);
    m_chatView->appendToLastAssistant("");   // 先占一个空 assistant 气泡

    // 清空上一轮可能残留的累积缓冲区
    m_pendingAssistantReply.clear();

    // ---------- ④ 构建带历史的 messages ----------
    // ChatSession::history() 内部过滤了 system,返回纯 user+assistant 的 vector<Message>
    auto history = m_session->history();

    // 历史截断:只保留最近 kMaxHistoryRounds 轮(10 轮 = 20 条 user+assistant)
    // 为什么是 * 2? 每轮 = 1 条 user + 1 条 assistant
    // 截断方向:从头部删老的,保留尾部新的
    if (history.size() > kMaxHistoryRounds * 2) {
        int keep = kMaxHistoryRounds * 2;
        history.erase(history.begin(), history.end() - keep);
    }

    // 最终拼装:system + 截断后历史 + 当前 user
    std::vector<Message> messages;
    messages.push_back(Message::system("You are a helpful assistant."));
    for (const auto& m : history) {
        messages.push_back(m);
    }
    messages.push_back(Message::user(text.toStdString()));

    // ---------- ⑤ user 消息存 session ----------
    // 这样 history() 下次就能取到这条,形成闭环
    m_session->addMessage(Message::user(text.toStdString()));

    // ---------- ⑥ 后台线程流式请求 ----------
    // chatStream 的三个回调都在后台线程执行(由 LLMClient 内部实现决定)
    // 所以 tokenReceived 用 Qt::QueuedConnection 自动切回 UI 线程
    // onFinish/onError 里的 UI 操作则手动 QMetaObject::invokeMethod(QueuedConnection)
    QtConcurrent::run([this, messages]() {
        m_client->chatStream(messages,
            // --- onToken:每个 token 到达 ---
            [this](const std::string& token) {
                // token 可能含 UTF-8 多字节字符(中文),用 fromUtf8 而非 fromLocal8Bit
                emit m_client->tokenReceived(
                    QString::fromUtf8(token.c_str(), static_cast<int>(token.size())));
            },

            // --- onFinish:正常结束 ---
            [this]() {
                QMetaObject::invokeMethod(this, [this]() {
                    // 把完整的 assistant 回复存入 session
                    // 这样下一轮对话时,AI 就能看到自己上轮说了什么
                    if (!m_pendingAssistantReply.isEmpty()) {
                        m_session->addMessage(Message::assistant(
                            m_pendingAssistantReply.toStdString()));
                    }
                    m_streaming = false;
                    m_inputBox->setStopMode(false);
                    m_inputBox->setSendEnabled(true);
                    m_chatView->finishAssistant();
                }, Qt::QueuedConnection);
            },

            // --- onError:取消 或 网络错误 ---
            [this](const std::string& err) {
                QMetaObject::invokeMethod(this, [this, err]() {
                    // 即使被中断,也把已收到的半截回复存进 session
                    // 这样用户续问时,AI 知道刚才说到哪儿了
                    if (!m_pendingAssistantReply.isEmpty()) {
                        QString marked = m_pendingAssistantReply;
                        marked += "\n\n[回复已中断]";
                        m_session->addMessage(Message::assistant(marked.toStdString()));
                    }
                    // 非取消错误才显示到界面(用户主动 cancel 就不打扰了)
                    if (!err.empty() && err.find("取消") == std::string::npos) {
                        m_chatView->appendToLastAssistant(
                            QString("\n[Error] %1").arg(QString::fromStdString(err)));
                    }
                    m_streaming = false;
                    m_inputBox->setStopMode(false);
                    m_inputBox->setSendEnabled(true);
                    m_chatView->finishAssistant();
                }, Qt::QueuedConnection);
            }
        );
    });
}

/** 用户点停止 → 通知 LLMClient 中止 SSE 连接 → onError 回调触发 */
void MainWindow::onStopRequested() {
    if (!m_streaming) return;
    m_client->cancel();
}

/**
 * @brief 流式 token 到达(UI 线程)
 *
 * 两件事:
 *   1. 累积到 m_pendingAssistantReply —— 结束时整体 addMessage 回 session
 *   2. 实时追加到 ChatView 的最后一个 assistant 气泡 —— 用户即时看到输出
 */
void MainWindow::onTokenReceived(QString token) {
    m_pendingAssistantReply += token;
    m_chatView->appendToLastAssistant(token);
}
