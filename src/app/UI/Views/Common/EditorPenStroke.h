#ifndef EDITORPENSTROKE_H
#define EDITORPENSTROKE_H

#include <QPointF>
#include <QPointingDevice>
#include <QVarLengthArray>
#include <Qt>

// Pure-logic single-stroke tracking for a stylus. Like EditorTouchGesture this
// is deliberately free of any widget or Qt event dependency: callers feed
// widget-local samples and get back an ordered list of intents to translate
// into synthetic mouse events. Keeping it headless is what makes it unit
// testable without a tablet (see src/tests/TestPenInput).
//
// This is not a gesture recognizer. A pen has a single contact point, so the
// only arbitration needed is which of the three physical inputs started the
// stroke, and when the stroke is over:
//
//   tip          -> an ordinary left-button stroke, identical to a mouse
//   eraser       -> an ordinary left-button stroke carrying EditorPointer's
//                   erase intent, so the view routes it to its erase path
//   side button  -> deferred: travel past the slop turns it into an eraser
//                   stroke (pressed at the original position), staying put
//                   turns it into a context menu click
//
// Two platform facts shape the state tracking, both measured on Windows with
// the TouchProbe (see docs/design/touch-and-pen-input-design.md):
//
//   1. Stroke boundaries come from the contact state (pressure), never from
//      TabletPress/TabletRelease. Pressing or releasing the barrel button in
//      the middle of a stroke makes the platform emit extra, self-contradictory
//      press/release pairs that say nothing about the tip.
//   2. The eraser is only distinguishable by pointerType(); its press reports
//      button=Left buttons=Left, exactly like the tip. And while the barrel
//      button is held the button set carries no Left at all, so the contact
//      state can never be read from `buttons`.
class EditorPenStroke {
public:
    // Which physical input started the stroke. Locked when the pen goes down
    // and never reconsidered: a barrel press halfway through a tip stroke does
    // not turn that stroke into an eraser, and does not open a menu either.
    enum class Input { Tip, Eraser, SideButton };

    struct Config {
        // Travel needed before a side-button press counts as an erase stroke
        // rather than a click. Matches the touch layer's long press slop so the
        // two input layers feel alike.
        double sideButtonSlopPx = 12.0;
    };

    // One platform report, already normalized by the caller.
    struct Sample {
        // Widget-local position.
        QPointF position;
        // > 0 means the tip is on the glass.
        double pressure = 0.0;
        Qt::MouseButtons buttons;
        QPointingDevice::PointerType pointerType = QPointingDevice::PointerType::Unknown;
    };

    // What the caller should do with the stroke so far.
    struct Intent {
        enum class Type {
            // Synthetic left-button press.
            Begin,
            // Synthetic left-button move.
            Move,
            // Synthetic left-button release.
            End,
            // The side button was clicked without travelling: synthesize the
            // right-button click and ask for the context menu here.
            ContextMenu,
        };

        Type type = Type::Begin;
        QPointF position;
        // This stroke erases. The caller brackets the Begin/End pair with
        // EditorPointer's erase intent, which is how a plain left-button
        // stream tells a view to erase instead of doing its normal job.
        bool erase = false;
    };

    using Intents = QVarLengthArray<Intent, 8>;

    // The three platform reports this layer reacts to. Deliberately not
    // QEvent::Type, so the tracker stays free of any event dependency and can
    // be driven from a unit test.
    enum class Report { Press, Move, Release };

    enum class Phase {
        Idle,
        // The stroke is claimed: every further sample belongs to the pen layer,
        // including the ones that produce nothing at all.
        Stroke,
    };

    EditorPenStroke();
    explicit EditorPenStroke(const Config &config);

    void setConfig(const Config &config);
    [[nodiscard]] const Config &config() const;

    // A button set that carries anything but the tip. The barrel button is
    // Right on Windows, Middle/Right on X11 and tracked from NSEvent on macOS,
    // so the test must never hardcode a button value.
    [[nodiscard]] static bool hasSideButton(Qt::MouseButtons buttons);
    // Should this sample be taken over from the platform at all? True for the
    // two inputs the platform maps wrongly (eraser, side button); a plain tip
    // is left to Qt, whose own tablet-to-mouse synthesis already reproduces it
    // exactly — including the double click, which is why the tip is never
    // claimed here: Qt only detects a double click for mouse presses it
    // dispatches itself (QGuiApplicationPrivate::processMouseEvent), and a
    // synthesised press sent from this layer would bypass it.
    [[nodiscard]] static bool needsTranslation(const Sample &sample);

    // `eraserSupported` is the armed tool's answer (EditorPenTarget): when the
    // tool has nothing an eraser could do, the stroke is claimed and swallowed.
    // Call once, on the press that claims the stroke.
    Intents pressed(const Sample &sample, bool eraserSupported);

    // Feed one further report into the claimed stroke. Which report it is
    // barely matters: the contact state decides. The platform emits extra,
    // self-contradictory press/release pairs every time the barrel changes
    // mid-stroke (`press button=Left buttons=Right`, then
    // `release button=Left buttons=Left`), and those say nothing about the tip
    // — so a report that arrives while the tip is still on the glass continues
    // the stroke (a move does the moving, a press or release produces nothing
    // at all), and only a report with the tip off the glass ends it.
    // See docs/design/touch-and-pen-input-design.md §5.1.
    Intents feed(Report report, const Sample &sample);

    // The stroke was aborted (window deactivated, widget hidden, gesture
    // cancelled, pen out of range). Releasing an erase stroke, never opening a
    // menu.
    Intents cancelled();

    [[nodiscard]] Phase phase() const;
    [[nodiscard]] Input input() const;
    // True while the claimed stroke erases (or, once it is over, did erase).
    [[nodiscard]] bool erases() const;
    // Where the pen went down. A deferred side-button press is replayed here so
    // the erase starts where the pen was put down, not where it crossed the
    // threshold.
    [[nodiscard]] QPointF origin() const;

private:
    [[nodiscard]] static Input classify(const Sample &sample);
    Intents moved(const Sample &sample);
    Intents released(const Sample &sample);
    void reset();

    Config m_config;
    Phase m_phase = Phase::Idle;

    Input m_input = Input::Tip;
    QPointF m_origin;
    QPointF m_lastPosition;
    // The tool's answer, cached for the whole stroke so the deferred
    // side-button decision is made against the tool that was armed at press.
    bool m_eraserSupported = false;
    // A side-button stroke has left the slop and become an erase stroke.
    bool m_sideButtonResolved = false;
};

#endif // EDITORPENSTROKE_H
