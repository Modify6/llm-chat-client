/**
 * @file MainWindow.h
 * @brief 主窗口 —— 应用的核心容器,负责 UI 布局 + 对话编排
 *
 * 本文件新增了【对话上下文记忆】相关成员:
 *   - m_session          : ChatSession 实例,持久保存 user/assistant 历史消息
 *   - m_pendingAssistantReply : 流式期间累积 assistant 回复的临时缓冲区
 *   - kMaxHistoryRounds  : 历史截断阈值(10 轮),防止 token 溢出 context window
 *
 * 改动前:每次 onSendRequested 只传 system + 当前 user,AI 完全不知道之前聊过什么
 * 改动后:用 m_session->history() 取完整历史 → 截断 → 加 system → 加当前 user
 *        流式完成/中断后,把完整(或带中断标记的)assistant 回复也存回 session
 */

#pragma once

#include <QMainWindow>
#include <memory>
#include <vector>

#include "core/Message.h"

class ChatView;
class InputBox;
class LLMClient;
class ChatSession;   // 新增:对话历史管理类(已实现于 core/ChatSession.h)
class StressPanel;
class QLabel;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    /**
     * @brief 用户点击发送(或按回车)时触发
     *
     * 完整流程:
     *  1. m_chatView 显示 user 气泡
     *  2. m_session->addMessage(user)       ← user 消息存入历史
     *  3. m_session->history()              ← 取历史(不含 system)
     *  4. 历史截断到最近 kMaxHistoryRounds 轮
     *  5. 拼 system + 截断后历史 + 当前 user → 构建最终 messages
     *  6. QtConcurrent::run 里调 m_client->chatStream(messages, ...)
     *     → 每个 token 通过 QueuedConnection 回到 UI 线程
     *     → 累积到 m_pendingAssistantReply + 显示到 ChatView
     *     → 完成/中断后把 assistant 回复也 addMessage 回 session
     */
    void onSendRequested(QString text);

    /** 用户点击"停止"按钮时触发 → m_client->cancel() 中止流式请求 */
    void onStopRequested();

    /**
     * @brief 流式 token 到达回调
     *
     * 这里做两件事:
     *  - m_pendingAssistantReply += token  // 累积完整回复(用于结束时存 session)
     *  - m_chatView->appendToLastAssistant(token)  // 实时显示到界面
     */
    void onTokenReceived(QString token);

    // ---------- 菜单栏槽 ----------
    void onOpenSettings();       ///< 连接配置对话框
    void onOpenStressPanel();    ///< 显示/激活右侧并发压测 Dock
    void onNewChat();            ///< 【新对话】清 session + 清 ChatView(快捷键 Ctrl+N)
    void onCopyAll();            ///< 【复制全部对话】收集所有气泡拼成 transcript 写入剪贴板

    /** 定时 healthCheck,更新右下角连接状态标签(每 30s) */
    void onConnectionCheck();

private:
    // ====== UI 组件 ======
    ChatView* m_chatView;        ///< 消息列表区(上)
    InputBox* m_inputBox;        ///< 输入框 + 发送/停止按钮(下)
    StressPanel* m_stressPanel;  ///< 并发压测面板(Dock,默认隐藏)

    // ====== 核心对象 ======
    std::unique_ptr<LLMClient> m_client;   ///< HTTP SSE 客户端(独占所有权)

    /**
     * @brief 对话历史持久化容器(新增)
     *
     * ChatSession 内部维护一个 vector<Message>,包含 system / user / assistant。
     * - addMessage(msg)     : 追加一条
     * - history()           : 返回不含 system 的 user+assistant 历史(LLMClient 需要)
     * - setSystemPrompt(s)  : 设置/替换 system prompt(始终在最前)
     * - clear()             : 清空全部
     * 注意:history() 返回值拷贝,当前对话轮数少,性能 OK;后续可改 const ref
     */
    ChatSession* m_session;

    // ====== 状态栏 ======
    QLabel* m_statusLabel;       ///< "● 已连接 / ● 未连接" 指示
    QTimer* m_statusTimer;       ///< 每 30s 触发 onConnectionCheck

    // ====== 流式状态 ======
    bool m_streaming = false;    ///< 是否正在流式(防止重复发送)

    /**
     * @brief 流式期间累积的 assistant 完整回复(新增)
     *
     * onTokenReceived 里 append,
     * onFinish 时整体 addMessage(assistant) 到 session,然后 clear。
     * onError(取消/网络错误)时,若此缓冲区非空 → 加 "[回复已中断]" 标记后再存 session。
     * 这样即使被中断,半截回复也会留在历史里,后续对话 AI 能看到。
     */
    QString m_pendingAssistantReply;

    /**
     * @brief 历史截断阈值:最多保留最近 N 轮(user+assistant 对)
     *
     * 为什么要截断?
     *   llama-server 默认 context window ≈ 4096 tokens。
     *   不截断 → 对话 20 轮后 token 溢出 → llama 返回错误或截断老消息。
     *
     * 截断策略:
     *   - system prompt 始终保留在最前
     *   - 只对 user+assistant 历史部分截断
     *   - 保留最近 kMaxHistoryRounds * 2 条(每轮 = 1 user + 1 assistant)
     *   - 当前 user 消息不算入历史(它是本次请求刚加的,还没配对)
     *
     * 10 轮 × ~300 tokens/轮 ≈ 3000 tokens,Q4_K_M 7B 模型安全。
     */
    static constexpr int kMaxHistoryRounds = 10;

    void setupMenuBar();
    void updateStatusLabel(bool connected);
};
