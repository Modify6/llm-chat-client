# LLM Chat Client - 架构文档

## 1. 模块划分

```
┌──────────────────────────────────────────────────────────────┐
│                      UI 层 (Qt Widgets)                       │
│                                                              │
│  MainWindow ─┬─ ChatView ─ ChatBubble                        │
│              ├─ InputBox                                      │
│              ├─ SettingsDialog                                │
│              └─ StressPanel (QDockWidget)                     │
├──────────────────────────────────────────────────────────────┤
│                      核心层 (纯 C++)                           │
│                                                              │
│  LLMClient ─── cpp-httplib::Client                           │
│         ├── 非流式:Post → 完整 JSON → choices[0].message.content│
│         └── 流式:  Post → ContentReceiver → SSEParser → token  │
│                                                              │
│  SSEParser ─── buffer + \n\n 切分 → events                    │
│  Message ──── role / content / timestamp                     │
│  ChatSession ─ QVector<Message> + JSON save/load              │
├──────────────────────────────────────────────────────────────┤
│                      工具层                                    │
│                                                              │
│  ConfigManager ─ QSettings (单例)                             │
│  Logger ──────── 控制台 + 文件 (单例 + 互斥锁)                │
├──────────────────────────────────────────────────────────────┤
│                      压测层                                    │
│                                                              │
│  StressRunner ─── QtConcurrent::run + N 线程 worker           │
│  StressReport ─── compute(QPS/P50/P99) + exportCsv            │
└──────────────────────────────────────────────────────────────┘
```

## 2. 线程模型

```
┌──────────────┐   QueuedConnection    ┌──────────────────────┐
│   UI 主线程   │ ◄──────────────────  │  QtConcurrent 线程池   │
│              │                       │                      │
│ MainWindow   │  tokenReceived(QString)│ LLMClient::chatStream│
│ ChatView     │  requestFinished      │   ├─ httplib::Post    │
│ InputBox     │                       │   ├─ ContentReceiver  │
│ StressPanel  │                       │   └─ SSEParser::feed  │
│ StatusBar    │ ─── QtConcurrent::run ►│                      │
└──────────────┘                       │  chat() / healthCheck │
                                       │  StressRunner workers  │
                                       └──────────────────────┘
```

**关键规则**:
- UI 操作只在主线程
- HTTP 请求只在工作线程(通过 `QtConcurrent::run` 提交)
- 回调里只发 Qt 信号,不直接操作 UI
- 信号槽默认 `Qt::AutoConnection`,但跨线程时会自动变成 `QueuedConnection`

## 3. 数据流(流式对话)

```
用户输入 "你好"
    │
    ▼
InputBox::sendRequested(QString)
    │
    ▼
MainWindow::onSendRequested
    ├─ m_streaming = true
    ├─ InputBox::setStopMode(true)      ← 按钮变"停止"
    ├─ ChatView::addMessage(User, "你好")
    ├─ ChatView::appendToLastAssistant("")  ← 先建空 AI 气泡
    │
    └─ QtConcurrent::run {              ← 提交到线程池
         LLMClient::chatStream(history,
           onToken:  emit tokenReceived(QString),
           onFinish: emit tokenReceived(""),
           onError:  emit tokenReceived("[Error] ...")
         )
       }
          │
          ▼ (httplib ContentReceiver 回调)
          │   每收到一个 chunk:
          │   parser.feed(chunk) → events
          │   对每个 event:
          │     data: [DONE] → onFinish()
          │     data: {...}  → JSON → delta.content → onToken()
          │
          ▼ (QueuedConnection 信号)
MainWindow::onTokenReceived(QString)
    └─ ChatView::appendToLastAssistant(token)  ← 打字机逐字追加

点击"停止"按钮
    │
    ▼
LLMClient::cancel()
    └─ m_cancelled = true              ← atomic 标志
        └─ ContentReceiver 返回 false  ← httplib 断开连接
            └─ httplib 触发 onError    ← "请求已取消"
                └─ MainWindow 恢复 UI   ← 按钮回"发送"
```

