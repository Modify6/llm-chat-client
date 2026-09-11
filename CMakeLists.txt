cmake_minimum_required(VERSION 3.31)
project(llm-chat-client VERSION 1.0 LANGUAGES CXX)

# C++17 (硬性约束,不要升级)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt MOC/RCC/UIC 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 路径配置(支持迁移):
# 1. 优先使用 -DQT_PREFIX_PATH=... 命令行覆盖
# 2. 其次使用 Qt6_DIR 环境变量
# 3. 默认值指向当前开发机路径,迁移时改这里或设环境变量
set(QT_PREFIX_PATH "D:/gd/llama/Qt/6.7.0/msvc2019_64"
    CACHE PATH "Qt 安装路径(指向包含 lib/cmake/Qt6 的那一层)")

# 让 find_package 能定位 Qt6
if(NOT DEFINED CMAKE_PREFIX_PATH)
    list(APPEND CMAKE_PREFIX_PATH ${QT_PREFIX_PATH})
endif()

# 第三方头文件目录
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)

# 主程序源文件
set(SOURCES
    src/main.cpp
    src/core/Message.cpp
    src/core/SSEParser.cpp
    src/core/LLMClient.cpp
    src/core/ChatSession.cpp
    src/ui/ChatBubble.cpp
    src/ui/ChatView.cpp
    src/ui/InputBox.cpp
    src/ui/SettingsDialog.cpp
    src/ui/StressPanel.cpp
    src/ui/MainWindow.cpp
    src/utils/ConfigManager.cpp
    src/utils/Logger.cpp
    src/stress/StressReport.cpp
    src/stress/StressRunner.cpp
)

set(HEADERS
    src/core/Message.h
    src/core/SSEParser.h
    src/core/LLMClient.h
    src/core/ChatSession.h
    src/ui/ChatBubble.h
    src/ui/ChatView.h
    src/ui/InputBox.h
    src/ui/SettingsDialog.h
    src/ui/StressPanel.h
    src/ui/MainWindow.h
    src/utils/ConfigManager.h
    src/utils/Logger.h
    src/stress/StressReport.h
    src/stress/StressRunner.h
)

# Qt 组件
find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent Test)

add_executable(llm-chat-client WIN32 ${SOURCES} ${HEADERS})

target_include_directories(llm-chat-client PRIVATE
    ${THIRD_PARTY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(llm-chat-client PRIVATE
    Qt6::Widgets
    Qt6::Concurrent
)

# httplib 在 Windows 上需要的系统库(Winsock / 加密 API)
if(WIN32)
    target_link_libraries(llm-chat-client PRIVATE ws2_32 crypt32)

    # ====== POST_BUILD:自动部署 Qt DLL + 资源 ======
    # windeployqt 能扫到大部分 DLL,但 QtConcurrent(模板库)扫不到,需手动补
    # 同时把 resources/style.qss 复制到 exe 旁边
    find_program(WINDEPLOYQT_EXE windeployqt HINTS ${QT_PREFIX_PATH}/bin)
    add_custom_command(TARGET llm-chat-client POST_BUILD
        COMMAND ${WINDEPLOYQT_EXE} $<TARGET_FILE:llm-chat-client> --release --no-translations
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QT_PREFIX_PATH}/bin/Qt6Concurrent.dll"
                "$<TARGET_FILE_DIR:llm-chat-client>"
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:llm-chat-client>/resources"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/resources/style.qss"
                "$<TARGET_FILE_DIR:llm-chat-client>/resources/style.qss"
        COMMENT "部署 Qt 运行库 + 资源文件"
    )
endif()

# ====== 单元测试 ======
enable_testing()

# SSEParser 单元测试(独立可执行,不依赖 Widgets)
add_executable(test_sse_parser tests/test_sse_parser.cpp src/core/SSEParser.cpp)
target_include_directories(test_sse_parser PRIVATE
    ${THIRD_PARTY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_sse_parser PRIVATE Qt6::Test)
add_test(NAME test_sse_parser COMMAND test_sse_parser)

# LLMClient 端到端测试(控制台程序,需要 llama-server 运行)
add_executable(test_llm_chat tests/test_llm_chat.cpp src/core/LLMClient.cpp src/core/Message.cpp src/core/SSEParser.cpp)
target_include_directories(test_llm_chat PRIVATE
    ${THIRD_PARTY_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(test_llm_chat PRIVATE Qt6::Core)
if(WIN32)
    target_link_libraries(test_llm_chat PRIVATE ws2_32 crypt32)
endif()
