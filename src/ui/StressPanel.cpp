#include "ui/StressPanel.h"
#include "stress/StressRunner.h"
#include "stress/StressReport.h"
#include "core/LLMClient.h"

#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>

StressPanel::StressPanel(LLMClient* client, QWidget* parent)
    : QWidget(parent)
    , m_client(client)
    , m_runner(new StressRunner(this)) {

    auto* mainLayout = new QVBoxLayout(this);

    // === 输入组 ===
    auto* inputGroup = new QGroupBox("压测参数", this);
    auto* grid = new QGridLayout(inputGroup);

    grid->addWidget(new QLabel("并发数:", this), 0, 0);
    m_concurrencySpin = new QSpinBox(this);
    m_concurrencySpin->setRange(1, 256);
    m_concurrencySpin->setValue(8);
    grid->addWidget(m_concurrencySpin, 0, 1);

    grid->addWidget(new QLabel("总请求数:", this), 0, 2);
    m_totalRequestsSpin = new QSpinBox(this);
    m_totalRequestsSpin->setRange(1, 100000);
    m_totalRequestsSpin->setValue(100);
    grid->addWidget(m_totalRequestsSpin, 0, 3);

    grid->addWidget(new QLabel("输入长度(tokens):", this), 1, 0);
    m_promptTokensSpin = new QSpinBox(this);
    m_promptTokensSpin->setRange(4, 4096);
    m_promptTokensSpin->setValue(32);
    grid->addWidget(m_promptTokensSpin, 1, 1);

    mainLayout->addWidget(inputGroup);

    // === 按钮组 ===
    auto* btnRow = new QHBoxLayout();
    m_startBtn  = new QPushButton("开始压测", this);
    m_stopBtn   = new QPushButton("停止", this);
    m_stopBtn->setEnabled(false);
    m_exportBtn = new QPushButton("导出 CSV", this);
    m_exportBtn->setEnabled(false);
    btnRow->addWidget(m_startBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_exportBtn);
    mainLayout->addLayout(btnRow);

    // === 进度条 ===
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    mainLayout->addWidget(m_progressBar);

    // === 结果组 ===
    auto* resultGroup = new QGroupBox("压测结果", this);
    auto* resultGrid = new QGridLayout(resultGroup);
    int r = 0;

    resultGrid->addWidget(new QLabel("QPS:", this), r, 0);
    m_qpsLabel = new QLabel("-", this);
    resultGrid->addWidget(m_qpsLabel, r, 1);

    resultGrid->addWidget(new QLabel("平均延迟:", this), r, 2);
    m_avgLabel = new QLabel("-", this);
    resultGrid->addWidget(m_avgLabel, r, 3);

    ++r;
    resultGrid->addWidget(new QLabel("P50:", this), r, 0);
    m_p50Label = new QLabel("-", this);
    resultGrid->addWidget(m_p50Label, r, 1);

    resultGrid->addWidget(new QLabel("P95:", this), r, 2);
    m_p95Label = new QLabel("-", this);
    resultGrid->addWidget(m_p95Label, r, 3);

    ++r;
    resultGrid->addWidget(new QLabel("P99:", this), r, 0);
    m_p99Label = new QLabel("-", this);
    resultGrid->addWidget(m_p99Label, r, 1);

    resultGrid->addWidget(new QLabel("成功率:", this), r, 2);
    m_successRateLabel = new QLabel("-", this);
    resultGrid->addWidget(m_successRateLabel, r, 3);

    ++r;
    resultGrid->addWidget(new QLabel("总耗时:", this), r, 0);
    m_totalTimeLabel = new QLabel("-", this);
    resultGrid->addWidget(m_totalTimeLabel, r, 1);

    mainLayout->addWidget(resultGroup);
    mainLayout->addStretch();

    // === 信号连接 ===
    connect(m_startBtn, &QPushButton::clicked, this, &StressPanel::onStart);
    connect(m_stopBtn,  &QPushButton::clicked, this, &StressPanel::onStop);
    connect(m_exportBtn, &QPushButton::clicked, this, &StressPanel::onExportCsv);

    connect(m_runner, &StressRunner::progress, this, &StressPanel::onProgress);
    connect(m_runner, &StressRunner::finished, this, &StressPanel::onFinished);
    connect(m_runner, &StressRunner::error,   this, &StressPanel::onError);
}

void StressPanel::onStart() {
    StressRunner::Params p;
    p.concurrency   = m_concurrencySpin->value();
    p.totalRequests = m_totalRequestsSpin->value();
    p.promptTokens  = m_promptTokensSpin->value();
    p.clientConfig  = m_client->config();

    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_exportBtn->setEnabled(false);
    m_progressBar->setValue(0);

    // 清空结果标签
    m_qpsLabel->setText("-");
    m_avgLabel->setText("-");
    m_p50Label->setText("-");
    m_p95Label->setText("-");
    m_p99Label->setText("-");
    m_successRateLabel->setText("-");
    m_totalTimeLabel->setText("-");

    m_runner->start(p);
}

void StressPanel::onStop() {
    m_runner->stop();
}

void StressPanel::onProgress(int done, int total) {
    if (total > 0) {
        m_progressBar->setRange(0, total);
        m_progressBar->setValue(done);
    }
}

void StressPanel::onFinished(StressReport report) {
    m_lastReport = report;

    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_exportBtn->setEnabled(true);

    auto fmtMs = [](double v) { return QString("%1 ms").arg(v, 0, 'f', 2); };
    auto fmtQps = [](double v) { return QString("%1").arg(v, 0, 'f', 2); };
    auto fmtPct = [](int ok, int total) {
        if (total == 0) return QString("-");
        return QString("%1%").arg(100.0 * ok / total, 0, 'f', 1);
    };

    m_qpsLabel->setText(fmtQps(report.qps));
    m_avgLabel->setText(fmtMs(report.avgLatencyMs));
    m_p50Label->setText(fmtMs(report.p50LatencyMs));
    m_p95Label->setText(fmtMs(report.p95LatencyMs));
    m_p99Label->setText(fmtMs(report.p99LatencyMs));
    m_successRateLabel->setText(fmtPct(report.successRequests, report.totalRequests));
    m_totalTimeLabel->setText(QString("%1 s").arg(report.totalTimeSec, 0, 'f', 2));
}

void StressPanel::onError(QString msg) {
    QMessageBox::warning(this, "压测错误", msg);
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}

void StressPanel::onExportCsv() {
    QString path = QFileDialog::getSaveFileName(
        this, "导出 CSV 报告",
        QString("stress_report_%1.csv").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    if (m_lastReport.exportCsv(path.toStdString())) {
        QMessageBox::information(this, "导出成功", QString("已保存到:\n%1").arg(path));
    } else {
        QMessageBox::critical(this, "导出失败", "无法写入文件,请检查路径权限");
    }
}