## 4. SSE 解析流程

```
HTTP 流到达
    │
    ▼
SSEParser::feed(chunk)
    ├─ 追加到 m_buffer
    ├─ 规范化 \r\n → \n
    └─ 循环:
        ├─ 找 \n\n 分隔符
        ├─ 切出事件文本
        ├─ 过滤注释行(: 开头)和空行
        ├─ 有效行拼回
        └─ 加入返回列表

调用方收到 events:
    ├─ "data: [DONE]"  → 结束
    └─ "data: {...}"   → JSON 解析 → delta.content → onToken
```

**跨 chunk 边界处理**:
- feed("data: ") → buffer 里有残留,返回 []
- feed("hello\n") → buffer = "data: hello\n",还没凑 \n\n,返回 []
- feed("\n") → buffer 凑够 "data: hello\n\n",切出事件,返回 ["data: hello"]

## 5. 压测并发模型

```
StressRunner::start(params)
    ├─ m_stopped = false
    ├─ QtConcurrent::run {
    │   ├─ 创建独立 LLMClient(每个 worker 不共享)
    │   ├─ 启动 N 个 std::thread workers
    │   │   每个 worker:
    │   │     while (!m_stopped) {
    │   │       taskId = taskCounter.fetch_add(1)
    │   │       if (taskId >= totalRequests) break
    │   │       t0 = now()
    │   │       client.chat(history, ...)
    │   │       t1 = now()
    │   │       latencies.push(t1 - t0)
    │   │       定期 emit progress(done, total)
    │   │     }
    │   ├─ join 所有 worker
    │   └─ emit finished(StressReport)
    │       └─ report.compute() → QPS, P50, P99...
    │
    └─ StressPanel 接收信号 → 更新 UI
```

## 6. 取消机制

三层协作:

```
用户点"停止"
    │
    ▼
MainWindow::onStopRequested()
    └─ LLMClient::cancel()
        └─ m_cancelled.store(true)     ← 原子标志层
            │
            ▼
httplib ContentReceiver:
    if (m_cancelled) return false      ← httplib 断连层
        │
        ▼
httplib 内部:ContentReceiver 返回 false
    → 关闭 socket,中断连接
    → Post() 返回空 response
        │
        ▼
LLMClient::chatStream():
    检测到 httplib 空 response
    → 调用 onError("请求已取消")
        │
        ▼
MainWindow (QueuedConnection):
    → m_streaming = false
    → InputBox::setStopMode(false)
    → InputBox::setSendEnabled(true)
```

## 7. 配置持久化

```
ConfigManager (单例)
    ├─ load():
    │   QSettings("llm-chat-client", "llm-chat-client")
    │   → host / port / model / temperature / max_tokens / timeout
    │
    └─ save(cfg):
        写 QSettings → 自动保存到
        %APPDATA%/llm-chat-client/config.ini
```

## 8. 构建系统

```
CMakeLists.txt
    ├─ C++17, AUTOMOC/AUTORCC/AUTOUIC
    ├─ find_package(Qt6 REQUIRED Widgets Concurrent Test)
    ├─ 主程序:WIN32 子系统(无控制台)
    ├─ 测试:test_sse_parser + test_llm_chat
    ├─ Windows 链接:ws2_32 crypt32(httplib)
    └─ Qt 路径:CMAKE_PREFIX_PATH(可 -D 覆盖,可设环境变量)
```

## 9. 资源管理

| 资源 | 管理方式 |
|---|---|
| QWidget | Qt 父子对象机制 |
| httplib::Client | std::unique_ptr |
| LLMClient | std::unique_ptr + QObject 父子 |
| 网络连接 | httplib 自动管理 |
| 日志文件 | QFile + QMutex(单例) |
| QSettings | 值类型,栈上 |
| StressReport | 值类型,栈上/信号传递 |

**禁止裸 new/delete**(经验证:全项目 grep 只有 `new QWidget` 等 Qt 父子场景,符合规范)。
