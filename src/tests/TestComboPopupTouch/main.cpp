#include <lite/GUI/Controls/ComboPopupTouchFilter.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QMouseEvent>
#include <QScroller>
#include <QScrollBar>
#include <QTest>
#include <QWidget>

#include <cstdio>

namespace {

    int g_failures = 0;

    void check(bool ok, const char *what) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++g_failures;
    }

    // A raw QComboBox like the LogWindow filters, with the popup touch
    // behavior installed. The 2 mm boundaries (filter tap threshold and
    // scroller DragStartDistance) are deterministic on offscreen: it reports
    // a sane 100 dpi, so 2 mm ~ 7.9 px.
    QComboBox *makeCombo() {
        auto *combo = new QComboBox;
        for (int i = 0; i < 60; ++i)
            combo->addItem(QStringLiteral("item %1").arg(i));
        ComboPopupTouchFilter::install(combo);
        combo->resize(160, 28);
        combo->show();
        QApplication::processEvents();
        combo->showPopup();
        QApplication::processEvents();
        return combo;
    }

    QPoint itemCenter(const QComboBox *combo, int index) {
        return combo->view()->visualRect(combo->view()->model()->index(index, 0)).center();
    }

    // A tap is a touch-derived press + release pair at (almost) the same
    // position; Windows emits both a BySystem and a ByQt stream per touch.
    void sendSynthClick(QWidget *target, const QPointF &pos, Qt::MouseEventSource source) {
        const auto globalPos = target->mapToGlobal(pos.toPoint());
        QMouseEvent press(QEvent::MouseButtonPress, pos, globalPos, globalPos, Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier, source);
        QCoreApplication::sendEvent(target, &press);
        QApplication::processEvents();
        QMouseEvent release(QEvent::MouseButtonRelease, pos, globalPos, globalPos, Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier, source);
        QCoreApplication::sendEvent(target, &release);
        QApplication::processEvents();
    }

    void sendMousePress(QWidget *target, const QPointF &pos, Qt::MouseEventSource source) {
        const auto globalPos = target->mapToGlobal(pos.toPoint());
        QMouseEvent press(QEvent::MouseButtonPress, pos, globalPos, globalPos, Qt::LeftButton,
                          Qt::LeftButton, Qt::NoModifier, source);
        QCoreApplication::sendEvent(target, &press);
        QApplication::processEvents();
    }

    void sendMouseMove(QWidget *target, const QPointF &pos, Qt::MouseEventSource source) {
        const auto globalPos = target->mapToGlobal(pos.toPoint());
        QMouseEvent move(QEvent::MouseMove, pos, globalPos, globalPos, Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier, source);
        QCoreApplication::sendEvent(target, &move);
        QApplication::processEvents();
    }

    void sendMouseRelease(QWidget *target, const QPointF &pos, Qt::MouseEventSource source) {
        const auto globalPos = target->mapToGlobal(pos.toPoint());
        QMouseEvent release(QEvent::MouseButtonRelease, pos, globalPos, globalPos, Qt::LeftButton,
                            Qt::NoButton, Qt::NoModifier, source);
        QCoreApplication::sendEvent(target, &release);
        QApplication::processEvents();
    }

    void sendMouseClick(QWidget *target, const QPointF &pos, Qt::MouseEventSource source) {
        sendMousePress(target, pos, source);
        sendMouseRelease(target, pos, source);
    }

    // QScroller's recognizer ignores presses while another scroller is active
    // ("active scrollers always have priority"); a previous section's release
    // can leave a glide running. Settle before each new stream.
    void settleScrollers() {
        for (auto *active : QScroller::activeScrollers())
            active->stop();
        QApplication::processEvents();
    }

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    // --- install() arms a raw QComboBox popup ---
    QComboBox plainCombo;
    for (int i = 0; i < 60; ++i)
        plainCombo.addItem(QStringLiteral("item %1").arg(i));
    ComboPopupTouchFilter::install(&plainCombo);
    check(QScroller::hasScroller(plainCombo.view()->viewport()),
          "install attaches the kinetic scroller to a raw QComboBox popup");
    check(QScroller::scroller(plainCombo.view()->viewport())
                  ->scrollerProperties()
                  .scrollMetric(QScrollerProperties::DragStartDistance)
                  .toReal() > 0.0f,
          "install aligns DragStartDistance with the tap threshold");
    ComboPopupTouchFilter::install(&plainCombo);
    check(plainCombo.view()->viewport()->property("lite_comboPopupTouch").toBool() &&
              QScroller::hasScroller(plainCombo.view()->viewport()),
          "install is idempotent");

    // --- a real mouse click still selects and closes ---
    {
        auto *combo = makeCombo();
        sendMouseClick(combo->view()->viewport(), itemCenter(combo, 2),
                       Qt::MouseEventNotSynthesized);
        check(combo->currentIndex() == 2, "a real mouse click selects the item under the cursor");
        check(!combo->view()->isVisible(), "a real mouse click closes the popup");
        combo->hidePopup();
        delete combo;
    }

    // --- a real mouse drag still changes the selection (desktop behavior) ---
    {
        auto *combo = makeCombo();
        auto *viewport = combo->view()->viewport();
        sendMousePress(viewport, itemCenter(combo, 2), Qt::MouseEventNotSynthesized);
        sendMouseMove(viewport, itemCenter(combo, 3), Qt::MouseEventNotSynthesized);
        sendMouseRelease(viewport, itemCenter(combo, 3), Qt::MouseEventNotSynthesized);
        check(combo->currentIndex() == 3, "a real mouse drag still changes the selection");
        combo->hidePopup();
        delete combo;
    }

    // --- a touch tap (synthesized press + release) selects and closes ---
    for (const auto source : { Qt::MouseEventSynthesizedBySystem, Qt::MouseEventSynthesizedByQt }) {
        auto *combo = makeCombo();
        settleScrollers();
        sendSynthClick(combo->view()->viewport(), itemCenter(combo, 3), source);
        check(combo->currentIndex() == 3, "a touch tap selects the item under the finger");
        check(!combo->view()->isVisible(), "a touch tap closes the popup");
        combo->hidePopup();
        delete combo;
    }

    // --- a wiggle under the tap threshold is still a tap ---
    {
        auto *combo = makeCombo();
        settleScrollers();
        auto *viewport = combo->view()->viewport();
        const auto pos = itemCenter(combo, 2);
        sendMousePress(viewport, pos, Qt::MouseEventSynthesizedBySystem);
        sendMouseMove(viewport, pos + QPointF(3, 0), Qt::MouseEventSynthesizedBySystem);
        sendMouseRelease(viewport, pos + QPointF(3, 0), Qt::MouseEventSynthesizedBySystem);
        check(combo->currentIndex() == 2, "a wiggle under the tap threshold still selects");
        check(!combo->view()->isVisible(), "a wiggle tap closes the popup");
        combo->hidePopup();
        delete combo;
    }

    // --- a touch drag scrolls and never commits ---
    {
        auto *combo = makeCombo();
        settleScrollers();
        auto *viewport = combo->view()->viewport();
        auto *scrollBar = combo->view()->verticalScrollBar();
        const auto from = QPointF(viewport->width() / 2.0, viewport->height() - 40.0);
        sendMousePress(viewport, from, Qt::MouseEventSynthesizedBySystem);
        // QScroller applies dragging positions from a timer, so the moves
        // need event-loop time in between (as in TestTouchScrollClaim).
        for (int i = 1; i <= 4; ++i) {
            sendMouseMove(viewport, from - QPointF(0, i * 15.0), Qt::MouseEventSynthesizedBySystem);
            QTest::qWait(20);
        }
        sendMouseRelease(viewport, from - QPointF(0, 60.0), Qt::MouseEventSynthesizedBySystem);
        check(scrollBar->value() > 0, "a touch drag scrolls the popup list");
        check(combo->view()->isVisible(), "a drag release keeps the popup open");
        check(combo->currentIndex() == 0, "a drag release does not commit a selection");
        combo->hidePopup();
        delete combo;
    }

    // --- a drag that returns to its start still counts as a scroll stream ---
    // (the m_moved latch: an out-and-back drag is not a tap; the 1:1 drag
    // returns the content to its start, so the scrollbar value ends at 0)
    {
        auto *combo = makeCombo();
        settleScrollers();
        auto *viewport = combo->view()->viewport();
        const auto from = QPointF(viewport->width() / 2.0, viewport->height() - 40.0);
        sendMousePress(viewport, from, Qt::MouseEventSynthesizedBySystem);
        sendMouseMove(viewport, from - QPointF(0, 80.0), Qt::MouseEventSynthesizedBySystem);
        QTest::qWait(20);
        sendMouseMove(viewport, from, Qt::MouseEventSynthesizedBySystem);
        QTest::qWait(20);
        sendMouseRelease(viewport, from, Qt::MouseEventSynthesizedBySystem);
        check(combo->currentIndex() == 0, "returning to the start does not turn a drag into a tap");
        check(combo->view()->isVisible(), "the out-and-back drag keeps the popup open");
        combo->hidePopup();
        delete combo;
    }

    // --- the doubled synthesized stream (BySystem + ByQt) taps exactly once ---
    {
        auto *combo = makeCombo();
        settleScrollers();
        auto *viewport = combo->view()->viewport();
        const auto pos = itemCenter(combo, 2);
        sendMousePress(viewport, pos, Qt::MouseEventSynthesizedBySystem);
        sendMouseMove(viewport, pos + QPointF(3, 0), Qt::MouseEventSynthesizedBySystem);
        // The second stream's press must not reset the one in flight.
        sendMousePress(viewport, pos + QPointF(3, 0), Qt::MouseEventSynthesizedByQt);
        sendMouseRelease(viewport, pos + QPointF(3, 0), Qt::MouseEventSynthesizedBySystem);
        sendMouseRelease(viewport, pos + QPointF(3, 0), Qt::MouseEventSynthesizedByQt);
        check(combo->currentIndex() == 2, "the doubled synthesized stream taps exactly once");
        check(!combo->view()->isVisible(), "the doubled stream closes the popup once");
        combo->hidePopup();
        delete combo;
    }

    std::printf("%s (%d failure(s))\n", g_failures == 0 ? "ALL OK" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
