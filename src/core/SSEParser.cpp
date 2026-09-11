#include "core/SSEParser.h"

#include <algorithm>

std::vector<std::string> SSEParser::feed(const std::string& chunk) {
    std::vector<std::string> events;

    m_buffer.append(chunk);

    // 先规范化换行:\r\n → \n,单独的 \r → \n
    // 这样后面所有逻辑只需处理 \n 一种换行
    std::string normalized;
    normalized.reserve(m_buffer.size());
    for (std::size_t i = 0; i < m_buffer.size(); ++i) {
        if (m_buffer[i] == '\r') {
            if (i + 1 < m_buffer.size() && m_buffer[i + 1] == '\n') {
                normalized += '\n';
                ++i;  // 跳过紧跟的 \n
            } else {
                normalized += '\n';  // 单独的 \r 也转成 \n
            }
        } else {
            normalized += m_buffer[i];
        }
    }
    m_buffer = std::move(normalized);

    // 循环切出完整事件(以 \n\n 为分隔符)
    while (true) {
        std::size_t pos = m_buffer.find("\n\n");
        if (pos == std::string::npos) {
            // 没有完整事件,等下次 feed
            break;
        }

        // 切出事件文本(不含结尾的 \n\n)
        std::string rawEvent = m_buffer.substr(0, pos);
        m_buffer.erase(0, pos + 2);

        // 把事件按换行切分,过滤空行和注释行,有效行拼回
        std::string processed;
        std::size_t start = 0;
        while (start < rawEvent.size()) {
            std::size_t end = rawEvent.find('\n', start);
            std::string line = (end == std::string::npos)
                ? rawEvent.substr(start)
                : rawEvent.substr(start, end - start);
            start = (end == std::string::npos) ? rawEvent.size() : end + 1;

            // 跳过空行和注释行(: 开头)
            if (line.empty()) continue;
            if (line[0] == ':') continue;

            if (!processed.empty()) processed += '\n';
            processed += line;
        }

        if (!processed.empty()) {
            events.push_back(processed);
        }
    }

    return events;
}

void SSEParser::reset() {
    m_buffer.clear();
}
