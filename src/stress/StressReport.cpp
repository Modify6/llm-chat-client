#include "stress/StressReport.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

void StressReport::compute() {
    totalRequests = successRequests + failedRequests;

    // QPS
    qps = (totalTimeSec > 0) ? (static_cast<double>(totalRequests) / totalTimeSec) : 0.0;

    // 平均延迟
    if (!latencies.empty()) {
        double sum = 0.0;
        for (double v : latencies) sum += v;
        avgLatencyMs = sum / latencies.size();

        // 排序后算分位数
        std::sort(latencies.begin(), latencies.end());
        auto percentile = [&](double p) -> double {
            if (latencies.empty()) return 0.0;
            double idx = p * (latencies.size() - 1);
            size_t lo = static_cast<size_t>(idx);
            size_t hi = std::min(lo + 1, latencies.size() - 1);
            double frac = idx - lo;
            return latencies[lo] * (1.0 - frac) + latencies[hi] * frac;
        };
        p50LatencyMs = percentile(0.50);
        p95LatencyMs = percentile(0.95);
        p99LatencyMs = percentile(0.99);
    }
}

bool StressReport::exportCsv(const std::string& path) const {
    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;

    ofs << std::fixed << std::setprecision(2);

    ofs << "metric,value\n";
    ofs << "total_requests,"   << totalRequests   << "\n";
    ofs << "success_requests," << successRequests << "\n";
    ofs << "failed_requests,"  << failedRequests  << "\n";
    ofs << "total_time_sec,"   << totalTimeSec    << "\n";
    ofs << "qps,"              << qps             << "\n";
    ofs << "avg_latency_ms,"   << avgLatencyMs    << "\n";
    ofs << "p50_latency_ms,"   << p50LatencyMs    << "\n";
    ofs << "p95_latency_ms,"   << p95LatencyMs    << "\n";
    ofs << "p99_latency_ms,"   << p99LatencyMs    << "\n";
    ofs << "success_rate_pct," << (totalRequests > 0 ? (100.0 * successRequests / totalRequests) : 0.0) << "\n";

    ofs.close();
    return true;
}
