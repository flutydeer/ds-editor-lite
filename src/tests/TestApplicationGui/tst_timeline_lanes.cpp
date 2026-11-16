#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "UI/Dialogs/Timeline/EditTempoDialog.h"
#include "UI/Dialogs/Timeline/EditTimeSignatureDialog.h"
#include "UI/Views/TrackEditor/InfoLane/TimeAxisLaneView.h"
#include "UI/Views/TrackEditor/InfoLane/TempoLaneView.h"
#include "UI/Views/TrackEditor/InfoLane/TimeSignatureLaneView.h"

#include <lite/GUI/Controls/AccentButton.h>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/SvsExpressionDoubleSpinBox.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>
#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QPointer>
#include <QScopeGuard>
#include <QTimer>
#include <QtTest/QTest>

#include <memory>

void ApplicationGuiTests::timelineLaneInputsEditAndRemoveMarkers_data() {
    QTest::addColumn<bool>("timeSignature");
    QTest::newRow("tempo") << false;
    QTest::newRow("time-signature") << true;
}

void ApplicationGuiTests::timelineLaneInputsEditAndRemoveMarkers() {
    QFETCH(bool, timeSignature);
    auto &runtime = *context->m_coreRuntime;
    auto *model = context->m_appModel;
    QVERIFY(runtime.timeline().setTempo(commandContext(), 0, 120));
    QVERIFY(runtime.timeline().setTimeSignature(commandContext(), 0, 4, 4));
    std::unique_ptr<InfoLaneView> lane;
    if (timeSignature)
        lane = std::make_unique<TimeSignatureLaneView>();
    else
        lane = std::make_unique<TempoLaneView>();
    lane->resize(800, 32);
    lane->setTimeRange(0, 9600);
    lane->show();
    lane->activateWindow();
    QTRY_VERIFY(lane->isActiveWindow());
    historyManager->reset();
    const auto original = TestSupport::projectSnapshot(*model);
    const auto before = runtime.documentVersion();
    const QPoint blank(qRound(3840.0 / 9600 * lane->width()), lane->height() / 2);
    const QPoint marker = blank + QPoint(8, 0);

    const auto editAt = [&](const QPoint &position, bool accept, int value) {
        bool responded = false;
        QTimer answer;
        answer.setInterval(10);
        connect(&answer, &QTimer::timeout, lane.get(), [&] {
            QPointer<OKCancelDialog> dialog =
                qobject_cast<OKCancelDialog *>(QApplication::activeModalWidget());
            if (!dialog)
                return;
            answer.stop();
            const auto close = qScopeGuard([&] {
                if (dialog && dialog->isVisible())
                    dialog->reject();
            });
            if (timeSignature) {
                QVERIFY(qobject_cast<EditTimeSignatureDialog *>(dialog.data()));
                auto *numerator = dialog->findChild<SVS::ExpressionSpinBox *>("spinNumerator");
                auto *denominator = dialog->findChild<QComboBox *>("cbDenominator");
                QVERIFY(numerator && denominator);
                QTest::mouseClick(numerator, Qt::LeftButton);
                QTest::keySequence(numerator, QKeySequence::SelectAll);
                QTest::keyClicks(numerator, QString::number(value));
                QTest::keyClick(numerator, Qt::Key_Tab);
                QTest::keyClick(denominator, Qt::Key_Home);
                for (int i = 0; i < denominator->findData(8); ++i)
                    QTest::keyClick(denominator, Qt::Key_Down);
                QCOMPARE(denominator->currentData().toInt(), 8);
            } else {
                QVERIFY(qobject_cast<EditTempoDialog *>(dialog.data()));
                // The header reports the point's musical position, not its raw tick.
                auto *message = dialog->findChild<QLabel *>(QStringLiteral("DialogMessageLabel"));
                QVERIFY(message);
                QCOMPARE(message->text(), QStringLiteral("003:01:000"));
                auto *tempo = dialog->findChild<SVS::ExpressionDoubleSpinBox *>("spinTempo");
                QVERIFY(tempo);
                QTest::mouseClick(tempo, Qt::LeftButton);
                QTest::keySequence(tempo, QKeySequence::SelectAll);
                QTest::keyClicks(tempo, QString::number(value));
                QTest::keyClick(tempo, Qt::Key_Tab);
            }
            QTest::mouseClick(accept ? static_cast<QWidget *>(dialog->okButton())
                                     : static_cast<QWidget *>(dialog->cancelButton()),
                              Qt::LeftButton);
            responded = true;
        });
        answer.start();
        QTest::mouseDClick(lane.get(), Qt::LeftButton, Qt::NoModifier, position);
        QTest::mouseRelease(lane.get(), Qt::LeftButton, Qt::NoModifier, position);
        QVERIFY(responded);
    };
    const int createdValue = timeSignature ? 6 : 150;
    const int editedValue = timeSignature ? 5 : 90;
    const auto checkMarker = [&](int value) {
        const auto &timeline = model->timeline();
        if (timeSignature) {
            QCOMPARE(timeline.timeSignatures(), QList<TimeSignature>({
                                                    {0, 4,     4},
                                                    {2, value, 8}
            }));
            QCOMPARE(timeline.tempos(), QList<Tempo>({
                                            {0, 120}
            }));
        } else {
            QCOMPARE(timeline.tempos(), QList<Tempo>({
                                            {0,    120          },
                                            {3840, double(value)}
            }));
            QCOMPARE(timeline.timeSignatures(), QList<TimeSignature>({
                                                    {0, 4, 4}
            }));
        }
    };
    editAt(blank, false, createdValue);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*model), original);
    QVERIFY(!historyManager->canUndo());
    editAt(blank, true, createdValue);
    if (QTest::currentTestFailed())
        return;
    checkMarker(createdValue);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    editAt(marker, true, editedValue);
    if (QTest::currentTestFailed())
        return;
    checkMarker(editedValue);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 2);

    const auto removeAt = [&](const QPoint &position, bool allowed) {
        bool inspected = false;
        QTimer choose;
        choose.setInterval(10);
        connect(&choose, &QTimer::timeout, lane.get(), [&] {
            QPointer<QMenu> menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu)
                return;
            choose.stop();
            const auto close = qScopeGuard([&] {
                if (menu && menu->isVisible())
                    menu->close();
            });
            QAction *remove = nullptr;
            const auto label = timeSignature ? TimeSignatureLaneView::tr("Remove Time Signature")
                                             : TempoLaneView::tr("Remove Tempo");
            for (auto *action : menu->actions()) {
                if (action->text() == label)
                    remove = action;
            }
            QVERIFY(remove);
            QCOMPARE(remove->isEnabled(), allowed);
            if (allowed)
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier,
                                  menu->actionGeometry(remove).center());
            else
                QTest::keyClick(menu, Qt::Key_Escape);
            inspected = true;
        });
        choose.start();
        QContextMenuEvent menu(QContextMenuEvent::Mouse, position, lane->mapToGlobal(position));
        QApplication::sendEvent(lane.get(), &menu);
        QVERIFY(inspected);
    };
    const auto beforeProtected = runtime.documentVersion();
    removeAt(QPoint(8, lane->height() / 2), false);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(runtime.documentVersion(), beforeProtected);
    checkMarker(editedValue);
    removeAt(marker, true);
    if (QTest::currentTestFailed())
        return;
    QCOMPARE(TestSupport::projectSnapshot(*model), original);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 3);
    QVERIFY(runtime.history().undo(commandContext()));
    checkMarker(editedValue);
    QVERIFY(runtime.history().undo(commandContext()));
    checkMarker(createdValue);
    QVERIFY(runtime.history().undo(commandContext()));
    QCOMPARE(TestSupport::projectSnapshot(*model), original);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::tempoDialogReportsTheBarBeatTickPosition() {
    auto &runtime = *context->m_coreRuntime;
    QVERIFY(runtime.timeline().setTimeSignature(commandContext(), 0, 4, 4));
    QVERIFY(runtime.timeline().setTimeSignature(commandContext(), 1, 3, 4));
    const auto messageOf = [](const int tick) {
        const EditTempoDialog dialog(tick, 120.0);
        const auto *message = dialog.findChild<QLabel *>(QStringLiteral("DialogMessageLabel"));
        return message ? message->text() : QString();
    };
    // The anchor point reads 1-based, exactly like the transport timecode.
    QCOMPARE(messageOf(0), QStringLiteral("001:01:000"));
    // Bar 0 is 4/4 (1920 ticks) and bar 1 is 3/4 (1440 ticks), so this tick lands
    // on the second bar of the 3/4 section. A fixed 4/4 conversion would print
    // "003:01:096" here.
    QCOMPARE(messageOf(3936), QStringLiteral("003:02:096"));
}

