#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "UI/Window/LogWindow.h"

#include <lite/History/HistoryManager.h>
#include <lite/Support/LogBus.h>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QKeySequence>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QtTest>

#include <thread>

namespace {
    void selectComboEntry(QComboBox *combo, int index) {
        QVERIFY(combo);
        QVERIFY(index >= 0);
        combo->setFocus();
        QTest::keyClick(combo, Qt::Key_Home);
        for (int row = 0; row < index; ++row)
            QTest::keyClick(combo, Qt::Key_Down);
        QCOMPARE(combo->currentIndex(), index);
    }
}

void ApplicationGuiTests::logWindowFiltersLiveMessagesAndCopiesDisplayedOrder() {
    const QString firstTag = QStringLiteral("FixtureLog-A");
    const QString secondTag = QStringLiteral("FixtureLog-B");
    const Log::LogMessage debug("10:00:00", Log::Debug, firstTag, "Inspect source");
    const Log::LogMessage warning("10:00:01", Log::Warning, firstTag, "Missing source");
    const Log::LogMessage error("10:00:02", Log::Error, secondTag, "Output failed");
    const Log::LogMessage recovered("10:00:03", Log::Warning, secondTag, "Output recovered");
    LogBus::instance()->append(debug);
    LogBus::instance()->append(warning);
    LogBus::instance()->append(error);
    const auto before = context->m_coreRuntime->documentVersion();
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    LogWindow window;
    window.resize(760, 360);
    window.show();
    window.activateWindow();
    auto *search = window.findChild<QLineEdit *>();
    auto *table = window.findChild<QTableView *>(QStringLiteral("logTableView"));
    auto *clear = window.findChild<QPushButton *>();
    QVERIFY(search);
    QVERIFY(table);
    QVERIFY(clear);
    QComboBox *level = nullptr;
    QComboBox *tag = nullptr;
    for (auto *combo : window.findChildren<QComboBox *>()) {
        if (combo->findData(Log::Fatal) >= 0)
            level = combo;
        else
            tag = combo;
    }
    QVERIFY(level);
    QVERIFY(tag);
    QTRY_VERIFY(window.isActiveWindow());
    search->setFocus();
    QTest::keyClicks(search, "fixturelog-");
    QTRY_COMPARE(table->model()->rowCount(), 3);
    std::thread producer([&] { LogBus::instance()->append(recovered); });
    producer.join();
    QTRY_COMPARE(table->model()->rowCount(), 4);

    selectComboEntry(level, level->findData(Log::Warning));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(table->model()->rowCount(), 3);
    selectComboEntry(tag, tag->findText(firstTag));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(table->model()->rowCount(), 1);
    QCOMPARE(table->model()->index(0, LogWindowModel::TextColumn).data().toString(), warning.text);
    search->setFocus();
    search->selectAll();
    QTest::keyClicks(search, "OUTPUT");
    QCOMPARE(table->model()->rowCount(), 0);
    selectComboEntry(tag, tag->findText(secondTag));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(table->model()->rowCount(), 2);

    const auto rowPoint = [&](int row) {
        return table->visualRect(table->model()->index(row, LogWindowModel::TextColumn))
            .intersected(table->viewport()->rect())
            .center();
    };
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::NoModifier, rowPoint(1));
    QTest::mouseClick(table->viewport(), Qt::LeftButton, Qt::ControlModifier, rowPoint(0));
    QCOMPARE(table->selectionModel()->selectedRows().size(), 2);
    QTest::keySequence(table, QKeySequence::Copy);
    QTRY_COMPARE(QApplication::clipboard()->text(),
                 error.toPlainText() + QLatin1Char('\n') + recovered.toPlainText());
    QTest::mouseClick(clear, Qt::LeftButton);
    QCOMPARE(table->model()->rowCount(), 0);
    search->clear();
    QCOMPARE(table->model()->rowCount(), 0);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
    window.close();
}
