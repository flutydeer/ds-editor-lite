// Unit tests for the pointer-anchored placement of ToolTip, the placement used
// by every drag interaction that shows a live value (phoneme length, speaker
// mix percentages).
//
// The bug being locked down: the card used to be put at the pointer's own
// coordinates, so its content landed ~(+24, +20) away and, during a touch or
// pen drag, right under the fingertip. The contract now is "visible card
// horizontally centred on the pointer and clear above it", plus the edge cases
// that come with anchoring to a moving point (screen clamping, flipping when
// the pointer is near the top edge).
//
// All assertions are written as relations between the pointer, the card and the
// screen, so they hold on any display size, scale factor and DPI.

#include <lite/GUI/Controls/ToolTip.h>

#include <QApplication>
#include <QLayout>
#include <QScreen>
#include <QTextStream>

namespace {

    int g_failures = 0;

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
        return false;
    }

    // The tooltip widget carries 16 px of layout margin around the card it
    // paints, so every geometric claim has to be made about the card, not about
    // the widget. Mirrors the mapping ToolTip::positionAbove() uses.
    QRect visibleCard(const ToolTip &toolTip) {
        const auto margins = toolTip.layout()->contentsMargins();
        return toolTip.geometry().adjusted(margins.left(), margins.top(), -margins.right(),
                                           -margins.bottom());
    }

    bool closeEnough(const int a, const int b) {
        return qAbs(a - b) <= 1;
    }

} // namespace

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);

    const auto *screen = QApplication::primaryScreen();
    if (!screen) {
        QTextStream(stdout) << "SKIP: no primary screen, geometry cannot be asserted" << Qt::endl;
        return 0;
    }

    const auto available = screen->availableGeometry();

    ToolTip toolTip(QStringLiteral("123 ms"));
    toolTip.setAttribute(Qt::WA_DontShowOnScreen);
    // Drag readouts must appear and disappear with no fade: the value changes
    // on every pointer move, so an in-flight opacity animation would never
    // settle. Every production call site sets this; assert it here too.
    toolTip.setAnimationEnabled(false);

    const auto clearance = ToolTip::pointerClearance(screen);

    expect(clearance >= 24, "pointer clearance must never collapse below 24 px");
    expect(clearance == ToolTip::pointerClearance(nullptr),
           "a null screen must resolve to the primary screen");

    // --- Centred on the pointer, clear above it ---
    {
        const auto pointer = available.center();
        toolTip.showAbovePointer(pointer);

        const auto card = visibleCard(toolTip);
        expect(closeEnough(card.center().x(), pointer.x()),
               "the card must be horizontally centred on the pointer");
        expect(closeEnough(pointer.y() - card.bottom(), clearance),
               "the card must sit `pointerClearance` above the pointer");
        expect(card.bottom() < pointer.y(),
               "the card must never reach the pointer, that is where the fingertip is");
    }

    // --- Re-centring after the content changes width ---
    {
        const auto pointer = available.center();
        toolTip.setTitle(QStringLiteral("1234 ms"));
        toolTip.moveAbovePointer(pointer);

        expect(closeEnough(visibleCard(toolTip).center().x(), pointer.x()),
               "a wider readout must be re-centred, not left anchored by its old width");
        expect(closeEnough(pointer.y() - visibleCard(toolTip).bottom(), clearance),
               "the clearance must survive a content change");
    }

    // --- Clamped at the left edge instead of running off screen ---
    {
        const auto pointer = QPoint(available.left() + 2, available.center().y());
        toolTip.showAbovePointer(pointer);

        expect(visibleCard(toolTip).left() >= available.left(),
               "a pointer at the left edge must clamp the card onto the screen");
    }

    // --- Flipped below when there is no room above ---
    {
        const auto cardHeight = visibleCard(toolTip).height();
        if (available.height() < cardHeight * 3) {
            QTextStream(stdout) << "SKIP: screen too short to test the flip" << Qt::endl;
        } else {
            const auto pointer = QPoint(available.center().x(), available.top() + 2);
            toolTip.showAbovePointer(pointer);

            const auto card = visibleCard(toolTip);
            expect(card.top() >= pointer.y(),
                   "with no room above, the card must flip below the pointer rather than "
                   "be clamped back underneath it");
            expect(card.bottom() <= available.bottom(),
                   "the flipped card must still be on screen");
        }
    }

    // --- Shown instantly when the animation is off ---
    {
        toolTip.hide();
        toolTip.setWindowOpacity(0);
        toolTip.showAbovePointer(available.center());

        expect(toolTip.isVisible(), "showAbovePointer must show the tooltip");
        expect(qFuzzyCompare(toolTip.windowOpacity(), 1.0),
               "with the animation disabled the tooltip must appear at full opacity");
    }

    // --- moveAbovePointer only moves ---
    {
        toolTip.hide();
        toolTip.setWindowOpacity(0);
        const auto before = toolTip.geometry().topLeft();
        const auto target = available.center() + QPoint(40, 0);
        toolTip.moveAbovePointer(target);

        expect(toolTip.geometry().topLeft() != before, "moveAbovePointer must reposition the card");
        expect(!toolTip.isVisible(), "moveAbovePointer must not show a hidden tooltip");
        expect(qFuzzyCompare(toolTip.windowOpacity(), 0.0),
               "moveAbovePointer must not touch the opacity");
        expect(closeEnough(visibleCard(toolTip).center().x(), target.x()),
               "the moved card must still be centred on the pointer");
    }

    // --- The rect anchor keeps its own, tighter gap (lyric hover tooltip) ---
    {
        const auto anchor = QRect(available.center() - QPoint(20, 10), QSize(40, 20));
        toolTip.showAbove(anchor);

        expect(closeEnough(anchor.top() - visibleCard(toolTip).bottom(), 4),
               "the rect anchor must keep its 4 px gap, it serves mouse hover tooltips");
        expect(closeEnough(visibleCard(toolTip).center().x(), anchor.center().x()),
               "the rect anchor must stay centred on the rect");
    }

    // --- An explicit null screen must not crash or skip the clamping ---
    {
        const auto pointer = available.center();
        toolTip.showAbovePointer(pointer, nullptr);

        expect(closeEnough(visibleCard(toolTip).center().x(), pointer.x()),
               "an explicit null screen must fall back to the screen under the pointer");
        expect(closeEnough(pointer.y() - visibleCard(toolTip).bottom(), clearance),
               "an explicit null screen must still keep the pointer clearance");
    }

    if (g_failures == 0) {
        QTextStream(stdout) << "All ToolTipPointerAnchor tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