void ApplicationGuiTests::timeAxisLaneStepKeepsLabelsApartAcrossTempos() {
    // A slow run, then a faster stretch whose first division lands 40px after the
    // last slow label: that division keeps its tick line and gives up its label,
    // because two labels that close would overlap.
    Timeline timeline;
    timeline.setTempos({
        {0,    60.0 },
        {1680, 200.0}
    });
    TimeAxisLaneView lane;
    lane.setTimeline(timeline);
    lane.resize(800, 28);
    lane.setTimeRange(0, 4800);

    const auto divisions = lane.divisionsInView();
    QVERIFY(!divisions.isEmpty());

    auto labels = 0;
    auto dropped = 0;
    auto lastLabelX = 0.0;
    for (const auto &division : divisions) {
        if (!division.labeled) {
            ++dropped;
            continue;
        }
        if (labels++ > 0) {
            // 64px is the documented minimum spacing between two labels.
            QVERIFY2(division.x - lastLabelX >= 64.0 - 0.5,
                     qPrintable(QStringLiteral("labels %1px apart at %2ms")
                                    .arg(division.x - lastLabelX)
                                    .arg(division.milliseconds)));
        }
        lastLabelX = division.x;
    }
    QVERIFY(labels > 3);
    QCOMPARE(dropped, 1);
}

