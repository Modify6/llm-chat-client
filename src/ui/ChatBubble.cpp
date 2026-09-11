#include "ui/ChatBubble.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QRegularExpression>
#include <QMenu>
#include <QContextMenuEvent>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>

/**
 * 简单的 Markdown → HTML 转换器
 *
 * 只处理常用场景,够聊天用:
 * - ```language\n...\n``` → <pre><code>...</code></pre>
 * - `code` → <code>code</code>
 * - **bold** → <b>bold</b>
 * - *italic* → <i>italic</i>
 * - # / ## / ### → <h1>/<h2>/<h3>
 * - - item / * item → <ul><li>item</li></ul>
 * - 1. item → <ol><li>item</li></ol>
 * - > quote → <blockquote>quote</blockquote>
 * - [text](url) → <a href="url">text</a>
 * - 空行分段
 */
static QString markdownToHtml(const QString& md) {
    QString html;
    QStringList lines = md.split('\n', Qt::KeepEmptyParts);

    bool inCodeBlock = false;
    QString codeContent;
    bool inList = false;
    bool inOrderedList = false;
    bool inParagraph = false;

    auto closeListTags = [&]() {
        if (inOrderedList) { html += "</ol>"; inOrderedList = false; }
        if (inList) { html += "</ul>"; inList = false; }
    };
    auto closeParagraph = [&]() {
        if (inParagraph) { html += "</p>"; inParagraph = false; }
    };
    auto closeAll = [&]() { closeListTags(); closeParagraph(); };

    auto inlineFormat = [](QString& line) {
        line.replace(QRegularExpression("`([^`]+)`"), "<code>\\1</code>");
        line.replace(QRegularExpression("\\*\\*([^*]+)\\*\\*"), "<b>\\1</b>");
        line.replace(QRegularExpression("\\*([^*]+)\\*"), "<i>\\1</i>");
        line.replace(QRegularExpression("\\[([^\\]]+)\\]\\(([^)]+)\\)"), "<a href=\"\\2\">\\1</a>");
    };

    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines[i];

        if (line.startsWith("```")) {
            if (!inCodeBlock) {
                inCodeBlock = true;
                codeContent.clear();
                closeAll();
                continue;
            } else {
                inCodeBlock = false;
                html += "<pre><code>" + codeContent.toHtmlEscaped() + "</code></pre>";
                continue;
            }
        }

        if (inCodeBlock) {
            codeContent += line + "\n";
            continue;
        }

        if (line.trimmed().isEmpty()) {
            closeAll();
            continue;
        }

        if (line.startsWith("### ")) {
            closeAll();
            QString text = line.mid(4); inlineFormat(text);
            html += "<h3>" + text + "</h3>"; continue;
        }
        if (line.startsWith("## ")) {
            closeAll();
            QString text = line.mid(3); inlineFormat(text);
            html += "<h2>" + text + "</h2>"; continue;
        }
        if (line.startsWith("# ")) {
            closeAll();
            QString text = line.mid(2); inlineFormat(text);
            html += "<h1>" + text + "</h1>"; continue;
        }

        if (line.startsWith("> ")) {
            closeAll();
            QString text = line.mid(2); inlineFormat(text);
            html += "<blockquote>" + text + "</blockquote>"; continue;
        }

        if (line.startsWith("- ") || line.startsWith("* ")) {
            closeParagraph();
            if (!inList) { html += "<ul>"; inList = true; }
            QString text = line.mid(2); inlineFormat(text);
            html += "<li>" + text + "</li>"; continue;
        }

        QRegularExpression orderedRe("^(\\d+)\\. (.+)$");
        auto match = orderedRe.match(line);
        if (match.hasMatch()) {
            closeParagraph();
            if (!inOrderedList) { html += "<ol>"; inOrderedList = true; }
            QString text = match.captured(2); inlineFormat(text);
            html += "<li>" + text + "</li>"; continue;
        }

        closeListTags();
        QString text = line; inlineFormat(text);
        if (!inParagraph) { html += "<p>"; inParagraph = true; }
        else { html += "<br>"; }
        html += text;
    }

    closeAll();
    return html;
}

/**
 * 用 <style> 标签注入全局样式
 * QLabel 支持 <style> 标签内的 CSS,比内联 style 更干净
 */
