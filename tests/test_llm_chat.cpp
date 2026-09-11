/**
 * @brief LLMClient 控制台测试(非流式 + 流式 + 取消)
 *
 * 用法:
 *   test_llm_chat.exe [--stream] [--cancel-after N] [prompt]
 *
 * 示例:
 *   test_llm_chat.exe "你好"              # 非流式
 *   test_llm_chat.exe --stream "你好"     # 流式,每 token 打印
 *   test_llm_chat.exe --stream --cancel-after 5 "写一首长诗"  # 流式,5 个 token 后取消
 *
 * 依赖: llama-server 已在 http://127.0.0.1:8080 运行
 */
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>

#include <QCoreApplication>

#include "core/LLMClient.h"
#include "core/Message.h"

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    // 解析命令行参数
    bool streamMode = false;
    int cancelAfterTokens = -1;   // -1 表示不取消
    std::string prompt = "Hello, please introduce yourself briefly.";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--stream") {
            streamMode = true;
        } else if (arg == "--cancel-after" && i + 1 < argc) {
            cancelAfterTokens = std::stoi(argv[++i]);
            streamMode = true;  // 取消只在流式下有意义
        } else {
            prompt = arg;
        }
    }

    std::cout << "=== LLMClient Test ===" << std::endl;
    std::cout << "Mode:   " << (streamMode ? "STREAMING" : "NON-STREAMING") << std::endl;
    std::cout << "Prompt: " << prompt << std::endl;
    if (cancelAfterTokens > 0) {
        std::cout << "Cancel after " << cancelAfterTokens << " tokens" << std::endl;
    }

    LLMClient client;

    // 健康检查
    bool healthOk = false;
    client.healthCheck(
        [&](const std::string&) { healthOk = true; },
        [&](const std::string& err) { std::cerr << "Health FAIL: " << err << std::endl; }
    );
    if (!healthOk) { std::cerr << "llama-server not reachable, exit." << std::endl; return 1; }
    std::cout << "Health OK" << std::endl;

    std::vector<Message> history;
    history.push_back(Message::system("You are a helpful assistant."));
    history.push_back(Message::user(prompt));

    if (!streamMode) {
        // ====== 非流式测试 ======
        std::cout << "\n--- Chat (non-streaming) ---" << std::endl;
        bool got = false;
        client.chat(history,
            [&](const std::string& reply) { got = true; std::cout << reply << std::endl; },
            [&](const std::string& err) { std::cerr << "[ERROR] " << err << std::endl; }
        );
        std::cout << "\n=== Test " << (got ? "PASSED" : "FAILED") << " ===" << std::endl;
        return got ? 0 : 1;
    }

    // ====== 流式测试 ======
    std::cout << "\n--- Chat (streaming) ---" << std::endl;
    std::cout << "[Assistant] " << std::flush;

    int tokenCount = 0;
    bool done = false;
    bool error = false;

    // 在另一个线程里触发取消(可选)
    std::thread cancelThread;
    if (cancelAfterTokens > 0) {
        cancelThread = std::thread([&]() {
            // 等足够多 token 到达后取消
            while (tokenCount < cancelAfterTokens) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            std::cout << "\n[Cancel triggered after " << cancelAfterTokens << " tokens]" << std::endl;
            client.cancel();
        });
    }

    client.chatStream(history,
        // onToken:逐 token 打印
        [&](const std::string& token) {
            tokenCount++;
            std::cout << token << std::flush;
            if (cancelAfterTokens > 0 && tokenCount >= cancelAfterTokens && cancelThread.joinable()) {
                // 取消由 cancelThread 触发,这里不做
            }
        },
        // onFinish:流正常结束
        [&]() {
            done = true;
            std::cout << "\n[Stream finished, " << tokenCount << " tokens]" << std::endl;
        },
        // onError:出错或被取消
        [&](const std::string& err) {
            error = true;
            std::cout << "\n[ERROR/CANCEL] " << err << std::endl;
        }
    );

    if (cancelThread.joinable()) cancelThread.join();

    std::cout << "\n=== Stream Test: "
              << (error ? (cancelAfterTokens > 0 && tokenCount > 0 ? "CANCELLED AS EXPECTED" : "ERROR")
                        : "COMPLETED OK")
              << " (" << tokenCount << " tokens) ===" << std::endl;

    if (cancelAfterTokens > 0 && error && tokenCount > 0) return 0;  // 取消成功
    if (!error && done) return 0;  // 正常完成
    return 1;
}
