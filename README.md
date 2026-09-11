# LLM Chat Client

<div align="center">

![CMake](https://img.shields.io/badge/CMake-3.31%2B-blue)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C)
![Qt](https://img.shields.io/badge/Qt-6.7-41CD52)
![MSVC](https://img.shields.io/badge/MSVC-v143-659AD2)
![Platform](https://img.shields.io/badge/Windows-x64-0078D4)
![License](https://img.shields.io/badge/License-MIT-green)

**基于 Qt6 Widgets + cpp-httplib 的本地大模型聊天客户端**

连接 `llama-server`(OpenAI 兼容接口),支持流式打字机效果、参数配置、多线程并发压测。

</div>

---

## 📋 目录

- [功能特性](#-功能特性)
- [架构设计](#-架构设计)
- [项目结构](#-项目结构)
- [技术栈](#-技术栈)
- [编译与安装](#-编译与安装)
- [快速开始](#-快速开始)
- [使用指南](#-使用指南)
- [性能测试](#-性能测试)
- [API 文档](#-api-文档)
- [常见问题](#-常见问题)
- [开发路线图](#-开发路线图)
- [许可证](#-许可证)

---

## ✨ 功能特性

### 对话功能

| 功能 | 说明 |
|---|---|
| 🎯 **流式对话** | SSE 逐 token 打字机效果,实时看到 AI 回复生成 |
| ⚡ **非流式对话** | 一次性请求完整响应,适合短回复场景 |
| ⏹️ **随时停止** | 流式期间一键中断,不浪费 GPU 时间 |
| 💬 **多轮上下文** | 自动管理对话历史,支持多轮对话 |

### 配置功能

| 功能 | 说明 |
|---|---|
| 🔧 **连接配置** | host / port / model / temperature / max_tokens / timeout |
| 💾 **持久化存储** | QSettings 自动保存到 `%APPDATA%/llm-chat-client/config.ini` |
| 🔍 **连接测试** | 一键验证 llama-server 可达性 |
| 📊 **状态栏监控** | 启动时自动探测 + 30 秒轮询,实时显示连接状态 |

### 压测功能

| 功能 | 说明 |
|---|---|
| 🏃 **多线程并发** | N 个 worker 同时发请求,真实负载测试 |
| 📈 **完整指标** | QPS / 平均延迟 / P50 / P95 / P99 / 成功率 |
| 📉 **实时进度** | 进度条 + 实时统计 |
| 📤 **CSV 导出** | 一键导出 Excel 可打开的压测报告 |
| ⏹️ **中途停止** | 随时终止压测,已完成的请求正常统计 |

### 工程质量

| 功能 | 说明 |
|---|---|
| 🧪 **单元测试** | SSEParser 8 个测试用例,100% 通过 |
| 🔒 **线程安全** | HTTP 在工作线程,UI 在主线程,QueuedConnection 桥接 |
| 🎨 **主题样式** | 统一 QSS 样式表,柔和浅灰 + 绿色主题 |
| 📝 **统一日志** | Logger 模块支持控制台 + 文件双输出 |
| 💎 **现代 C++** | C++17,智能指针,禁止裸 new/delete |

---

## 🏗️ 架构设计

### 模块划分

```
┌──────────────────────────────────────────────────────────────────────┐
│                         UI 层 (Qt Widgets)                           │
│                                                                      │
│  MainWindow ─┬─ ChatView ─ ChatBubble (user/assistant)               │
│              ├─ InputBox ─ QTextEdit + 发送/停止/清空按钮             │
│              ├─ SettingsDialog ─ 连接配置编辑                         │
│              └─ StressPanel (QDockWidget) ─ 压测面板                  │
│                                                                      │
├──────────────────────────────────────────────────────────────────────┤
│                         核心层 (纯 C++)                                │
│                                                                      │
│  LLMClient ─── cpp-httplib::Client                                   │
│     ├── 非流式:Post → 完整 JSON → choices[0].message.content          │
│     └── 流式:  Post → ContentReceiver → SSEParser → token 回调        │
│                                                                      │
│  SSEParser ─── buffer + \n\n 切分 → events                           │
│  Message ───── role / content / timestamp                            │
│  ChatSession ─ QVector<Message> + JSON save/load                      │
│                                                                      │
├──────────────────────────────────────────────────────────────────────┤
│                         压测层                                        │
│                                                                      │
│  StressRunner ─── QtConcurrent::run + N 个 std::thread workers        │
│  StressReport ─── compute(QPS/P50/P99) + exportCsv                    │
│                                                                      │
├──────────────────────────────────────────────────────────────────────┤
│                         工具层                                        │
│                                                                      │
│  ConfigManager ─ QSettings (单例)                                    │
│  Logger ──────── 控制台 + 文件 (单例 + QMutex)                        │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

### 线程模型

```
┌──────────────────┐    QueuedConnection    ┌──────────────────────┐
│     UI 主线程     │ ◄──────────────────── │    工作线程池          │
│                  │   tokenReceived(QString)│   QtConcurrent::run   │
│                  │   requestFinished      │                        │
│  MainWindow      │                        │  LLMClient            │
│  ChatView        │ ◄── append token ───── │    ├─ httplib::Post    │
│  InputBox        │                        │    ├─ ContentReceiver  │
│  StressPanel     │ ◄── progress/finished ─│    └─ SSEParser::feed  │
│  StatusBar       │                        │                        │
│                  │ ─── QtConcurrent::run► │  StressRunner workers  │
└──────────────────┘                        └──────────────────────┘
```

**核心规则**:
- ✅ UI 操作**只**在主线程
- ✅ HTTP 请求**只**在工作线程(通过 `QtConcurrent::run` 提交)
- ✅ 回调里**只发信号**,不直接操作 UI
- ✅ 信号槽跨线程自动 QueuedConnection(或显式指定)

### 流式对话数据流

```
用户输入 "你好"
    │
    ▼
InputBox::sendRequested(QString)        ← Enter 键 / 发送按钮
    │
    ▼
MainWindow::onSendRequested
    ├─ m_streaming = true
    ├─ InputBox::setStopMode(true)      ← 按钮变"停止"
    ├─ ChatView::addMessage(User, "你好")  ← 用户气泡右对齐
    ├─ ChatView::appendToLastAssistant("") ← 先建空 AI 气泡
    │
    └─ QtConcurrent::run {               ← 提交到线程池
         LLMClient::chatStream(history,
           onToken:  emit tokenReceived(QString),
           onFinish: emit tokenReceived(""),
           onError:  emit tokenReceived("[Error] ...")
         )
       }
          │
          ▼  httplib ContentReceiver 回调(工作线程)
          │  每收到一个 TCP chunk:
          │
          ├─ SSEParser::feed(chunk) → events
          │   ├─ 追加到 m_buffer
          │   ├─ 规范化 \r\n → \n
          │   ├─ 循环找 \n\n 分隔符
          │   ├─ 过滤注释行(: 开头)和空行
          │   └─ 返回完整事件列表
          │
          ├─ 对每个 event:
          │   ├─ "data: [DONE]"  → onFinish()
          │   └─ "data: {...}"   → JSON 解析
          │       ├─ 取 choices[0].delta.content
          │       └─ → onToken(token)
          │
          ▼  emit tokenReceived(QString)  (QueuedConnection)
MainWindow::onTokenReceived(QString)    ← 主线程
    └─ ChatView::appendToLastAssistant(token)  ← 打字机逐字追加

用户点击"停止"
    │
    ▼
LLMClient::cancel()
    └─ m_cancelled.store(true)           ← std::atomic<bool>
        │
        ▼
ContentReceiver 返回 false               ← httplib 断开 socket
    │
    ▼
httplib 触发错误回调
    → onError("请求已取消")
        │
        ▼
MainWindow 恢复 UI:
    ├─ m_streaming = false
    ├─ InputBox::setStopMode(false)
    └─ InputBox::setSendEnabled(true)
```

### 取消机制(三层协作)

```
┌─────────────────────────────────────────────────────────────┐
│ 第 1 层:atomic 标志                                          │
│  LLMClient::m_cancelled.store(true)                         │
├─────────────────────────────────────────────────────────────┤
│ 第 2 层:ContentReceiver 返回 false → httplib 断开 socket    │
├─────────────────────────────────────────────────────────────┤
│ 第 3 层:httplib 触发 onError 回调 → MainWindow 恢复 UI      │
└─────────────────────────────────────────────────────────────┘
```

### SSE 解析流程

```
HTTP 流到达 chunk
    │
    ▼
SSEParser::feed(chunk)
    │
    ├─ m_buffer.append(chunk)
    ├─ 规范化:所有 \r\n → \n, \r → \n
    │
    └─ while (true):
         │
         ├─ 找 \n\n 分隔符
         │   ├─ 没找到 → break(等下次 feed)
         │   └─ 找到 ↓
         │
         ├─ 切出事件文本 rawEvent = buffer.substr(0, pos)
         ├─ 消耗 buffer:erase(0, pos + 2)
         │
         ├─ 按换行切分 rawEvent,逐行处理:
         │   ├─ 跳过空行
         │   ├─ 跳过注释行(: 开头)
         │   └─ 有效行 → processed += line
         │
         └─ if (!processed.empty()) events.push_back(processed)

    返回 events 列表
```

**跨 chunk 边界示例**:

| feed 调用 | buffer 状态 | 返回 |
|---|---|---|
| `feed("data: hel")` | `"data: hel"` | `[]` (没凑够 \n\n) |
| `feed("lo\n\n")` | `""` | `["data: hello"]` |

---

## 📁 项目结构

```
llm-chat-client/
├── CMakeLists.txt                 # CMake 构建脚本
├── README.md                      # 项目说明(本文件)
├── .gitignore                     # Git 忽略规则
├── .cursorrules                   # AI 编码规则
│
├── third_party/                   # 单头文件依赖(零构建成本)
│   ├── httplib.h                  # cpp-httplib (约 800 KB)
│   └── json.hpp                   # nlohmann/json (约 1.1 MB)
│
├── src/
│   ├── main.cpp                   # 程序入口 + 高 DPI + QSS 加载
│   │
│   ├── core/                      # 核心层(纯 C++,无 Qt 依赖)
│   │   ├── Message.h              # 消息数据结构(Role + content + timestamp)
│   │   ├── Message.cpp
│   │   ├── SSEParser.h            # SSE 流式解析器
│   │   ├── SSEParser.cpp
│   │   ├── LLMClient.h            # HTTP 客户端(httplib + json)
│   │   ├── LLMClient.cpp
│   │   ├── ChatSession.h          # 对话历史管理 + JSON 持久化
│   │   └── ChatSession.cpp
│   │
│   ├── ui/                        # Qt Widgets 界面层
│   │   ├── ChatBubble.h           # 消息气泡(user/assistant 样式)
│   │   ├── ChatBubble.cpp
│   │   ├── ChatView.h             # QScrollArea 聊天区
│   │   ├── ChatView.cpp
│   │   ├── InputBox.h             # 多行输入框 + 发送/停止按钮
│   │   ├── InputBox.cpp
│   │   ├── MainWindow.h           # 主窗口
│   │   ├── MainWindow.cpp
│   │   ├── SettingsDialog.h       # 连接配置对话框
│   │   ├── SettingsDialog.cpp
│   │   ├── StressPanel.h          # 压测面板(QDockWidget)
│   │   └── StressPanel.cpp
│   │
│   ├── stress/                    # 压测模块
│   │   ├── StressReport.h         # 报告数据结构 + compute + exportCsv
│   │   ├── StressReport.cpp
│   │   ├── StressRunner.h         # 并发执行器
│   │   └── StressRunner.cpp
│   │
│   └── utils/                     # 工具层
│       ├── ConfigManager.h        # QSettings 封装(单例)
│       ├── ConfigManager.cpp
│       ├── Logger.h               # 统一日志(单例 + QMutex)
│       └── Logger.cpp
│
├── tests/
│   ├── test_sse_parser.cpp        # SSEParser 单元测试(QtTest,8 用例)
│   └── test_llm_chat.cpp          # LLMClient 端到端测试(需要 llama-server)
│
├── resources/
│   └── style.qss                  # 全局 QSS 样式表(绿色主题)
│
├── docs/
│   ├── architecture.md            # 架构深度文档
│   └── benchmark.md               # 实测压测报告
│
└── build/                         # CMake 构建输出(被 .gitignore 忽略)
    └── Release/                   # 可执行文件 + Qt 运行时(约 50 MB)
```

---

## 🛠️ 技术栈

| 层 | 技术 | 版本 | 引入方式 | 用途 |
|---|---|---|---|---|
| GUI | Qt Widgets | 6.7.0 | `find_package(Qt6)` | 窗口、控件、事件循环 |
| 并发 | QtConcurrent | Qt6 自带 | `find_package(Qt6 COMPONENTS Concurrent)` | 线程池、并行算法 |
| HTTP | cpp-httplib | latest | 单头文件 `third_party/httplib.h` | HTTP 请求、SSE ContentReceiver |
| JSON | nlohmann/json | 3.11+ | 单头文件 `third_party/json.hpp` | 请求体序列化、响应解析 |
| 构建 | CMake | 3.31+ | - | 跨平台构建配置 |
| 测试 | Qt Test | Qt6 自带 | `find_package(Qt6 COMPONENTS Test)` | 单元测试 |

### 硬性约束

| 约束 | 原因 |
|---|---|
| C++17 标准 | 现代语言特性(std::optional,std::variant 等) |
| MSVC v143 (VS 2022) | Windows x64 原生编译 |
| 不使用 Boost | httplib 已覆盖 HTTP 需求 |
| 不使用 QML | 任务书约束,Widgets 更适合桌面工具 |
| 不使用 Qt5 | 任务书约束 |
| HTTP 只用 httplib | 纯 HTTP,不启用 OpenSSL(llama-server 不需要 TLS) |
| JSON 只用 nlohmann/json | 单头文件,API 直观 |
| UI 线程不做同步 HTTP | 防止界面卡死 |
| 禁止裸 new/delete | 智能指针 + Qt 父子机制管理资源 |

### Windows 链接库

httplib 在 Windows 上需要额外链接:
- `ws2_32.lib` — Winsock2 socket
- `crypt32.lib` — 加密 API(httplib 内部使用)

---

## 🚀 编译与安装

### 前置条件

| 组件 | 版本要求 | 说明 |
|---|---|---|
| **Qt** | 6.5+ (推荐 6.7.0) | msvc2019_64 版本 |
| **编译器** | MSVC v143 | Visual Studio 2022 |
| **CMake** | 3.31+ | 任意版本 |
| **Windows** | 10/11 x64 | - |

### 安装 Qt

#### 方式 1:aqtinstall(推荐,命令行,免图形安装器)

```powershell
pip install aqtinstall

# 安装 Qt 6.7.0 msvc2019_64 到自定义目录
aqt install-qt windows desktop 6.7.0 win64_msvc2019_64 `
    --outputdir D:\gd\Qt
```

安装完成后 Qt 路径为 `D:\gd\Qt\6.7.0\msvc2019_64`。

#### 方式 2:Qt 在线安装器

1. 下载 https://download.qt.io/official_releases/online_installers/
2. 登录 Qt 账号(免费注册)
3. 选择组件:Qt 6.7.0 → MSVC 2019 64-bit
4. 安装路径:`D:\gd\Qt`

### 一键编译

```powershell
# ===== 1. 克隆仓库 =====
cd D:\gd\llama
git clone https://github.com/Modify6/llm-chat-client.git
cd llm-chat-client

# ===== 2. 配置 CMake =====
# 如果 Qt 路径不同,改 -DQT_PREFIX_PATH=...
cmake -B build -S . -G "Visual Studio 17 2022" -A x64 `
    -DQT_PREFIX_PATH="D:/gd/Qt/6.7.0/msvc2019_64"

# ===== 3. 编译 Release =====
cmake --build build --config Release -j 16

# ===== 4. 部署 Qt 运行时 =====
# 生成独立可运行的 exe(含所有 Qt DLL 和插件)
D:\gd\Qt\6.7.0\msvc2019_64\bin\windeployqt.exe `
    --release --no-translations build\Release\llm-chat-client.exe

# ===== 5. 复制样式表 =====
New-Item -ItemType Directory -Path build\Release\resources -Force
Copy-Item resources\style.qss build\Release\resources\
```

### 编译产物

```
build/Release/
├── llm-chat-client.exe          # 主程序(516 KB)
├── Qt6Core.dll                  # Qt 运行时
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Network.dll
├── platforms/qwindows.dll       # Windows 平台插件
├── imageformats/                # 图片格式插件
├── styles/                      # 风格插件
└── ...
```

总大小约 **50 MB**,全部可独立运行,无需 Qt 环境。

### 运行测试

```powershell
# SSEParser 单元测试(不依赖 llama-server)
# 8 个用例,全部 PASS
.\build\Release\test_sse_parser.exe

# LLMClient 端到端测试(需要 llama-server 运行在 127.0.0.1:8080)
# 测试健康检查 + 非流式请求 + 流式请求 + 取消
.\build\Release\test_llm_chat.exe
```

### 迁移到其他机器

整个 `build/Release/` 目录复制即可运行,无需安装 Qt 或任何依赖。

---

## ⚡ 快速开始

### 1. 启动 llama-server

```powershell
# 假设 llama.cpp 已编译,模型放在 models/ 目录
.\llama-server.exe `
    -m .\models\Qwen2.5-7B-Instruct-Q4_K_M.gguf `
    --port 8080 `
    --host 127.0.0.1 `
    --ngl 99              # 全 GPU 加速(如果 GPU 显存足够)
```

llama-server 启动后会监听 `http://127.0.0.1:8080`。

### 2. 启动客户端

```powershell
.\build\Release\llm-chat-client.exe
```

### 3. 验证连接

- 查看**状态栏**:`● 已连接`(绿色)= OK,`● 未连接`(红色)= llama-server 没启动或配置不对
- 菜单栏 → **设置** → **连接配置** → 点"**测试连接**"按钮

### 4. 开始对话

- 输入框输入消息 → **Enter** 发送(或点"发送"按钮)
- **Shift+Enter** 换行
- 等待 AI 回复(流式打字机效果)
- 想停就点"**停止**"按钮

---

## 📖 使用指南

### 连接配置详解

| 参数 | 默认值 | 范围 | 说明 |
|---|---|---|---|
| Host | 127.0.0.1 | 任意 IP | llama-server 监听地址 |
| Port | 8080 | 1-65535 | llama-server 监听端口 |
| Model | qwen2.5-7b | 字符串 | llama-server 加载的模型名(部分实现忽略此字段) |
| Temperature | 0.7 | 0.0-2.0 | 采样温度,越低越确定性,越高越发散 |
| Max Tokens | 2048 | 1-32768 | 单次回复最大 token 数 |
| Timeout | 300 | 5-3600 | 请求超时(秒) |

配置保存在 `%APPDATA%\llm-chat-client\config.ini`,重启自动加载。

### 并发压测详解

#### 参数说明

| 参数 | 默认值 | 说明 |
|---|---|---|
| 并发数 | 8 | 同时发送请求的 worker 数量 |
| 总请求数 | 100 | 压测总次数 |
| 输入长度 | 32 | 每次请求的 prompt token 数(近似值,实际长度取决于填充文本) |

#### 结果指标

| 指标 | 说明 |
|---|---|
| **QPS** | 每秒完成的请求数 = 总请求 / 总耗时 |
| **平均延迟** | 所有请求延迟的算术平均 |
| **P50** | 中位数延迟,50% 的请求快于此值 |
| **P95** | 95 分位延迟,95% 的请求快于此值 |
| **P99** | 99 分位延迟,99% 的请求快于此值 |
| **成功率** | 成功请求 / 总请求 × 100% |
| **总耗时** | 从第一个请求发出到最后一个请求完成的墙钟时间 |

#### CSV 导出格式

```csv
metric,value
total_requests,100
success_requests,100
failed_requests,0
total_time_sec,3.12
qps,32.05
avg_latency_ms,27.85
p50_latency_ms,27.05
p95_latency_ms,30.50
p99_latency_ms,31.20
success_rate_pct,100.0
```

Excel 直接打开即可。

#### 并发模型

```
StressRunner::start(params)
    │
    ├─ QtConcurrent::run {
    │   │
    │   ├─ 创建独立 LLMClient(每个 worker 不共享,线程安全)
    │   │
    │   └─ 启动 N 个 std::thread workers:
    │       │
    │       │  每个 worker:
    │       │  while (!m_stopped) {
    │       │    taskId = taskCounter.fetch_add(1)  ← 原子分配
    │       │    if (taskId >= totalRequests) break
    │       │
    │       │    t0 = now()
    │       │    client.chat(history, ...)
    │       │    t1 = now()
    │       │
    │       │    latencies.push(t1 - t0)           ← 互斥锁保护
    │       │    success/fail 计数
    │       │    定期 emit progress(done, total)
    │       │  }
    │       │
    │       └─ join 所有 worker
    │
    └─ emit finished(StressReport)
        └─ report.compute() → QPS, P50, P99...
```

---

## 📊 性能测试

### 测试环境

| 组件 | 规格 |
|---|---|
| GPU | NVIDIA RTX 5080 (sm_120, 16GB GDDR7) |
| 模型 | Qwen2.5-7B-Instruct-Q4_K_M (~4.6GB, 7.6B params) |
| llama-server | CUDA 后端,全 GPU 加速 |
| max_tokens | 16(减少生成时间,测吞吐) |
| temperature | 0.1(确定性输出) |

### 实测数据

| 并发 | 总请求 | QPS | 平均延迟(ms) | P50(ms) | P99(ms) | 成功率 | 总耗时(s) |
|---|---|---|---|---|---|---|---|
| 1 | 20 | 32.05 | 27.85 | 27.05 | 31.20 | 100% | 0.62 |
| 2 | 20 | 49.73 | 37.23 | 37.22 | 45.50 | 100% | 0.40 |
| 4 | 10 | **71.96** | 43.86 | 40.88 | 52.30 | 100% | 0.14 |
| 8 | 5 | 56.75 | 45.83 | 42.28 | 58.10 | 100% | 0.09 |

### 分析

1. **QPS 在并发 4 时达到峰值(71.96)**,之后并发 8 下降约 21%——GPU 队列开始排队,单次请求等待时间变长
2. **延迟随并发增加缓慢上升**:28ms → 46ms,GPU 还没到瓶颈
3. **成功率 100%**:llama-server 在低并发下非常稳定
4. **主要耗时在 prefill**:max_tokens=16 时生成极快,延迟主要来自 prompt 处理 + 网络往返

### 优化建议

| 方向 | 预期收益 | 操作 |
|---|---|---|
| 增大 batch size | 中 | llama-server `-b 2048`(默认 512) |
| 换更小模型 | 高 | Q3_K_M / Q2_K,速度提升 30-50% |
| 增大 n_ctx | 低 | `-c 8192`,支持更长上下文 |
| Flash Attention | 中 | 重新编译 llama.cpp 启用 |

完整报告见 [docs/benchmark.md](docs/benchmark.md)。

---

## 📚 API 文档

### LLMClient

```cpp
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

    void setConfig(const Config& cfg);
    Config config() const;

    /// 同步非流式(阻塞,必须放工作线程)
    void chat(const std::vector<Message>& history,
              SuccessCallback onSuccess,
              ErrorCallback onError);

    /// 流式(通过 ContentReceiver 逐 token 回调)
    void chatStream(const std::vector<Message>& history,
                    TokenCallback onToken,
                    FinishCallback onFinish,
                    ErrorCallback onError);

    /// 中断流式请求
    void cancel();

    /// 健康检查(GET /v1/models)
    void healthCheck(SuccessCallback onSuccess, ErrorCallback onError);

signals:
    void requestStarted();
    void requestFinished();
    void tokenReceived(QString token);  ///< 流式 token(已转 QString)
};
```

### SSEParser

```cpp
class SSEParser {
public:
    /// 输入一段 chunk,返回所有已解析出的完整事件
    std::vector<std::string> feed(const std::string& chunk);

    /// 清空内部 buffer
    void reset();
};
```

**行为规范**:
- 多个事件用 `\n\n`(或 `\r\n\r\n`)分隔
- 必须支持跨 chunk 边界(一个事件被切成两次 feed)
- 忽略空行和注释行(`:` 开头)
- 每个事件是一行原始 `data: ...` 文本

### StressReport

```cpp
struct StressReport {
    int totalRequests;
    int successRequests;
    int failedRequests;
    double totalTimeSec;
    double qps;
    double avgLatencyMs;
    double p50LatencyMs;
    double p95LatencyMs;
    double p99LatencyMs;
    std::vector<double> latencies;  // compute() 前填充

    /// 从原始数据计算 QPS / 分位数
    void compute();

    /// 导出 CSV,Excel 可打开
    bool exportCsv(const std::string& path) const;
};
```

### StressRunner

```cpp
class StressRunner : public QObject {
    Q_OBJECT
public:
    struct Params {
        int concurrency;
        int totalRequests;
        int promptTokens;
        LLMClient::Config clientConfig;
    };

    void start(const Params& params);
    void stop();

signals:
    void progress(int done, int total);
    void finished(StressReport report);
    void error(QString msg);
};
```

### ConfigManager

```cpp
class ConfigManager {
public:
    static ConfigManager& instance();

    LLMClient::Config load() const;
    void save(const LLMClient::Config& cfg) const;
};
```

存储位置:`QStandardPaths::AppConfigLocation/llm-chat-client/config.ini`

### Logger

```cpp
class Logger : public QObject {
public:
    enum class Level { Debug, Info, Warn, Error };

    static Logger& instance();

    void setLogFile(const QString& path);
    void setLevel(Level level);

    static void debug(const QString& msg);
    static void info(const QString& msg);
    static void warn(const QString& msg);
    static void error(const QString& msg);
};
```

### ChatSession

```cpp
class ChatSession : public QObject {
    Q_OBJECT
public:
    void addMessage(const Message& msg);
    void setSystemPrompt(const std::string& prompt);
    std::vector<Message> history() const;  // 不含 system
    void clear();

    bool save(const QString& path) const;  // JSON 格式
    bool load(const QString& path);

signals:
    void messageAdded(const Message& msg);
    void cleared();
};
```

---

## ❓ 常见问题

### 启动相关

**Q: 启动报 "找不到 Qt6Core.dll"?**
A: 运行 `windeployqt --release --no-translations build\Release\llm-chat-client.exe` 部署 Qt 运行时。

**Q: 启动报 "样式表加载失败"?**
A: 确认 `build\Release\resources\style.qss` 存在。如果不存在,手动复制 `resources\style.qss` 到该路径。

**Q: 窗口模糊 / 文字发虚?**
A: 已在 main.cpp 启用 `AA_EnableHighDpiScaling`。如果你的显示缩放是 125%/15%/175%,Windows 的非整数倍缩放确实会模糊——这是 Qt 在 Windows 上的已知限制,建议设为 100% 或 200%。

### 连接相关

**Q: 状态栏一直显示 "未连接"?**
A:
1. 确认 llama-server 已启动,且监听在正确的 host/port
2. 打开 `http://127.0.0.1:8080/v1/models`,浏览器应返回 JSON
3. 检查 Windows 防火墙是否拦截
4. 在 SettingsDialog 点"测试连接"定位具体错误

**Q: llama-server 返回 404 / 400?**
A: 检查模型路径是否正确,以及 llama-server 的启动参数。llama.cpp 不同版本的 API 略有差异。

### 对话相关

**Q: 流式输出断断续续?**
A: 这是 SSE 正常行为,模型逐 token 生成。首 token 延迟取决于模型 prefill 速度(RTX 5080 + Q4_K_M 约 100-300ms)。

**Q: AI 回复被截断?**
A: 检查 `Max Tokens` 设置,默认 2048 应该够大部分对话。如果还是截断,增大该值。

**Q: 回复内容质量差?**
A: 降低 Temperature(0.1-0.3 更确定性),或换更大的模型。也可以在 system prompt 里加更多指令。

### 压测相关

**Q: QPS 很低?**
A:
1. 增大并发数(但超过 GPU 能处理的范围反而会下降)
2. 调整 llama-server:`-b 2048`(增大 batch size),`-ngl 99`(全 GPU)
3. 换更小模型(Q3_K_M / Q2_K)
4. 减小 max_tokens(压测吞吐时不需要长回复)

**Q: 压测时客户端崩溃?**
A: 可能是并发太高导致 llama-server 拒绝连接。降低并发重试。

**Q: P99 数据异常?**
A: 如果总请求数太少(< 20),P99 等于 max,不具统计意义。建议至少 50 次请求。

### 编译相关

**Q: CMake 找不到 Qt?**
A: 确认 `QT_PREFIX_PATH` 指向包含 `lib/cmake/Qt6` 的目录,如 `D:/gd/Qt/6.7.0/msvc2019_64`。

**Q: 编译报 "httplib.h: No such file"?**
A: 确认 `third_party/httplib.h` 存在。如果丢了,从 `https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h` 下载。

**Q: 编译报 "无法打开包括文件: QHighDpiScaleFactorRoundingPolicy"?**
A: 该头在 Qt 6.7 中不存在,已移除。如果你用更新的 Qt 版本,API 可能变化。

---

## 🗺️ 开发路线图

### 已完成 ✅

- [x] 阶段 0:项目骨架 + CMake + 空窗口
- [x] 阶段 1:Message + SSEParser + 单元测试(8/8 PASS)
- [x] 阶段 2:LLMClient 非流式 + httplib + json + 联调
- [x] 阶段 3:LLMClient 流式 + cancel + SSE ContentReceiver
- [x] 阶段 4:ChatBubble / ChatView / InputBox / MainWindow
- [x] 阶段 5:流式打字机效果 + 停止按钮 + QueuedConnection
- [x] 阶段 6:SettingsDialog + ConfigManager + 状态栏健康检查
- [x] 阶段 7:StressRunner / StressReport / StressPanel + CSV 导出
- [x] 阶段 8:打包 + README + 架构文档
- [x] 阶段 9:.gitignore + style.qss + ChatSession + Logger + 实测压测数据

### 计划中 📋

- [ ] 对话历史持久化(ChatSession 集成到 MainWindow)
- [ ] 多会话标签页(类似 ChatGPT 的对话列表)
- [ ] 模型选择下拉框(从 /v1/models 动态获取)
- [ ] 快捷键体系完善(F2 改配置,Ctrl+N 新对话等)
- [ ] 国际化(中英双语)
- [ ] macOS / Linux 适配(Cross-platform 构建)
- [ ] GitHub Actions CI(自动编译 + 跑测试)
- [ ] 窗口图标 + 应用签名

### 远期 🔮

- [ ] OpenAI / Anthropic API 支持(切换 provider)
- [ ] 插件系统(自定义 prompt 模板、工具调用)
- [ ] 语音输入/输出(Whisper + TTS)
- [ ] RAG 集成(本地知识库检索)
- [ ] GPU 显存实时监控(压测面板内嵌)

---

## 📄 许可证

MIT License — 自由使用、修改、分发。

---

## 🙏 致谢

| 项目 | 用途 | 链接 |
|---|---|---|
| [Qt](https://www.qt.io/) | GUI 框架 | LGPL / Commercial |
| [cpp-httplib](https://github.com/yhirose/cpp-httplib) | HTTP 客户端 | MIT |
| [nlohmann/json](https://github.com/nlohmann/json) | JSON 解析 | MIT |
| [llama.cpp](https://github.com/ggerganov/llama.cpp) | LLM 推理后端 | MIT |

---

**如果这个项目对你有帮助,请给个 ⭐!**