void ApplicationGuiTests::timeAxisLaneDividesEachTempoSegmentAtItsOwnStep() {
    // A slow run followed by a long fast stretch. Dividing the whole window at the
    // slow region's step would leave the fast labels 320px apart, so each
    // constant-tempo run picks the step its own density allows.
    Timeline timeline;
    timeline.setTempos({
        {0,    60.0 },
        {1920, 240.0}
    });
    TimeAxisLaneView lane;
    lane.setTimeline(timeline);
    lane.resize(800, 28);
    lane.setTimeRange(0, 9600);

    const auto seamMs = timeline.tickToMs(1920.0);
    auto slowStep = 0.0;
    auto fastStep = 0.0;
    const auto divisions = lane.divisionsInView();
    for (qsizetype index = 1; index < divisions.size(); ++index) {
        const auto &previous = divisions.at(index - 1);
        const auto &current = divisions.at(index);
        if (!previous.labeled || !current.labeled)
            continue;
        if (current.milliseconds <= seamMs)
            slowStep = current.milliseconds - previous.milliseconds;
        else if (previous.milliseconds >= seamMs)
            fastStep = current.milliseconds - previous.milliseconds;
    }
    QVERIFY2(slowStep > 0, "the slow region has no labeled pair");
    QVERIFY2(fastStep > 0, "the fast region has no labeled pair");
    // The step follows the local tempo, so the fast region divides finer than the
    // slow one instead of inheriting its step.
    QVERIFY2(fastStep < slowStep,
             qPrintable(QStringLiteral("fast step %1ms is not finer than slow step %2ms")
                            .arg(fastStep)
                            .arg(slowStep)));

    // Pixel densities derived from the geometry alone: each region maps linearly
    // onto its share of the row width.
    const auto slowPixelsPerMs =
        lane.width() * 1920.0 / 9600.0 / (seamMs - timeline.tickToMs(0.0));
    const auto fastPixelsPerMs = lane.width() * (9600.0 - 1920.0) / 9600.0 /
                                 (timeline.tickToMs(9600.0) - seamMs);
    QVERIFY2(slowStep * slowPixelsPerMs >= 64.0 - 0.5,
             qPrintable(QStringLiteral("slow step %1ms leaves %2px between labels")
                            .arg(slowStep)
                            .arg(slowStep * slowPixelsPerMs)));
    QVERIFY2(fastStep * fastPixelsPerMs >= 64.0 - 0.5,
             qPrintable(QStringLiteral("fast step %1ms leaves %2px between labels")
                            .arg(fastStep)
                            .arg(fastStep * fastPixelsPerMs)));
    // The finest step that fits, not merely a finer one: the fast labels stay
    // within twice the minimum spacing instead of drifting far apart.
    QVERIFY2(fastStep * fastPixelsPerMs <= 128.0,
             qPrintable(QStringLiteral("fast step %1ms leaves %2px between labels")
                            .arg(fastStep)
                            .arg(fastStep * fastPixelsPerMs)));
}
