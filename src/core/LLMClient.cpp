#include "core/LLMClient.h"

// httplib 单头文件,放在这里编译,避免每个 cpp 都重复
// 注意:不定义 CPPHTTPLIB_OPENSSL_SUPPORT,走纯 HTTP(llama-server 不需要 TLS)
#include "httplib.h"
#include "json.hpp"

#include "core/SSEParser.h"

#include <QMetaObject>
#include <QString>
#include <QByteArray>

using json = nlohmann::json;

LLMClient::LLMClient(QObject* parent)
    : QObject(parent) {
    rebuildClient();
}

LLMClient::~LLMClient() = default;

void LLMClient::setConfig(const Config& cfg) {
    m_cfg = cfg;
    rebuildClient();
}

void LLMClient::rebuildClient() {
    // httplib::Client 构造参数:host(不带 scheme) + port
    m_client = std::make_unique<httplib::Client>(m_cfg.host, m_cfg.port);
    m_client->set_read_timeout(m_cfg.timeout_seconds, 0);
    m_client->set_write_timeout(m_cfg.timeout_seconds, 0);
    m_client->set_connection_timeout(10, 0);
}

std::string LLMClient::buildRequestBody(const std::vector<Message>& history, bool stream) const {
    json messages = json::array();
    for (const auto& msg : history) {
        const char* roleStr = "user";
        switch (msg.role) {
            case Message::Role::User:      roleStr = "user";      break;
            case Message::Role::Assistant: roleStr = "assistant"; break;
            case Message::Role::System:    roleStr = "system";    break;
        }
        messages.push_back({
            {"role", roleStr},
            {"content", msg.content}
        });
    }

    json body = {
        {"model",       m_cfg.model},
        {"messages",    messages},
        {"temperature", m_cfg.temperature},
        {"max_tokens",  m_cfg.max_tokens},
        {"stream",      stream}
    };
    return body.dump();
}

// ============================================================
// 非流式 chat()
// ============================================================

void LLMClient::chat(const std::vector<Message>& history,
                     SuccessCallback onSuccess,
                     ErrorCallback onError) {
    if (!m_client) {
        if (onError) onError("LLMClient: 未初始化");
        return;
    }

    emit requestStarted();

    std::string body = buildRequestBody(history, /*stream=*/false);

    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };

    auto res = m_client->Post("/v1/chat/completions", headers, body, "application/json");

    if (!res) {
        std::string err = "HTTP 请求失败(网络错误)";
        if (onError) onError(err);
        emit requestFinished();
        return;
    }

    if (res->status != 200) {
        std::string err = "HTTP " + std::to_string(res->status) + ": " + res->body;
        if (onError) onError(err);
        emit requestFinished();
        return;
    }

    try {
        auto j = json::parse(res->body);
        std::string reply;
        if (j.contains("choices") && !j["choices"].empty()
            && j["choices"][0].contains("message")
            && j["choices"][0]["message"].contains("content")) {
            reply = j["choices"][0]["message"]["content"].get<std::string>();
        }
        if (onSuccess) onSuccess(reply);
    } catch (const std::exception& e) {
        if (onError) onError(std::string("JSON 解析失败: ") + e.what() + "\n原始响应: " + res->body);
    }

    emit requestFinished();
}

// ============================================================
// 流式 chatStream()
// ============================================================

void LLMClient::chatStream(const std::vector<Message>& history,
                           TokenCallback onToken,
                           FinishCallback onFinish,
                           ErrorCallback onError) {
    if (!m_client) {
        if (onError) onError("LLMClient: 未初始化");
        return;
    }

    m_cancelled.store(false);
    emit requestStarted();

    std::string body = buildRequestBody(history, /*stream=*/true);

    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };

    // 每个请求独立的 SSEParser 实例,用 shared_ptr 让 ContentReceiver lambda 安全捕获
    auto parser = std::make_shared<SSEParser>();
    bool finished = false;  // 防止重复触发 onFinish

    // ContentReceiver:每收到一个 chunk 调用一次
    auto contentReceiver =
        [this, parser, &onToken, &onFinish, &onError, &finished](const char* data, size_t len) -> bool {
        // 检查取消标志
        if (m_cancelled.load()) {
            if (!finished && onError) onError("请求已取消");
            finished = true;
            return false;  // 返回 false 会中断连接
        }

        std::string chunk(data, len);
        auto events = parser->feed(chunk);

        for (const auto& ev : events) {
            // DONE marker:流结束
            if (ev.find("data: [DONE]") != std::string::npos) {
                if (!finished) {
                    finished = true;
                    if (onFinish) onFinish();
                }
                return true;  // 返回 true 让 httplib 继续读,等服务端关连接
            }

            // 正常 SSE 事件:data: {...}
            if (ev.rfind("data: ", 0) == 0) {
                std::string jsonStr = ev.substr(6);  // 跳过 "data: "
                try {
                    auto j = json::parse(jsonStr);
                    // OpenAI 风格: choices[0].delta.content
                    if (j.contains("choices") && !j["choices"].empty()) {
                        const auto& delta = j["choices"][0]["delta"];
                        if (delta.contains("content")) {
                            std::string token = delta["content"].get<std::string>();
                            if (!token.empty()) {
                                if (onToken) onToken(token);

                                // 同时发 Qt 信号(utf-8 → QString),给 UI 层跨线程用
                                // 注意:emit 运行在 httplib 工作线程,UI 层需用 QueuedConnection 接收
                                auto qtoken = QString::fromUtf8(token.c_str(),
                                                                 static_cast<int>(token.size()));
                                emit tokenReceived(qtoken);
                            }
                        }
                    }
                } catch (const std::exception& /*e*/) {
                    // JSON 解析失败:忽略,继续处理后续事件(不中断流)
                }
            }
            // 非 data: 行(如 event:ping)忽略
        }

        return true;  // 继续接收
    };

    auto res = m_client->Post("/v1/chat/completions",
                              headers, body, "application/json",
                              contentReceiver);

    // 请求结束后:如果既没 DONE 也没取消,且有错误,触发 onError
    if (!res) {
        if (!finished && onError) onError("HTTP 流式请求失败(网络错误)");
    } else if (res->status != 200) {
        if (!finished && onError) {
            std::string err = "HTTP " + std::to_string(res->status) + ": " + res->body;
            onError(err);
        }
    } else {
        // 正常结束但没收到 [DONE](服务端可能直接关连接),补发 onFinish
        if (!finished) {
            finished = true;
            if (onFinish) onFinish();
        }
    }

    emit requestFinished();
}

void LLMClient::cancel() {
    m_cancelled.store(true);
}

// ============================================================
// 健康检查
// ============================================================

void LLMClient::healthCheck(SuccessCallback onSuccess, ErrorCallback onError) {
    if (!m_client) {
        if (onError) onError("LLMClient: 未初始化");
        return;
    }

    auto res = m_client->Get("/v1/models");
    if (!res) {
        if (onError) onError("健康检查失败: 无法连接到 " + m_cfg.host + ":" + std::to_string(m_cfg.port));
        return;
    }
    if (res->status != 200) {
        if (onError) onError("健康检查失败: HTTP " + std::to_string(res->status));
        return;
    }
    if (onSuccess) onSuccess(res->body);
}
