#pragma once

#include <QObject>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <atomic>

#include "core/Message.h"

// 前向声明,避免头文件污染 httplib.h
namespace httplib {
class Client;
}

class SSEParser;  // 前向声明

/**
 * @brief LLM HTTP 客户端(基于 cpp-httplib)
 *
 * 支持同步非流式 chat() 和流式 chatStream()。
 * chat() 阻塞直到完整响应,chatStream() 通过 ContentReceiver 逐 token 回调。
 * 调用方负责放到工作线程,禁止在 UI 线程直接调用。
 */
class LLMClient : public QObject {
    Q_OBJECT

public:
    struct Config {
        std::string host = "127.0.0.1";
        int port = 8080;
        std::string model = "qwen2.5-7b";
        double temperature = 0.7;
        int max_tokens = 2048;
        int timeout_seconds = 300;
    };

    using SuccessCallback = std::function<void(const std::string& reply)>;
    using ErrorCallback   = std::function<void(const std::string& error)>;
    using TokenCallback   = std::function<void(const std::string& token)>;
    using FinishCallback  = std::function<void()>;

    explicit LLMClient(QObject* parent = nullptr);
    ~LLMClient() override;

    /// 更新配置(会重建底层 httplib::Client)
    void setConfig(const Config& cfg);

    /// 当前配置
    Config config() const { return m_cfg; }

    /**
     * @brief 同步非流式请求(阻塞直到收到完整响应)
     */
    void chat(const std::vector<Message>& history,
              SuccessCallback onSuccess,
              ErrorCallback onError);

    /**
     * @brief 流式请求(通过 ContentReceiver 逐 token 回调)
     * @param onToken 每收到一个增量 token 时调用
     * @param onFinish 流正常结束时调用(收到 [DONE])
     * @param onError 出错或被取消时调用
     *
     * 注意:回调运行在 httplib 的工作线程,不是 UI 线程。
     * UI 层应通过 Qt 信号槽把结果转回主线程。
     */
    void chatStream(const std::vector<Message>& history,
                    TokenCallback onToken,
                    FinishCallback onFinish,
                    ErrorCallback onError);

    /// 中断当前流式请求(ContentReceiver 返回 false 会断开连接)
    void cancel();

    /**
     * @brief 健康检查(尝试 GET /v1/models)
     */
    void healthCheck(SuccessCallback onSuccess, ErrorCallback onError);

signals:
    void requestStarted();
    void requestFinished();

    /// 流式场景:收到一个 token 时发出(参数已从 utf-8 转成 QString)
    void tokenReceived(QString token);

private:
    Config m_cfg;
    std::unique_ptr<httplib::Client> m_client;
    std::atomic<bool> m_cancelled{false};   ///< cancel() 标志

    void rebuildClient();
    std::string buildRequestBody(const std::vector<Message>& history, bool stream) const;
};
