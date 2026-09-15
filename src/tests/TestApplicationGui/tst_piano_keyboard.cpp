#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoKeyboardView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <lite/History/HistoryManager.h>
#include <TalcsMidi/MidiMessageIntegrator.h>
#include <TalcsMidi/MidiMessageListener.h>

#include <QApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QThread>
#include <QWheelEvent>
#include <QtTest/QTest>

namespace {
    struct NoteEvent {
        int key;
        bool on;
        bool operator==(const NoteEvent &) const = default;
    };

    class KeyboardMidiReceipt final : public talcs::MidiMessageListener {
    public:
        explicit KeyboardMidiReceipt(talcs::MidiMessageIntegrator &integrator)
            : integrator(integrator) {
            integrator.flush();
            integrator.addFilter(this);
        }

        ~KeyboardMidiReceipt() override {
            integrator.removeFilter(this);
            integrator.flush();
        }

        QList<NoteEvent> events;

    protected:
        bool processMessage(const talcs::MidiMessage &message) override {
            // External-device callbacks can arrive on other threads during a local test.
            if (QThread::currentThread() == inputThread && message.isNoteOnOrOff())
                events.append({message.getNoteNumber(), message.isNoteOn()});
            return false;
        }

    private:
        talcs::MidiMessageIntegrator &integrator;
        QThread *const inputThread = QThread::currentThread();
    };
}

void ApplicationGuiTests::pianoKeyboardGlissandoAndHideReleasePressedNotes() {
    auto *integrator = AudioSystem::midiSystem()->integrator();
    QVERIFY(integrator);
    KeyboardMidiReceipt receipt(*integrator);
    const auto originalCursor = QCursor::pos();
    PianoKeyboardView keyboard;
    const auto cleanup = qScopeGuard([&] {
        keyboard.hide();
        QTest::mouseRelease(&keyboard, Qt::LeftButton);
        QCursor::setPos(originalCursor);
    });
    keyboard.setKeyRange(72, 60);
    keyboard.resize(keyboard.width(), 240);
    keyboard.show();
    keyboard.activateWindow();
    QTRY_VERIFY(keyboard.isVisible());
    const QPoint whiteC(qRound(keyboard.width() * 0.85), 230);
    const QPoint blackCSharp(qRound(keyboard.width() * 0.3), 230);
    const QPoint whiteD(whiteC.x(), 210);
    QCursor::setPos(keyboard.mapToGlobal(whiteC));
    QCoreApplication::processEvents();
    const auto before = context->m_coreRuntime->documentVersion();
    const auto moveTo = [&](const QPoint &position) {
        QMouseEvent move(QEvent::MouseMove, QPointF(position),
                         QPointF(keyboard.mapToGlobal(position)), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(&keyboard, &move);
    };
    QTest::mousePress(&keyboard, Qt::LeftButton, Qt::NoModifier, whiteC);
    QCOMPARE(receipt.events, (QList<NoteEvent>{
                                 {60, true}
    }));
    moveTo(blackCSharp);
    const QList<NoteEvent> crossedBlackKey{
        {60, true },
        {60, false},
        {61, true }
    };
    QCOMPARE(receipt.events, crossedBlackKey);
    moveTo(blackCSharp + QPoint(1, 0));
    QCOMPARE(receipt.events, crossedBlackKey);
    moveTo(whiteD);
    QTest::mouseRelease(&keyboard, Qt::LeftButton, Qt::NoModifier, whiteD);
    const QList<NoteEvent> releasedGlissando{
        {60, true },
        {60, false},
        {61, true },
        {61, false},
        {62, true },
        {62, false}
    };
    QCOMPARE(receipt.events, releasedGlissando);

    receipt.events.clear();
    QTest::mousePress(&keyboard, Qt::LeftButton, Qt::NoModifier, blackCSharp);
    QCOMPARE(receipt.events, (QList<NoteEvent>{
                                 {61, true}
    }));
    keyboard.hide();
    const QList<NoteEvent> hiddenWhilePressed{
        {61, true },
        {61, false}
    };
    QCOMPARE(receipt.events, hiddenWhilePressed);
    QTest::mouseRelease(&keyboard, Qt::LeftButton, Qt::NoModifier, blackCSharp);
    QCoreApplication::processEvents();
    QCOMPARE(receipt.events, hiddenWhilePressed);
    keyboard.show();
    keyboard.activateWindow();
    QTRY_VERIFY(keyboard.isVisible());
    QTest::mouseClick(&keyboard, Qt::LeftButton, Qt::NoModifier, whiteC);
    const QList<NoteEvent> playedAfterReopening{
        {61, true },
        {61, false},
        {60, true },
        {60, false}
    };
    QCOMPARE(receipt.events, playedAfterReopening);
    receipt.events.clear();
    QTest::mousePress(&keyboard, Qt::LeftButton, Qt::NoModifier, whiteC);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&keyboard, &leave);
    const QList<NoteEvent> releasedOnLeave{
        {60, true },
        {60, false}
    };
    QCOMPARE(receipt.events, releasedOnLeave);
    QTest::mouseRelease(&keyboard, Qt::LeftButton, Qt::NoModifier, whiteC);
    QCOMPARE(receipt.events, releasedOnLeave);

    QPoint forwardedDelta;
    connect(&keyboard, &PianoKeyboardView::wheelScroll, &keyboard,
            [&](const QWheelEvent *event) { forwardedDelta = event->angleDelta(); });
    QWheelEvent wheel(QPointF(whiteC), QPointF(keyboard.mapToGlobal(whiteC)), {}, {0, 120},
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&keyboard, &wheel);
    QCOMPARE(forwardedDelta, QPoint(0, 120));
    QVERIFY(wheel.isAccepted());
    QCOMPARE(receipt.events, releasedOnLeave);
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}

