#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QtTest>

using namespace AutomationWire;

class TestAutomationCursor final : public QObject {
    Q_OBJECT

private slots:

    void roundTrip_data() {
        QTest::addColumn<qint64>("offset");
        QTest::newRow("first-page") << qint64(0);
        QTest::newRow("following-page") << qint64(37);
        QTest::newRow("maximum-safe-integer") << qint64(MaximumSafeJsonInteger);
    }

    void roundTrip() {
        QFETCH(qint64, offset);
        OpaqueCursorCodec codec;
        const auto token = codec.issue("manifest:l2", "snapshot-a", offset);
        QVERIFY(!token.isEmpty());
        QVERIFY(!token.contains(u'='));
        QVERIFY(!token.contains(u'+'));
        QVERIFY(!token.contains(u'/'));
        const auto parsed = codec.parse(token, "manifest:l2", "snapshot-a");
        QVERIFY(parsed.valid());
        QCOMPARE(*parsed.offset, offset);
    }

    void invalidOffset_data() {
        QTest::addColumn<qint64>("offset");
        QTest::newRow("negative") << qint64(-1);
        QTest::newRow("unsafe-integer") << qint64(MaximumSafeJsonInteger + 1);
    }

    void invalidOffset() {
        QFETCH(qint64, offset);
        OpaqueCursorCodec codec;
        QVERIFY(codec.issue("manifest:l2", "snapshot-a", offset).isEmpty());
    }

    void changedScopeInvalidatesCursor_data() {
        QTest::addColumn<QString>("context");
        QTest::addColumn<QString>("snapshot");
        QTest::addColumn<int>("error");
        QTest::newRow("other-collection") << QString("tasks:list") << QString("snapshot-a")
                                          << int(OpaqueCursorError::ContextMismatch);
        QTest::newRow("changed-collection") << QString("manifest:l2") << QString("snapshot-b")
                                            << int(OpaqueCursorError::SnapshotMismatch);
    }

    void changedScopeInvalidatesCursor() {
        QFETCH(QString, context);
        QFETCH(QString, snapshot);
        QFETCH(int, error);
        OpaqueCursorCodec codec;
        const auto token = codec.issue("manifest:l2", "snapshot-a", 37);
        QCOMPARE(int(codec.parse(token, context, snapshot).error), error);
    }

    void malformedCursor_data() {
        QTest::addColumn<QString>("token");
        QTest::newRow("empty") << QString();
        QTest::newRow("truncated") << QString("abc");
        QTest::newRow("foreign-format") << QString("a.b.c");
        QTest::newRow("invalid-base64") << QString("*");
    }

    void malformedCursor() {
        QFETCH(QString, token);
        OpaqueCursorCodec codec;
        QVERIFY(!codec.parse(token, "manifest:l2", "snapshot-a").valid());
    }
};

QTEST_GUILESS_MAIN(TestAutomationCursor)
#include "main.moc"
