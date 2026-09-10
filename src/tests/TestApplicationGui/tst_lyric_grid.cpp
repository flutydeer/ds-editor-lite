#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Modules/FillLyric/Controls/CellList.h"
#include "Modules/FillLyric/Controls/LyricCell.h"
#include "Modules/FillLyric/Controls/LyricWrapView.h"
#include "Modules/FillLyric/Utils/G2pService.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QMenu>
#include <QPointer>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTimer>
#include <QWindow>
#include <QtTest/QTest>

namespace {
    using Rows = QList<QStringList>;

    Rows words(const FillLyric::LyricWrapView &grid) {
        Rows rows;
        for (const auto *line : grid.cellLists()) {
            QStringList row;
            for (const auto *cell : line->m_cells)
                row.append(cell->lyric());
            rows.append(row);
        }
        return rows;
    }

    void showGrid(FillLyric::LyricWrapView &grid, const Rows &rows) {
        QList<QList<LangNote>> notes;
        for (const auto &row : rows) {
            QList<LangNote> line;
            for (const auto &word : row)
                line.append(LangNote(word, QStringLiteral("eng"), kUnknownG2pId));
            notes.append(line);
        }
        grid.resize(800, 400);
        grid.init(notes);
        grid.show();
        grid.activateWindow();
        grid.setFocus();
        QTRY_VERIFY(grid.isActiveWindow());
        QTRY_VERIFY(grid.hasFocus());
        QCOMPARE(words(grid), rows);
    }

    QPoint cellPosition(FillLyric::LyricWrapView &grid, int line, int column) {
        return grid.mapFromScene(
            grid.cellLists().at(line)->m_cells.at(column)->lyricRect().center());
    }

    QPoint handlePosition(FillLyric::LyricWrapView &grid, int line) {
        for (auto *item : grid.cellLists().at(line)->childItems()) {
            if (auto *handle = dynamic_cast<FillLyric::HandleItem *>(item))
                return grid.mapFromScene(handle->sceneBoundingRect().center());
        }
        return {-1, -1};
    }

    QList<int> selectedLines(const FillLyric::LyricWrapView &grid) {
        QList<int> selected;
        const auto lines = grid.cellLists();
        for (int index = 0; index < lines.size(); ++index) {
            for (auto *item : lines.at(index)->childItems()) {
                if (dynamic_cast<FillLyric::HandleItem *>(item) && item->isSelected())
                    selected.append(index);
            }
        }
        return selected;
    }

    void clickAt(FillLyric::LyricWrapView &grid, const QPoint &position,
                 Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
        QVERIFY(grid.viewport()->rect().contains(position));
        QTest::mouseClick(grid.viewport(), Qt::LeftButton, modifiers, position);
    }

    void chooseMenu(FillLyric::LyricWrapView &grid, const QPoint &position, const QString &text) {
        QVERIFY(grid.viewport()->rect().contains(position));
        bool entered = false;
        QTimer action;
        action.setSingleShot(true);
        QObject::connect(&action, &QTimer::timeout, &grid, [&] {
            QPointer<QMenu> menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            QVERIFY(menu);
            const auto closeMenu = qScopeGuard([&] {
                if (menu)
                    menu->close();
            });
            entered = true;
            QAction *selected = nullptr;
            for (auto *candidate : menu->actions()) {
                if (candidate->text() == text)
                    selected = candidate;
            }
            QVERIFY2(selected, qPrintable(text));
            QVERIFY(selected->isEnabled());
            QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                              menu->actionGeometry(selected).center());
        });
        const auto previousCursor = QCursor::pos();
        const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(previousCursor); });
        const auto global = grid.viewport()->mapToGlobal(position);
        QCursor::setPos(global);
        QTest::mouseMove(grid.windowHandle(), grid.mapFromGlobal(global));
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, global);
        action.start(0);
        QApplication::sendEvent(grid.viewport(), &event);
        action.stop();
        QVERIFY(entered);
    }

    QString cellMenuText(const char *text) {
        return QCoreApplication::translate("LyricCell", text);
    }
}

