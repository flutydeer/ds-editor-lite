#include "tst_application_gui.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Modules/Audio/AudioSystem.h"
#include "Modules/Audio/subsystem/MidiSystem.h"
#include "Modules/Audio/utils/MidiInputNoteMonitor.h"
#include "UI/Controls/HorizontalPianoKeyboardView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoKeyboardView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PhonemeView.h"
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <lite/History/HistoryManager.h>
#include <TalcsMidi/MidiMessageIntegrator.h>
#include <TalcsMidi/MidiMessageListener.h>

#include <QApplication>
#include <QCursor>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
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
    const auto before = context->m_coreRuntime->documentVersion();
    QEvent baselineLeave(QEvent::Leave);
    QApplication::sendEvent(&keyboard, &baselineLeave);
    const auto idleKeys = keyboard.grab().toImage();
    QVERIFY(!idleKeys.isNull());
    QTest::mouseMove(keyboard.windowHandle(), whiteC);
    const auto whiteHover = keyboard.grab().toImage().pixelColor(whiteC);
    QVERIFY(whiteHover != idleKeys.pixelColor(whiteC));
    QVERIFY(receipt.events.isEmpty());
    QTest::mouseMove(keyboard.windowHandle(), blackCSharp);
    const auto blackHover = keyboard.grab().toImage().pixelColor(blackCSharp);
    QVERIFY(blackHover != idleKeys.pixelColor(blackCSharp));
    QCOMPARE(keyboard.grab().toImage().pixelColor(whiteC), idleKeys.pixelColor(whiteC));
    QVERIFY(receipt.events.isEmpty());
    QApplication::sendEvent(&keyboard, &baselineLeave);
    QCOMPARE(keyboard.grab().toImage(), idleKeys);
    QCursor::setPos(keyboard.mapToGlobal(whiteC));
    QCoreApplication::processEvents();
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
    const auto whitePressed = keyboard.grab().toImage().pixelColor(whiteC);
    QVERIFY(whitePressed != idleKeys.pixelColor(whiteC));
    QVERIFY(whitePressed != whiteHover);
    moveTo(blackCSharp);
    const QList<NoteEvent> crossedBlackKey{
        {60, true },
        {60, false},
        {61, true }
    };
    QCOMPARE(receipt.events, crossedBlackKey);
    const auto blackPressed = keyboard.grab().toImage().pixelColor(blackCSharp);
    QVERIFY(blackPressed != idleKeys.pixelColor(blackCSharp));
    QVERIFY(blackPressed != blackHover);
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
    QCOMPARE(keyboard.grab().toImage(), idleKeys);
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

