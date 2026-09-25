#include "UI/Views/Common/EditorPenStroke.h"
#include "UI/Views/Common/EditorPenTarget.h"

#include <QTest>

// The pen layer's decidable half, without a tablet: stroke boundaries, what
// locks at press time, how a deferred side-button stroke resolves, and every
// row of the per-tool strategy tables. The hover shim needs Windows messages
// and a real pen, so it is verified on hardware instead.

using Stroke = EditorPenStroke;
using Type = EditorPenStroke::Intent::Type;
using Input = EditorPenStroke::Input;
using Sample = EditorPenStroke::Sample;
using Eraser = EditorPenEraser;
// The tool enumerators live in the global namespace alias, exactly as the views
// read them.
using namespace EditorViewGlobal;

namespace {
    Sample penSample(const QPointF &position, const double pressure = 0.5,
                     const Qt::MouseButtons buttons = Qt::LeftButton,
                     const QPointingDevice::PointerType pointerType =
                         QPointingDevice::PointerType::Pen) {
        Sample sample;
        sample.position = position;
        sample.pressure = pressure;
        sample.buttons = buttons;
        sample.pointerType = pointerType;
        return sample;
    }

    int indexOf(const Stroke::Intents &intents, const Type type) {
        for (qsizetype i = 0; i < intents.size(); ++i) {
            if (intents.at(i).type == type)
                return static_cast<int>(i);
        }
        return -1;
    }
}

class TestPenInput final : public QObject {
    Q_OBJECT

private slots:
    // --- The sample classification -------------------------------------------

    // Only the two inputs the platform maps wrongly are taken over. A plain
    // tip keeps going to Qt, whose own tablet-to-mouse synthesis reproduces it
    // exactly (double click included) and which is what the pen did before this
    // layer existed.
    void aPlainTipIsLeftToQt() {
        QVERIFY(!Stroke::needsTranslation(penSample({10, 10})));
    }

    // The eraser reports button=Left buttons=Left, so pointerType is the only
    // thing that gives it away.
    void theEraserIsRecognizedByPointerType() {
        QVERIFY(Stroke::needsTranslation(
            penSample({10, 10}, 0.5, Qt::LeftButton, QPointingDevice::PointerType::Eraser)));
    }

    // The barrel button is Right on Windows, Middle/Right on X11 and tracked
    // from NSEvent on macOS, so the test is "anything but the tip", never a
    // hardcoded button.
    void anyNonTipButtonCountsAsTheSideButton() {
        QVERIFY(Stroke::hasSideButton(Qt::RightButton));
        QVERIFY(Stroke::hasSideButton(Qt::MiddleButton));
        QVERIFY(Stroke::hasSideButton(Qt::LeftButton | Qt::RightButton));
        QVERIFY(!Stroke::hasSideButton(Qt::LeftButton));
        QVERIFY(!Stroke::hasSideButton(Qt::NoButton));
        QVERIFY(Stroke::needsTranslation(penSample({10, 10}, 0.5, Qt::RightButton)));
    }

    // --- Tip strokes ---------------------------------------------------------

    void aTipStrokeIsAPlainLeftStream() {
        Stroke stroke;
        auto intents = stroke.pressed(penSample({100, 100}), true);
        QCOMPARE(static_cast<int>(intents.size()), 1);
        QCOMPARE(intents.at(0).type, Type::Begin);
        QCOMPARE(intents.at(0).position, QPointF(100, 100));
        QVERIFY(!intents.at(0).erase);
        QCOMPARE(stroke.input(), Input::Tip);
        QVERIFY(!stroke.erases());

        intents = stroke.feed(Stroke::Report::Move, penSample({120, 100}));
        QCOMPARE(static_cast<int>(intents.size()), 1);
        QCOMPARE(intents.at(0).type, Type::Move);
        QVERIFY(!intents.at(0).erase);

        intents = stroke.feed(Stroke::Report::Release, penSample({120, 100}, 0.0));
        QCOMPARE(static_cast<int>(intents.size()), 1);
        QCOMPARE(intents.at(0).type, Type::End);
        QVERIFY(!intents.at(0).erase);
        QCOMPARE(stroke.phase(), Stroke::Phase::Idle);
    }

