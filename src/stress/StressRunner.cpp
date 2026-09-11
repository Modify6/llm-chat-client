#include "stress/StressRunner.h"

#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMetaObject>
#include <chrono>
#include <mutex>
#include <thread>

void StressRunner::start(const Params& params) {
    m_stopped.store(false);

    auto startPoint = std::chrono::steady_clock::now();

    // 共享计数器(原子分配任务 + 统计结果)
    std::atomic<int> taskCounter{0};
    std::atomic<int> successCount{0};
    std::atomic<int> failCount{0};
    std::mutex latencyMutex;
    std::vector<double> allLatencies;  // 所有请求的延迟(毫秒)

    // 生成固定长度的提示词(简单重复,不占模型太多算力)
    std::string fixedPrompt = "Reply with OK only. ";
    for (int i = 0; i < params.promptTokens / 4; ++i) {
        fixedPrompt += "token ";
    }

    int totalRequests = params.totalRequests;
    int concurrency   = params.concurrency;

    // 启动 concurrency 个 worker
    auto watcher = new QFutureWatcher<void>(this);
    connect(watcher, &QFutureWatcher<void>::finished, this,
        [this, watcher, startPoint, &allLatencies, &successCount, &failCount,
         totalRequests]() {
            auto endPoint = std::chrono::steady_clock::now();
            double totalSec = std::chrono::duration<double>(endPoint - startPoint).count();

            StressReport report;
            report.successRequests = successCount.load();
            report.failedRequests  = failCount.load();
            report.totalTimeSec    = totalSec;
            report.latencies       = allLatencies;
            report.compute();

            emit finished(report);
            watcher->deleteLater();
        });

    watcher->setFuture(QtConcurrent::run([this, params, &taskCounter, &successCount, &failCount,
                                          &allLatencies, &latencyMutex, totalRequests,
                                          concurrency, fixedPrompt]() {
        // 创建一个独立的 LLMClient(每个 worker 不共享,线程安全)
        LLMClient client;
        client.setConfig(params.clientConfig);

        auto progressCounter = std::make_shared<std::atomic<int>>(0);

        // 启动 concurrency 个工作线程
        std::vector<std::thread> workers;
        for (int w = 0; w < concurrency; ++w) {
            workers.emplace_back([&, progressCounter]() {
                while (!m_stopped.load()) {
                    int myTask = taskCounter.fetch_add(1);
                    if (myTask >= totalRequests) break;

                    // 构建请求
                    std::vector<Message> history;
                    history.push_back(Message::system("You are a helpful assistant."));
                    history.push_back(Message::user(fixedPrompt));

                    auto t0 = std::chrono::steady_clock::now();
                    bool ok = false;
                    client.chat(history,
                        [&](const std::string&) { ok = true; },
                        [&](const std::string&) { ok = false; }
                    );
                    auto t1 = std::chrono::steady_clock::now();
                    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

                    if (ok) {
                        successCount.fetch_add(1);
                    } else {
                        failCount.fetch_add(1);
                    }

                    {
                        std::lock_guard<std::mutex> lock(latencyMutex);
                        allLatencies.push_back(ms);
                    }

                    // 进度信号:定期发(避免过于频繁)
                    int done = progressCounter->fetch_add(1) + 1;
                    if (done % 10 == 0 || done == totalRequests) {
                        emit progress(done, totalRequests);
                    }
                }
            });
        }

        for (auto& t : workers) t.join();

        // 最终进度
        emit progress(totalRequests, totalRequests);
    }));
}

void StressRunner::stop() {
    m_stopped.store(true);
}
