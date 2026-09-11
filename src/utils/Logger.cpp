#include "utils/Logger.h"

#include <QDateTime>
#include <QTextStream>
#include <QDebug>

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() = default;
Logger::~Logger() {
    if (m_file.isOpen()) m_file.close();
}

void Logger::setLogFile(const QString& path) {
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) m_file.close();
    if (!path.isEmpty()) {
        m_file.setFileName(path);
        if (!m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            qWarning() << "Logger: 无法打开日志文件" << path;
        }
    }
}

void Logger::setLevel(Level level) {
    m_level = level;
}

void Logger::log(Level level, const QString& msg) {
    if (level < m_level) return;

    QString line = QString("[%1] [%2] %3")
        .arg(timestamp(), levelToString(level), msg);

    // 控制台输出
    QTextStream out(stderr);
    out << line << Qt::endl;

    // 文件输出
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        QTextStream fs(&m_file);
        fs << line << Qt::endl;
    }
}

QString Logger::levelToString(Level level) const {
    switch (level) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
    }
    return "?????";
}

QString Logger::timestamp() const {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

// 静态快捷方法
void Logger::debug(const QString& msg) { instance().log(Level::Debug, msg); }
void Logger::info(const QString& msg)  { instance().log(Level::Info, msg); }
void Logger::warn(const QString& msg)  { instance().log(Level::Warn, msg); }
void Logger::error(const QString& msg) { instance().log(Level::Error, msg); }