    // --- Eraser --------------------------------------------------------------

    // The eraser becomes a left-button stream carrying the erase intent: the
    // view, not the pen layer, decides what erasing means for the armed tool,
    // and it does that by routing a plain left stream down its erase path.
    void theEraserBecomesALeftStreamWithTheEraseIntent() {
        Stroke stroke;
        const auto intents = stroke.pressed(
            penSample({100, 100}, 0.5, Qt::LeftButton, QPointingDevice::PointerType::Eraser), true);
        QCOMPARE(static_cast<int>(intents.size()), 1);
        QCOMPARE(intents.at(0).type, Type::Begin);
        QVERIFY(intents.at(0).erase);
        QCOMPARE(stroke.input(), Input::Eraser);
        QVERIFY(stroke.erases());

        const auto moved = stroke.feed(Stroke::Report::Move, penSample({140, 100}, 0.5, Qt::LeftButton,
                                                  QPointingDevice::PointerType::Eraser));
        QCOMPARE(static_cast<int>(moved.size()), 1);
        QCOMPARE(moved.at(0).type, Type::Move);
        QVERIFY(moved.at(0).erase);

        const auto released = stroke.feed(Stroke::Report::Release, penSample({140, 100}, 0.0, Qt::LeftButton,
                                                        QPointingDevice::PointerType::Eraser));
        QCOMPARE(static_cast<int>(released.size()), 1);
        QCOMPARE(released.at(0).type, Type::End);
        QVERIFY(released.at(0).erase);
    }

    // Under a tool that has nothing to erase the whole stroke is claimed and
    // produces nothing: no press ever reaches the interaction layer, so the
    // eraser cannot perform the tool's normal action by accident.
    void anEraserStrokeUnderAnUnsupportedToolProducesNothing() {
        Stroke stroke;
        QVERIFY(stroke.pressed(penSample({100, 100}, 0.5, Qt::LeftButton,
                                         QPointingDevice::PointerType::Eraser),
                               false)
                    .isEmpty());
        QCOMPARE(stroke.phase(), Stroke::Phase::Stroke);
        QVERIFY(!stroke.erases());
        QVERIFY(stroke.feed(Stroke::Report::Move, penSample({140, 100}, 0.5, Qt::LeftButton,
                                       QPointingDevice::PointerType::Eraser))
                    .isEmpty());
        QVERIFY(stroke.feed(Stroke::Report::Release, penSample({140, 100}, 0.0, Qt::LeftButton,
                                          QPointingDevice::PointerType::Eraser))
                    .isEmpty());
        QCOMPARE(stroke.phase(), Stroke::Phase::Idle);
    }

    // --- Stroke boundaries come from pressure --------------------------------

    // The platform emits extra, contradictory press/release pairs around a
    // mid-stroke barrel change, so the boundary has to be the contact state.
    // A frame with no contact ends the stroke; nothing else does.
    void theStrokeEndsOnTheReleaseFrameNotOnTheContactFlag() {
        Stroke stroke;
        const auto eraser = [](const QPointF &position, const double pressure) {
            return penSample(position, pressure, Qt::LeftButton,
                             QPointingDevice::PointerType::Eraser);
        };
        stroke.pressed(eraser({100, 100}, 0.5), true);

        // A move that still reports contact keeps the stroke alive.
        QCOMPARE(stroke.feed(Stroke::Report::Move, eraser({110, 100}, 0.02)).size(), 1);
        QCOMPARE(stroke.phase(), Stroke::Phase::Stroke);

        // The lift, whether it arrives as a release or as a bare move: no
        // contact means the stroke is over.
        const auto released = stroke.feed(Stroke::Report::Move, eraser({120, 100}, 0.0));
        QCOMPARE(static_cast<int>(released.size()), 1);
        QCOMPARE(released.at(0).type, Type::End);
        QCOMPARE(stroke.phase(), Stroke::Phase::Idle);
        // And the release frame that follows it produces nothing.
        QVERIFY(stroke.feed(Stroke::Report::Release, eraser({120, 100}, 0.0)).isEmpty());
    }