static QString wrapWithStyle(const QString& html) {
    return QString(
        "<style>"
        "body { font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;"
        "  font-size: 13px; color: #212121; line-height: 1.6; }"
        "pre { background: #F5F5F5; border: 1px solid #E0E0E0;"
        "  border-radius: 6px; padding: 10px 12px;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px; white-space: pre-wrap; }"
        "code { background: #F0F0F0; border-radius: 3px;"
        "  padding: 1px 5px; font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px; color: #C62828; }"
        "pre code { background: transparent; padding: 0; color: #212121; }"
        "h1 { font-size: 18px; color: #2E7D32; margin: 10px 0 6px 0; }"
        "h2 { font-size: 16px; color: #2E7D32; margin: 8px 0 4px 0; }"
        "h3 { font-size: 14px; color: #2E7D32; margin: 6px 0 4px 0; }"
        "a { color: #2196F3; text-decoration: underline; }"
        "blockquote { border-left: 3px solid #4CAF50;"
        "  margin: 6px 0; padding: 4px 12px; background: #F1F8E9; color: #558B2F; }"
        "ul { margin: 4px 0; padding-left: 20px; }"
        "ol { margin: 4px 0; padding-left: 20px; }"
        "li { margin: 2px 0; }"
        "p { margin: 4px 0; }"
        "</style>"
        "%1"
    ).arg(html);
}

ChatBubble* ChatBubble::user(const QString& text, QWidget* parent) {
    return new ChatBubble(Role::User, text, parent);
}

ChatBubble* ChatBubble::assistant(const QString& text, QWidget* parent) {
    return new ChatBubble(Role::Assistant, text, parent);
}

ChatBubble::ChatBubble(Role role, const QString& text, QWidget* parent)
    : QFrame(parent)
    , m_role(role)
    , m_label(new QLabel(this))
    , m_rawText(text) {

    // QLabel 配置:自动换行 + 可选文本 + 链接点击
    m_label->setWordWrap(true);
    m_label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    m_label->setOpenExternalLinks(true);

    // 关键:让 QLabel 不收缩,高度随内容
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 用属性选择器让 style.qss 区分 user / assistant
    setProperty("role", (role == Role::User) ? "user" : "assistant");

    // VBoxLayout 包裹
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(0);
    layout->addWidget(m_label);

    renderContent();
}

void ChatBubble::appendText(const QString& text) {
    m_rawText += text;
    renderContent();
}

void ChatBubble::renderContent() {
    if (m_role == Role::Assistant) {
        QString html = wrapWithStyle(markdownToHtml(m_rawText));
        m_label->setText(html);
    } else {
        m_label->setText(m_rawText);
    }

    // QLabel + wordWrap:设置 maximumWidth 后它会自动换行并扩展高度
    // 不需要手动算 document 高度,QLabel 自己搞定
    // 但需要通知布局系统尺寸变了
    m_label->updateGeometry();
    updateGeometry();
}

// =====================================================================
// 复制功能:右键菜单
// =====================================================================

/**
 * @brief 从 Markdown 文本里提取所有 ```...``` 代码块内容
 *
 * 支持两种 fence:
 *   ```cpp\ncode here\n```   (带语言标记)
 *   ```\ncode here\n```       (无语言标记)
 *
 * @return 所有代码块拼接,块之间用 \n\n 分隔;无代码块时返回空字符串
 */
static QString extractCodeBlocks(const QString& markdown) {
    QStringList blocks;
    // 正则:``` 可选语言标记 换行 捕获直到下一个 ```
    QRegularExpression re("```(?:\\w+)?\\n([\\s\\S]*?)```");
    auto it = re.globalMatch(markdown);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString code = match.captured(1).trimmed();
        if (!code.isEmpty()) {
            blocks.append(code);
        }
    }
    return blocks.join("\n\n");
}

/**
 * @brief 右键菜单
 *
 * 用户气泡:
 *   └─ 复制消息
 *
 * AI 气泡:
 *   ├─ 复制消息
 *   └─ 复制所有代码块(若无代码块则灰掉)
 */
void ChatBubble::contextMenuEvent(QContextMenuEvent* event) {
    QMenu menu(this);

    // --- 复制消息(所有气泡都有) ---
    QAction* actCopyMsg = menu.addAction("复制消息");
    actCopyMsg->setShortcut(QKeySequence::Copy);

    // --- 复制代码块(仅 AI 气泡) ---
    QAction* actCopyCode = nullptr;
    if (m_role == Role::Assistant) {
        QString code = extractCodeBlocks(m_rawText);
        actCopyCode = menu.addAction("复制所有代码块");
        actCopyCode->setEnabled(!code.isEmpty());   // 无代码块时灰掉
        if (code.isEmpty()) {
            actCopyCode->setToolTip("这条回复里没有代码块");
        }
    }

    // 弹出菜单,等待用户选择
    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen) return;

    QClipboard* cb = QApplication::clipboard();

    if (chosen == actCopyMsg) {
        cb->setText(m_rawText);
    } else if (chosen == actCopyCode) {
        QString code = extractCodeBlocks(m_rawText);
        if (!code.isEmpty()) {
            cb->setText(code);
        }
    }
}
