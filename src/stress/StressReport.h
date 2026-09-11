#pragma once

#include <string>
#include <vector>

/**
 * @brief 压测报告:汇总统计 + CSV 导出
 */
struct StressReport {
    int totalRequests   = 0;   ///< 总请求数
    int successRequests = 0;   ///< 成功数
    int failedRequests  = 0;   ///< 失败数
    double totalTimeSec = 0.0; ///< 总耗时(秒)
    double qps          = 0.0; ///< 每秒请求数
    double avgLatencyMs = 0.0; ///< 平均延迟
    double p50LatencyMs = 0.0; ///< P50 延迟
    double p95LatencyMs = 0.0; ///< P95 延迟
    double p99LatencyMs = 0.0; ///< P99 延迟
    std::vector<double> latencies; ///< 每次请求的延迟(毫秒),compute 前填充

    /// 从原始数据计算 QPS / 分位数 / 成功率
    void compute();

    /// 导出 CSV(含表头),返回 true=成功
    bool exportCsv(const std::string& path) const;
};