    // A barrel press and release in the middle of a claimed stroke are the
    // platform talking about the button, not about the tip. Believing the event
    // type instead of the contact state is what makes this go wrong: the press
    // restarts the stroke (a second press reaches the tool), and the release
    // cuts it short (the erase stops while the pen is still on the glass).
    void midStrokeBarrelNoiseNeitherRestartsNorEndsTheStroke() {
        Stroke stroke;
        stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), true);
        QCOMPARE(stroke.input(), Input::SideButton);

        // Past the slop the stroke commits: press at the origin, then the
        // sample that crossed the threshold.
        const auto begun =
            stroke.feed(Stroke::Report::Move, penSample({140, 100}, 0.5, Qt::RightButton));
        QCOMPARE(static_cast<int>(begun.size()), 2);
        QCOMPARE(begun.at(0).type, Type::Begin);
        QCOMPARE(begun.at(0).position, QPointF(100, 100));

        // Barrel released while the tip is still down — measured as
        // `release button=Left buttons=Left`, a release that still reports the
        // button as held. Noise, in both readings.
        QVERIFY(stroke.feed(Stroke::Report::Release, penSample({142, 100}, 0.5, Qt::LeftButton))
                    .isEmpty());
        QCOMPARE(stroke.phase(), Stroke::Phase::Stroke);
        // And the barrel going back down is the matching `press button=Left
        // buttons=Right`.
        QVERIFY(stroke.feed(Stroke::Report::Press, penSample({144, 100}, 0.5, Qt::RightButton))
                    .isEmpty());
        QCOMPARE(stroke.phase(), Stroke::Phase::Stroke);

        // The stroke erases on, with no second Begin anywhere.
        const auto more =
            stroke.feed(Stroke::Report::Move, penSample({146, 100}, 0.5, Qt::LeftButton));
        QCOMPARE(static_cast<int>(more.size()), 1);
        QCOMPARE(more.at(0).type, Type::Move);
        QVERIFY(more.at(0).erase);

        // Only the lift ends it.
        const auto ended =
            stroke.feed(Stroke::Report::Release, penSample({146, 100}, 0.0, Qt::LeftButton));
        QCOMPARE(static_cast<int>(ended.size()), 1);
        QCOMPARE(ended.at(0).type, Type::End);
        QVERIFY(ended.at(0).erase);
        QCOMPARE(stroke.phase(), Stroke::Phase::Idle);
    }

    // --- Side button ---------------------------------------------------------

    // Locked at press: the stroke is a side-button stroke from beginning to
    // end, whatever the platform reports about the barrel afterwards.
    void theSideButtonIsLockedWhenThePenGoesDown() {
        Stroke stroke;
        QVERIFY(stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), true).isEmpty());
        QCOMPARE(stroke.input(), Input::SideButton);
        // Barrel released halfway through: the stroke stays what it was.
        QVERIFY(stroke.feed(Stroke::Report::Move, penSample({104, 100}, 0.5, Qt::LeftButton)).isEmpty());
        QCOMPARE(stroke.input(), Input::SideButton);
    }

    // A barrel press halfway through a tip stroke does not change that stroke,
    // and does not open a menu either.
    void aBarrelPressMidStrokeDoesNotChangeTheInput() {
        Stroke stroke;
        stroke.pressed(penSample({100, 100}, 0.5), true);
        QCOMPARE(stroke.input(), Input::Tip);
        const auto moved = stroke.feed(Stroke::Report::Move, penSample({110, 100}, 0.5, Qt::LeftButton | Qt::RightButton));
        QCOMPARE(static_cast<int>(moved.size()), 1);
        QCOMPARE(moved.at(0).type, Type::Move);
        QVERIFY(!moved.at(0).erase);
        const auto released = stroke.feed(Stroke::Report::Release, penSample({110, 100}, 0.0, Qt::LeftButton));
        QCOMPARE(static_cast<int>(released.size()), 1);
        QCOMPARE(released.at(0).type, Type::End);
        QVERIFY(!released.at(0).erase);
    }

    // Inside the slop nothing is pressed yet: the stroke may still turn out to
    // be a click, and a click must not erase.
    void aSideButtonPressInsideTheSlopPressesNothing() {
        Stroke stroke;
        QVERIFY(stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), true).isEmpty());
        QVERIFY(stroke.feed(Stroke::Report::Move, penSample({105, 103}, 0.5, Qt::RightButton)).isEmpty());
        QCOMPARE(stroke.phase(), Stroke::Phase::Stroke);
    }

    // Past the slop it is an erase stroke, and it presses at the point the pen
    // was put down, so erasing starts where the pen landed rather than where it
    // crossed the threshold.
    void aSideButtonDragErasesFromTheOriginalPosition() {
        Stroke stroke;
        stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), true);
        const auto intents = stroke.feed(Stroke::Report::Move, penSample({140, 100}, 0.5, Qt::RightButton));
        QCOMPARE(static_cast<int>(intents.size()), 2);
        QCOMPARE(intents.at(0).type, Type::Begin);
        QCOMPARE(intents.at(0).position, QPointF(100, 100));
        QVERIFY(intents.at(0).erase);
        QCOMPARE(intents.at(1).type, Type::Move);
        QCOMPARE(intents.at(1).position, QPointF(140, 100));
        QVERIFY(intents.at(1).erase);
        QCOMPARE(stroke.origin(), QPointF(100, 100));

        const auto released = stroke.feed(Stroke::Report::Release, penSample({160, 100}, 0.0, Qt::RightButton));
        QCOMPARE(static_cast<int>(released.size()), 1);
        QCOMPARE(released.at(0).type, Type::End);
        QVERIFY(released.at(0).erase);
    }

    // The slop is a radius, not a per-axis threshold.
    void theSideButtonSlopIsARadius() {
        Stroke stroke;
        stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), true);
        // 8 px on each axis: 11.3 px of travel, still inside the 12 px slop.
        QVERIFY(stroke.feed(Stroke::Report::Move, penSample({108, 108}, 0.5, Qt::RightButton)).isEmpty());
        QCOMPARE(stroke.phase(), Stroke::Phase::Stroke);
    }

    // A side-button stroke that never travelled is a click: it asks for the
    // context menu at the point the pen went down, and never erases.
    void aSideButtonClickAsksForTheContextMenu() {
        Stroke stroke;
        stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), true);
        stroke.feed(Stroke::Report::Move, penSample({104, 102}, 0.5, Qt::RightButton));
        const auto released = stroke.feed(Stroke::Report::Release, penSample({104, 102}, 0.0, Qt::RightButton));
        QCOMPARE(static_cast<int>(released.size()), 1);
        QCOMPARE(released.at(0).type, Type::ContextMenu);
        QCOMPARE(released.at(0).position, QPointF(100, 100));
        QVERIFY(!released.at(0).erase);
        QCOMPARE(stroke.phase(), Stroke::Phase::Idle);
    }

    // The menu is a tool-independent request, so a tool with nothing to erase
    // still gets one; what it does not get is an erase stroke.
    void aSideButtonClickStillOpensTheMenuWhereErasingIsUnsupported() {
        Stroke stroke;
        stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), false);
        const auto released = stroke.feed(Stroke::Report::Release, penSample({100, 100}, 0.0, Qt::RightButton));
        QCOMPARE(static_cast<int>(released.size()), 1);
        QCOMPARE(released.at(0).type, Type::ContextMenu);
    }

    // Once a side-button stroke has travelled, it is an erase attempt: under a
    // tool that cannot erase it stays swallowed, menu included.
    void aSwallowedSideButtonDragDoesNotFallBackToTheMenu() {
        Stroke stroke;
        stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), false);
        QVERIFY(stroke.feed(Stroke::Report::Move, penSample({140, 100}, 0.5, Qt::RightButton)).isEmpty());
        QVERIFY(stroke.feed(Stroke::Report::Release, penSample({140, 100}, 0.0, Qt::RightButton)).isEmpty());
    }

    // --- Cancel --------------------------------------------------------------

    // An aborted stroke is released, never turned into a menu.
    void aCancelledStrokeIsReleasedAndNotTurnedIntoAMenu() {
        Stroke stroke;
        stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), true);
        QVERIFY(stroke.cancelled().isEmpty());

        stroke.pressed(penSample({100, 100}, 0.5, Qt::RightButton), true);
        stroke.feed(Stroke::Report::Move, penSample({140, 100}, 0.5, Qt::RightButton));
        const auto intents = stroke.cancelled();
        QCOMPARE(static_cast<int>(intents.size()), 1);
        QCOMPARE(intents.at(0).type, Type::End);
        QVERIFY(intents.at(0).erase);
    }

    // --- Strategy tables -----------------------------------------------------

    void thePianoRollTableHasNoGaps() {
        QCOMPARE(EditorPenPolicy::pianoRoll(Select), Eraser::EraseNote);
        QCOMPARE(EditorPenPolicy::pianoRoll(EraseNote), Eraser::EraseNote);
        QCOMPARE(EditorPenPolicy::pianoRoll(DrawNote), Eraser::EraseNote);

        QCOMPARE(EditorPenPolicy::pianoRoll(DrawPitch), Eraser::EraseParam);
        QCOMPARE(EditorPenPolicy::pianoRoll(ErasePitch), Eraser::EraseParam);
        QCOMPARE(EditorPenPolicy::pianoRoll(TracePitch), Eraser::EraseParam);

        QCOMPARE(EditorPenPolicy::pianoRoll(SplitNote), Eraser::Unsupported);
        QCOMPARE(EditorPenPolicy::pianoRoll(EditPitchAnchor), Eraser::Unsupported);
        QCOMPARE(EditorPenPolicy::pianoRoll(ModulatePitch), Eraser::Unsupported);
        QCOMPARE(EditorPenPolicy::pianoRoll(IntervalSelect), Eraser::Unsupported);
    }

    void theParameterEditorTableHasNoGaps() {
        using Mode = EditorViewGlobal::ParameterEditMode;
        QCOMPARE(EditorPenPolicy::parameterEditor(Mode::Draw), Eraser::EraseParam);
        QCOMPARE(EditorPenPolicy::parameterEditor(Mode::Erase), Eraser::EraseParam);
        QCOMPARE(EditorPenPolicy::parameterEditor(Mode::Trace), Eraser::EraseParam);
        QCOMPARE(EditorPenPolicy::parameterEditor(Mode::Shape), Eraser::Unsupported);
        QCOMPARE(EditorPenPolicy::parameterEditor(Mode::Scale), Eraser::Unsupported);
        QCOMPARE(EditorPenPolicy::parameterEditor(Mode::Anchor), Eraser::Unsupported);
    }

    // The arrangement canvas has no tool of its own and nothing to erase.
    void theArrangementCanvasRespondsToNothing() {
        QCOMPARE(EditorPenPolicy::arrangement(), Eraser::Unsupported);
    }
};

QTEST_APPLESS_MAIN(TestPenInput)

#include "main.moc"
