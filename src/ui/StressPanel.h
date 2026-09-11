#pragma once

#include <QWidget>
#include "stress/StressReport.h"  // 值类型成员,必须完整定义

class QSpinBox;
class QLabel;
class QProgressBar;
class QPushButton;
class LLMClient;
class StressRunner;

/**
 * @brief 并发压测面板
 *
 * 输入并发数 / 总请求数 / 提示长度,点击开始运行,
 * 实时显示进度 / QPS / P50 / P99 / 成功率,支持导出 CSV。
 */
class StressPanel : public QWidget {
    Q_OBJECT

public:
    explicit StressPanel(LLMClient* client, QWidget* parent = nullptr);

private slots:
    void onStart();
    void onStop();
    void onExportCsv();
    void onProgress(int done, int total);
    void onFinished(class StressReport report);
    void onError(QString msg);

private:
    LLMClient* m_client;
    StressRunner* m_runner;

    // 输入
    QSpinBox* m_concurrencySpin;
    QSpinBox* m_totalRequestsSpin;
    QSpinBox* m_promptTokensSpin;

    // 按钮
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_exportBtn;

    // 进度
    QProgressBar* m_progressBar;

    // 结果标签
    QLabel* m_qpsLabel;
    QLabel* m_avgLabel;
    QLabel* m_p50Label;
    QLabel* m_p95Label;
    QLabel* m_p99Label;
    QLabel* m_successRateLabel;
    QLabel* m_totalTimeLabel;

    StressReport m_lastReport;   ///< 最近一次完成的报告
};
