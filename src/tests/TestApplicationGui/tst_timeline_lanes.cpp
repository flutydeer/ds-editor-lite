#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "UI/Dialogs/Timeline/EditTempoDialog.h"
#include "UI/Dialogs/Timeline/EditTimeSignatureDialog.h"
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
