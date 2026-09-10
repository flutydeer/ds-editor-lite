#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoKeyboardView.h"

#include <lite/History/HistoryManager.h>
#include <TalcsMidi/MidiMessageIntegrator.h>
#include <TalcsMidi/MidiMessageListener.h>

#include <QApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QThread>
#include <QtTest/QTest>

namespace {
    struct NoteEvent {
        int key;
        bool on;
        bool operator==(const NoteEvent &) const = default;
    };

    class KeyboardMidiReceipt final : public talcs::MidiMessageListener {
    public:
        QList<NoteEvent> events;

    protected:
        bool processMessage(const talcs::MidiMessage &message) override {
            // External-device callbacks can arrive on other threads during a local test.
            if (QThread::currentThread() == inputThread && message.isNoteOnOrOff())
                events.append({message.getNoteNumber(), message.isNoteOn()});
            return false;
        }

    private:
        QThread *const inputThread = QThread::currentThread();
    };
}

void ApplicationGuiTests::pianoKeyboardGlissandoAndHideReleasePressedNotes() {
    auto *integrator = AudioSystem::midiSystem()->integrator();
    QVERIFY(integrator);
    KeyboardMidiReceipt receipt;
    integrator->flush();
    integrator->addFilter(&receipt);
    const auto originalCursor = QCursor::pos();
    PianoKeyboardView keyboard;
    const auto cleanup = qScopeGuard([&] {
        keyboard.hide();
        QTest::mouseRelease(&keyboard, Qt::LeftButton);
        integrator->removeFilter(&receipt);
        integrator->flush();
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
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QVERIFY(!historyManager->canUndo());
}
