#pragma once

#include <string>
#include <chrono>

/**
 * @brief 一条聊天消息的数据结构
 *
 * 纯 POD 风格,不持有任何外部资源。
 * 生产/消费端都按值传递或 const& 传递。
 */
struct Message {
    enum class Role {
        User,
        Assistant,
        System
    };

    Role role;
    std::string content;
    std::chrono::system_clock::time_point timestamp;

    /// 构造一条用户消息(自动填充当前时间戳)
    static Message user(const std::string& text);

    /// 构造一条助手回复(可带初始内容,流式场景先建空消息再追加)
    static Message assistant(const std::string& text = "");

    /// 构造一条系统提示消息
    static Message system(const std::string& text);
};
