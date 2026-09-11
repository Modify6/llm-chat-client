#include <QtTest>
#include "core/SSEParser.h"

class TestSSEParser : public QObject {
    Q_OBJECT

private slots:
    void test_single_event() {
        SSEParser p;
        auto events = p.feed("data: {\"a\":1}\n\n");
        QCOMPARE(events.size(), 1u);
        QCOMPARE(QString::fromStdString(events[0]), QString("data: {\"a\":1}"));
    }

    void test_multiple_events() {
        SSEParser p;
        auto events = p.feed("data: 1\n\ndata: 2\n\n");
        QCOMPARE(events.size(), 2u);
        QCOMPARE(QString::fromStdString(events[0]), QString("data: 1"));
        QCOMPARE(QString::fromStdString(events[1]), QString("data: 2"));
    }

    void test_split_across_chunks() {
        SSEParser p;
        auto e1 = p.feed("data: 1\n");
        QCOMPARE(e1.size(), 0u);   // 还没凑够 \n\n
        auto e2 = p.feed("\n");
        QCOMPARE(e2.size(), 1u);   // 现在完整了
        QCOMPARE(QString::fromStdString(e2[0]), QString("data: 1"));
    }

    void test_done_marker() {
        SSEParser p;
        auto events = p.feed("data: [DONE]\n\n");
        QCOMPARE(events.size(), 1u);
        QCOMPARE(QString::fromStdString(events[0]), QString("data: [DONE]"));
    }

    void test_empty_chunk() {
        SSEParser p;
        auto events = p.feed("");
        QCOMPARE(events.size(), 0u);
    }

    // 额外覆盖:注释行 + \r\n 兼容 + reset
    void test_ignore_comments() {
        SSEParser p;
        auto events = p.feed(": this is comment\ndata: real\n\n");
        QCOMPARE(events.size(), 1u);
        QCOMPARE(QString::fromStdString(events[0]), QString("data: real"));
    }

    void test_crlf_separator() {
        SSEParser p;
        auto events = p.feed("data: a\r\n\r\n");
        QCOMPARE(events.size(), 1u);
        QCOMPARE(QString::fromStdString(events[0]), QString("data: a"));
    }

    void test_reset_discards_buffer() {
        SSEParser p;
        p.feed("data: incomplete");  // 没 \n\n,留在 buffer
        p.reset();
        auto events = p.feed("data: fresh\n\n");
        QCOMPARE(events.size(), 1u);
        QCOMPARE(QString::fromStdString(events[0]), QString("data: fresh"));
    }
};

QTEST_MAIN(TestSSEParser)
#include "test_sse_parser.moc"
