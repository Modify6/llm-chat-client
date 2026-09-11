#pragma once

#include <QMainWindow>
#include <memory>

class ChatView;
class InputBox;
class LLMClient;
class StressPanel;
class QLabel;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onSendRequested(QString text);
    void onStopRequested();
    void onTokenReceived(QString token);

    // 菜单项
    void onOpenSettings();
    void onOpenStressPanel();

    // 定期连接状态检查
    void onConnectionCheck();

private:
    ChatView* m_chatView;
    InputBox* m_inputBox;
    StressPanel* m_stressPanel;   ///< 压测面板(DockWidget)
    std::unique_ptr<LLMClient> m_client;

    QLabel* m_statusLabel;      ///< 状态栏连接状态标签
    QTimer* m_statusTimer;       ///< 定期 healthCheck 定时器

    bool m_streaming = false;    ///< 当前是否在流式生成中

    /// 初始化菜单栏
    void setupMenuBar();

    /// 更新状态栏连接状态
    void updateStatusLabel(bool connected);
};
