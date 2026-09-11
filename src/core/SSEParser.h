#pragma once

#include <string>
#include <vector>

/**
 * @brief Server-Sent Events (SSE) 流式解析器
 *
 * 用法:每次 HTTP chunk 到达时调用 feed(),
 * 当内部 buffer 凑够一个完整事件(以 \n\n 结尾)时,
 * 将其从 buffer 中切出并加入返回列表。
 *
 * 每个事件是一行原始 data: ... 文本(不含结尾 \n\n)。
 * 注释行(: 开头)和空行被忽略。
 */
class SSEParser {
public:
    SSEParser() = default;
    ~SSEParser() = default;

    /**
     * @brief 输入一段新收到的字节,返回所有已解析出的完整事件
     * @param chunk 任意长度的网络分片(可以是 0 字节)
     * @return 完整事件列表;单个事件是原始 data: ... 行,如 "data: {\"choices\":...}" 或 "data: [DONE]"
     */
    std::vector<std::string> feed(const std::string& chunk);

    /// 清空内部 buffer(切换到新请求时调用,防止旧数据污染)
    void reset();

private:
    std::string m_buffer;   ///< 未完成的事件缓冲
};