void ApplicationGuiTests::horizontalPreviewKeyboardMapsKeysAndReleasesOnHide() {
    // C2 - C6: 29 white keys of 20px each, laid out like a real piano.
    HorizontalPianoKeyboardView keyboard;
    QCOMPARE(keyboard.sizeHint(), QSize(580, 64));
    QCOMPARE(keyboard.size(), QSize(580, 64));
    QSignalSpy noteOn(&keyboard, &HorizontalPianoKeyboardView::noteOn);
    QSignalSpy noteOff(&keyboard, &HorizontalPianoKeyboardView::noteOff);
    keyboard.show();
    keyboard.activateWindow();
    QTRY_VERIFY(keyboard.isVisible());
    const auto cleanup = qScopeGuard([&] {
        keyboard.hide();
        QTest::mouseRelease(&keyboard, Qt::LeftButton);
    });

    // C4 is white key 14, D4 the next one, and C#4 the black key straddling
    // their shared boundary at x = 300.
    constexpr int keyWidth = 20;
    const auto whiteKeyCenter = [](const int whiteIndex) {
        return QPoint(whiteIndex * keyWidth + keyWidth / 2, 48);
    };
    const auto c4 = whiteKeyCenter(14);
    const auto d4 = whiteKeyCenter(15);
    const auto cSharp4 = QPoint(15 * keyWidth, 20);
    const auto d4UnderBlackKey = QPoint(15 * keyWidth, 48);

    QTest::mouseClick(&keyboard, Qt::LeftButton, Qt::NoModifier, c4);
    QCOMPARE(noteOn.count(), 1);
    QCOMPARE(noteOn.last().at(0).toInt(), 60);
    QCOMPARE(noteOff.count(), 1);
    QCOMPARE(noteOff.last().at(0).toInt(), 60);

    QTest::mouseClick(&keyboard, Qt::LeftButton, Qt::NoModifier, cSharp4);
    QCOMPARE(noteOn.count(), 2);
    QCOMPARE(noteOn.last().at(0).toInt(), 61);
    QCOMPARE(noteOff.count(), 2);
    QCOMPARE(noteOff.last().at(0).toInt(), 61);

    // Below the black key the same x belongs to the white key underneath.
    QTest::mouseClick(&keyboard, Qt::LeftButton, Qt::NoModifier, d4UnderBlackKey);
    QCOMPARE(noteOn.last().at(0).toInt(), 62);

    noteOn.clear();
    noteOff.clear();
    const auto moveTo = [&](const QPoint &position) {
        QMouseEvent move(QEvent::MouseMove, QPointF(position),
                         QPointF(keyboard.mapToGlobal(position)), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(&keyboard, &move);
    };
    QTest::mousePress(&keyboard, Qt::LeftButton, Qt::NoModifier, c4);
    moveTo(cSharp4);
    moveTo(d4);
    QTest::mouseRelease(&keyboard, Qt::LeftButton, Qt::NoModifier, d4);
    QList<int> played;
    for (const auto &arguments : noteOn)
        played.append(arguments.at(0).toInt());
    QCOMPARE(played, (QList<int>{60, 61, 62}));
    QList<int> released;
    for (const auto &arguments : noteOff)
        released.append(arguments.at(0).toInt());
    QCOMPARE(released, (QList<int>{60, 61, 62}));

    noteOn.clear();
    noteOff.clear();
    QTest::mousePress(&keyboard, Qt::LeftButton, Qt::NoModifier, cSharp4);
    QCOMPARE(noteOn.count(), 1);
    keyboard.hide();
    QCOMPARE(noteOff.count(), 1);
    QCOMPARE(noteOff.last().at(0).toInt(), 61);
    keyboard.show();
    QTRY_VERIFY(keyboard.isVisible());

    // Painting order: a white-key highlight must not bleed over the half of the
    // black key that straddles it, and black keys have square tops because the
    // bed is cut off at the widget's top edge.
    const auto grab = [&] { return keyboard.grab().toImage(); };
    const auto overlap = QPoint(15 * keyWidth - 3, 10);
    const auto underBlackKey = QPoint(15 * keyWidth - 3, 48);
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(&keyboard, &leave);
    const auto idle = grab();
    QMouseEvent hover(QEvent::MouseMove, QPointF(c4), QPointF(keyboard.mapToGlobal(c4)),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&keyboard, &hover);
    const auto hovered = grab();
    QCOMPARE(hovered.pixelColor(overlap), idle.pixelColor(overlap));
    QVERIFY(hovered.pixelColor(underBlackKey) != idle.pixelColor(underBlackKey));
    const auto distance = [](const QColor &lhs, const QColor &rhs) {
        return qAbs(lhs.red() - rhs.red()) + qAbs(lhs.green() - rhs.green()) +
               qAbs(lhs.blue() - rhs.blue());
    };
    // Just below the frame line and inside the old corner arc, so the probe only
    // reads as a key pixel when the top rounding is clipped away.
    const auto topCorner = idle.pixelColor(QPoint(15 * keyWidth - 6, 1));
    QVERIFY2(distance(topCorner, keyboard.blackKeyColor()) <
                 distance(topCorner, keyboard.whiteKeyColor()),
             qPrintable(QStringLiteral("black key top corner is %1").arg(topCorner.name())));
    // The bed's outer frame keeps it readable against the card surface.
    QCOMPARE(idle.pixelColor(QPoint(0, 30)), keyboard.dividerColor());
    QCOMPARE(idle.pixelColor(QPoint(keyboard.width() - 1, 30)), keyboard.dividerColor());

    // Keys held by an external note source paint like a press, in the same
    // z-order: the white key first, the black keys over it, and a white hold
    // must not bleed over the black key that straddles it.
    const auto idleKeys = grab();
    keyboard.setHeldKey(60, true);
    QVERIFY(keyboard.isKeyHeld(60));
    const auto heldWhite = grab();
    QVERIFY(heldWhite.pixelColor(underBlackKey) != idleKeys.pixelColor(underBlackKey));
    QCOMPARE(heldWhite.pixelColor(overlap), idleKeys.pixelColor(overlap));
    keyboard.setHeldKey(61, true);
    const auto heldBoth = grab();
    QVERIFY(heldBoth.pixelColor(overlap) != idleKeys.pixelColor(overlap));
    keyboard.clearHeldKeys();
    QVERIFY(!keyboard.isKeyHeld(60));
    QCOMPARE(grab(), idleKeys);
    QApplication::sendEvent(&keyboard, &leave);

    // Nothing listens for the wheel here, so it has to reach the scroll area.
    QWheelEvent wheel(QPointF(c4), QPointF(keyboard.mapToGlobal(c4)), {}, {0, 120},
                      Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QApplication::sendEvent(&keyboard, &wheel);
    QVERIFY(!wheel.isAccepted());
    QCOMPARE(noteOn.count(), 1);

}

void ApplicationGuiTests::midiInputNoteMonitorTracksHeldNotes() {
    talcs::MidiMessageIntegrator integrator;
    MidiInputNoteMonitor monitor;
    monitor.attachTo(&integrator);
    QSignalSpy pressed(&monitor, &MidiInputNoteMonitor::notePressed);
    QSignalSpy released(&monitor, &MidiInputNoteMonitor::noteReleased);
    QSignalSpy allReleased(&monitor, &MidiInputNoteMonitor::allNotesReleased);

    integrator.messageCallback(talcs::MidiMessage::noteOn(1, 60, 0.8f));
    integrator.messageCallback(talcs::MidiMessage::noteOff(1, 60));
    // A note on with velocity zero is the running-status form of a note off.
    integrator.messageCallback(talcs::MidiMessage::noteOn(1, 62, 0.0f));
    QCOMPARE(pressed.count(), 1);
    QCOMPARE(pressed.at(0).at(0).toInt(), 60);
    QCOMPARE(released.count(), 2);
    QCOMPARE(released.at(0).at(0).toInt(), 60);
    QCOMPARE(released.at(1).at(0).toInt(), 62);

    integrator.messageCallback(talcs::MidiMessage::allNotesOff(1));
    QCOMPARE(allReleased.count(), 1);
    integrator.messageCallback(talcs::MidiMessage::allSoundOff(1));
    QCOMPARE(allReleased.count(), 2);

    // Detaching stops the reports.
    monitor.attachTo(nullptr);
    integrator.messageCallback(talcs::MidiMessage::noteOn(1, 64, 0.8f));
    QCOMPARE(pressed.count(), 1);
}

void ApplicationGuiTests::pianoAuxiliaryViewsNavigateWithoutChangingTheDocument() {
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
    const auto beforeModel = TestSupport::projectSnapshot(*context->m_appModel);
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

    auto *phonemes = editor.findChild<PhonemeView *>();
    QVERIFY(phonemes && phonemes->isVisible());
    QVERIFY(editor.setTimeViewport(2400, 1.0));
    const auto beforePhonemeZoom = editor.viewState();
    const auto phonemePosition = phonemes->rect().center();
    const auto wheelOnPhonemes = [&](int delta, Qt::KeyboardModifiers modifiers) {
        QWheelEvent event(QPointF(phonemePosition), QPointF(phonemes->mapToGlobal(phonemePosition)),
                          {}, {0, delta}, Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(phonemes, &event);
        QVERIFY(event.isAccepted());
    };
    wheelOnPhonemes(120, Qt::ControlModifier);
    QTRY_VERIFY(editor.viewState().horizontalScale > beforePhonemeZoom.horizontalScale);
    QCOMPARE(editor.viewState().centerKeyIndex, beforePhonemeZoom.centerKeyIndex);
    QCOMPARE(editor.viewState().verticalScale, beforePhonemeZoom.verticalScale);
    const auto beforePhonemeScroll = editor.viewState();
    wheelOnPhonemes(-120, Qt::ShiftModifier);
    QTRY_VERIFY(editor.viewState().centerTick > beforePhonemeScroll.centerTick);
    QCOMPARE(editor.viewState().horizontalScale, beforePhonemeScroll.horizontalScale);
    QVERIFY(receipt.events.isEmpty());
    QCOMPARE(context->m_coreRuntime->documentVersion(), before);
    QCOMPARE(TestSupport::projectSnapshot(*context->m_appModel), beforeModel);
    QVERIFY(!historyManager->canUndo());
}

