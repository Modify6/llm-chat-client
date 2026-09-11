#pragma once

#include <QObject>
#include <atomic>
#include <memory>
#include <string>
#include <functional>

#include "stress/StressReport.h"
#include "core/LLMClient.h"

/**
 * @brief 并发压测执行器
 *
 * 用 QtConcurrent::run 启动 N 个 worker,每个 worker 循环请求直到总数完成。
 * 用 std::atomic<int> 分配任务,用 std::atomic<bool> 支持停止。
 */
class StressRunner : public QObject {
    Q_OBJECT

public:
    explicit StressRunner(QObject* parent = nullptr) : QObject(parent) {}

    struct Params {
        int concurrency   = 8;     ///< 并发线程数
        int totalRequests = 100;   ///< 总请求数
        int promptTokens  = 32;    ///< 输入长度(用于生成固定提示)
        LLMClient::Config clientConfig;  ///< 连接配置
    };

    void start(const Params& params);
    void stop();

signals:
    /// 进度回调:已完成 / 总数
    void progress(int done, int total);

    /// 完成回调:报告结果
    void finished(StressReport report);

    /// 错误回调
    void error(QString msg);

private:
    std::atomic<bool> m_stopped{false};
};