void ApplicationGuiTests::pianoKeyboardRangeAndScrollingFollowTheEditor() {
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    view->hide();
    auto *integrator = AudioSystem::midiSystem()->integrator();
    QVERIFY(integrator);
    KeyboardMidiReceipt receipt(*integrator);
    PianoRollView editor;
    editor.setDataContext(singingClip);
    const auto detach = qScopeGuard([&] { editor.setDataContext(nullptr); });
    editor.resize(800, 420);
    editor.show();
    editor.activateWindow();
    QTRY_VERIFY(editor.isActiveWindow());
    auto *keyboard = editor.findChild<PianoKeyboardView *>();
    QVERIFY(keyboard);
    QTRY_VERIFY(keyboard->isVisible());
    const auto before = context->m_coreRuntime->documentVersion();
    const auto beforeModel = context->m_appModel->serialize();
    for (int key : {60, 72}) {
        QVERIFY(editor.setPitchViewport(key, 1.0));
        QCoreApplication::processEvents();
        const QPoint position(qRound(keyboard->width() * 0.85), keyboard->height() / 2);
        QTest::mouseClick(keyboard, Qt::LeftButton, Qt::NoModifier, position);
        QCOMPARE(receipt.events, (QList<NoteEvent>{
                                     {key, true },
                                     {key, false}
        }));
        receipt.events.clear();
    }
    const auto beforeScroll = editor.viewState().centerKeyIndex;
    const auto position = keyboard->rect().center();
    QWheelEvent wheel(QPointF(position), QPointF(keyboard->mapToGlobal(position)), {}, {0, -120},
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(keyboard, &wheel);
    QTRY_VERIFY(editor.viewState().centerKeyIndex != beforeScroll);
    QVERIFY(wheel.isAccepted());
    QVERIFY(receipt.events.isEmpty());
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QCOMPARE(context->m_appModel->serialize(), beforeModel);
    QVERIFY(!historyManager->canUndo());
}
