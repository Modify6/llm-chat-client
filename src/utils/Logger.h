#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QMutex>

/**
 * @brief 统一日志模块:支持 INFO / WARN / ERROR / DEBUG,可输出到控制台 + 文件
 *
 * 用法:
 *   Logger::info("程序启动");
 *   Logger::warn("连接超时");
 *   Logger::error("HTTP 请求失败: " + err);
 *   Logger::debug("token count=42");
 *
 *   Logger::setLogFile("app.log");  // 可选:同时写文件
 *   Logger::setLevel(Logger::Level::INFO);  // 过滤级别
 */
class Logger : public QObject {
    Q_OBJECT

public:
    enum class Level { Debug, Info, Warn, Error };

    static Logger& instance();

    /// 设置日志文件路径(空字符串=不写文件)
    void setLogFile(const QString& path);

    /// 设置最低输出级别(Debug < Info < Warn < Error)
    void setLevel(Level level);

    /// 当前级别
    Level level() const { return m_level; }

    // 快捷静态方法
    static void debug(const QString& msg);
    static void info(const QString& msg);
    static void warn(const QString& msg);
    static void error(const QString& msg);

private:
    Logger();
    ~Logger();

    void log(Level level, const QString& msg);
    QString levelToString(Level level) const;
    QString timestamp() const;

    Level m_level = Level::Info;
    QFile m_file;
    QMutex m_mutex;  ///< 文件写入互斥
};