void ApplicationGuiTests::lyricGridSelectionDeletesOnlyChosenWords_data() {
    QTest::addColumn<QString>("selection");
    QTest::addColumn<Rows>("expected");
    QTest::newRow("single-word") << QStringLiteral("single")
                                 << Rows{
                                        {"one", "three"},
                                        {"four", "five"},
                                        {"six"}
    };
    QTest::newRow("shift-across-lines")
        << QStringLiteral("shift") << Rows{{"one"}, {"five"}, {"six"}};
    QTest::newRow("reverse-drag") << QStringLiteral("drag") << Rows{{"one"}, {"five"}, {"six"}};
    QTest::newRow("whole-line") << QStringLiteral("line")
                                << Rows{
                                       {"four", "five"},
                                       {"six"}
    };
    QTest::newRow("last-word-in-line") << QStringLiteral("last")
                                       << Rows{
                                              {"one", "two", "three"},
                                              {"four", "five"}
    };
}

void ApplicationGuiTests::lyricGridSelectionDeletesOnlyChosenWords() {
    QFETCH(QString, selection);
    QFETCH(Rows, expected);
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    const auto before = context->m_coreRuntime->documentVersion();
    FillLyric::G2pService g2p(singingClip->singerIdentifier(),
                              SynthrtEngine::instance().languageService());
    FillLyric::LyricWrapView grid({}, {QStringLiteral("eng")}, &g2p);
    showGrid(grid, {
                       {"one", "two", "three"},
                       {"four", "five"},
                       {"six"}
    });
    if (QTest::currentTestFailed())
        return;
    QSignalSpy countChanged(&grid, &FillLyric::LyricWrapView::noteCountChanged);
    if (selection == QStringLiteral("line")) {
        clickAt(grid, handlePosition(grid, 0));
    } else if (selection == QStringLiteral("last")) {
        clickAt(grid, cellPosition(grid, 2, 0));
    } else if (selection == QStringLiteral("drag")) {
        const auto start = cellPosition(grid, 1, 0);
        const auto finish = cellPosition(grid, 0, 1);
        QTest::mousePress(grid.viewport(), Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(grid.windowHandle(),
                         grid.mapFromGlobal(grid.viewport()->mapToGlobal(finish)));
        QTest::mouseRelease(grid.viewport(), Qt::LeftButton, Qt::NoModifier, finish);
    } else {
        clickAt(grid, cellPosition(grid, 0, 1));
        if (selection == QStringLiteral("shift"))
            clickAt(grid, cellPosition(grid, 1, 0), Qt::ShiftModifier);
    }
    if (QTest::currentTestFailed())
        return;
    QTest::keyClick(&grid, Qt::Key_Delete);
    QCOMPARE(words(grid), expected);
    int count = 0;
    for (const auto &row : expected)
        count += row.size();
    QTRY_VERIFY(!countChanged.isEmpty());
    QTRY_COMPARE(countChanged.last().first().toInt(), count);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::lyricGridMovesSelectedLinesTogether_data() {
    QTest::addColumn<bool>("menu");
    QTest::newRow("keyboard") << false;
    QTest::newRow("context-menu") << true;
}

void ApplicationGuiTests::lyricGridMovesSelectedLinesTogether() {
    QFETCH(bool, menu);
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    FillLyric::G2pService g2p(singingClip->singerIdentifier(),
                              SynthrtEngine::instance().languageService());
    FillLyric::LyricWrapView grid({}, {QStringLiteral("eng")}, &g2p);
    const Rows original{{"one"}, {"two"}, {"three"}, {"four"}};
    showGrid(grid, original);
    if (QTest::currentTestFailed())
        return;
    clickAt(grid, handlePosition(grid, 2));
    QCOMPARE(selectedLines(grid), QList<int>{2});
    clickAt(grid, handlePosition(grid, 1), Qt::ControlModifier);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(selectedLines(grid), (QList<int>{1, 2}));
    auto *firstSelected = grid.cellLists().at(1);
    auto *secondSelected = grid.cellLists().at(2);
    const auto move = [&](bool up) {
        if (menu)
            chooseMenu(grid, handlePosition(grid, grid.cellLists().indexOf(firstSelected)),
                       FillLyric::LyricWrapView::tr(up ? "move up" : "move down"));
        else
            QTest::keyClick(&grid, up ? Qt::Key_Up : Qt::Key_Down);
    };
    move(true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{{"two"}, {"three"}, {"one"}, {"four"}}));
    for (int index = 1; index < grid.cellLists().size(); ++index)
        QVERIFY(grid.cellLists().at(index - 1)->y() < grid.cellLists().at(index)->y());
    QCOMPARE(grid.cellLists().first(), firstSelected);
    QCOMPARE(grid.cellLists().at(1), secondSelected);
    move(false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), original);
    for (int index = 1; index < grid.cellLists().size(); ++index)
        QVERIFY(grid.cellLists().at(index - 1)->y() < grid.cellLists().at(index)->y());
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::lyricGridSplitKeepsTheNewLineEditable() {
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    FillLyric::G2pService g2p(singingClip->singerIdentifier(),
                              SynthrtEngine::instance().languageService());
    FillLyric::LyricWrapView grid({}, {QStringLiteral("eng")}, &g2p);
    showGrid(grid, {
                       {"one", "two", "three"}
    });
    if (QTest::currentTestFailed())
        return;
    chooseMenu(grid, cellPosition(grid, 0, 1), cellMenuText("linebreak"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{
                              {"one"},
                              {"two", "three"}
    }));
    clickAt(grid, cellPosition(grid, 1, 0));
    chooseMenu(grid, cellPosition(grid, 1, 0), cellMenuText("delete cell"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{{"one"}, {"three"}}));
    clickAt(grid, cellPosition(grid, 0, 0));
    QTest::keyClick(&grid, Qt::Key_Delete);
    QCOMPARE(words(grid), (Rows{{"three"}}));
    chooseMenu(grid, cellPosition(grid, 0, 0), cellMenuText("add next cell"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{
                              {"three", ""}
    }));
    chooseMenu(grid, cellPosition(grid, 0, 1), cellMenuText("delete cell"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{{"three"}}));
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::lyricGridMenusInsertAndClearWords() {
    createLyricSelection();
    if (QTest::currentTestFailed())
        return;
    FillLyric::G2pService g2p(singingClip->singerIdentifier(),
                              SynthrtEngine::instance().languageService());
    FillLyric::LyricWrapView grid({}, {QStringLiteral("eng")}, &g2p);
    showGrid(grid, {
                       {"one", "two"},
                       {"three"}
    });
    if (QTest::currentTestFailed())
        return;
    chooseMenu(grid, cellPosition(grid, 0, 1), cellMenuText("add prev cell"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{
                              {"one", "", "two"},
                              {"three"}
    }));
    clickAt(grid, cellPosition(grid, 0, 0));
    clickAt(grid, cellPosition(grid, 0, 2), Qt::ControlModifier);
    chooseMenu(grid, cellPosition(grid, 0, 0), FillLyric::LyricWrapView::tr("clear cells"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{
                              {"", "", ""},
                              {"three"}
    }));
    clickAt(grid, handlePosition(grid, 1));
    chooseMenu(grid, handlePosition(grid, 1), FillLyric::CellList::tr("add prev line"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{
                              {"", "", ""},
                              {""},
                              {"three"}
    }));
    clickAt(grid, handlePosition(grid, 2));
    chooseMenu(grid, handlePosition(grid, 2), FillLyric::CellList::tr("add next line"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{
                              {"", "", ""},
                              {""},
                              {"three"},
                              {""}
    }));
    clickAt(grid, handlePosition(grid, 3));
    chooseMenu(grid, handlePosition(grid, 3), FillLyric::CellList::tr("append cell"));
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(words(grid), (Rows{
                              {"", "", ""},
                              {""},
                              {"three"},
                              {"", ""}
    }));
    QVERIFY(!historyManager->canUndo());
}
