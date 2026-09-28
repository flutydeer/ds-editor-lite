#include <lite/GUI/Controls/SvsSeekbar.h>
#include <lite/GUI/Controls/SmoothScroller.h>
#include <lite/GUI/Controls/TouchClaimFilter.h>

#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QScroller>
#include <QScrollBar>
#include <QScrollArea>
#include <QTest>
#include <QVBoxLayout>
#include <QWidget>

#include <cstdio>

namespace {

    int g_failures = 0;

    void check(bool ok, const char *what) {
        std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
        if (!ok)
            ++g_failures;
    }

    // Records that a claimed widget actually received the replayed mouse press.
    // Records that a claimed widget actually received the replayed mouse press.
    class MouseButtonRecorder : public QObject {
    public:
        using QObject::QObject;

        bool sawPress = false;

    protected:
        bool eventFilter(QObject *watched, QEvent *event) override {
            if (event->type() == QEvent::MouseButtonPress)
                sawPress = true;
            return QObject::eventFilter(watched, event);
        }
    };

    QWidget *makeTallContent(int height) {
        auto *content = new QWidget;
        content->setFixedSize(400, height);
        return content;
    }

    QPoint seekBarCenterInViewport(const SVS::SeekBar *seekBar, const QScrollArea *area) {
        return seekBar->mapTo(area->viewport(), seekBar->rect().center());
    }

    // Drags the primary touch point from \p from to \p to in viewport-local
    // coordinates, spacing the moves so QScroller can estimate a flick velocity.
    void touchDrag(QWidget *viewport, QPointingDevice *device, const QPoint &from,
                   const QPoint &to, int steps) {
        QTest::touchEvent(viewport, device).press(0, from);
        QApplication::processEvents();
        for (int i = 1; i <= steps; ++i) {
            QTest::touchEvent(viewport, device)
                .move(0, from + (to - from) * i / steps);
            QApplication::processEvents();
            QTest::qWait(15);
        }
        QTest::touchEvent(viewport, device).release(0, to);
        QApplication::processEvents();
    }

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    auto *touchDevice = QTest::createTouchDevice();

    // --- attachTo installs touch kinetic scrolling on the viewport ---
    QScrollArea scrollArea;
    scrollArea.setWidget(makeTallContent(1200));
    scrollArea.resize(400, 300);
    scrollArea.show();
    SmoothScroller scroller;
    scroller.attachTo(&scrollArea);
    QApplication::processEvents();

    check(scrollArea.viewport()->testAttribute(Qt::WA_AcceptTouchEvents),
          "viewport accepts touch events after attach");
    // The offscreen platform reports a bogus physical DPI, which inflates the
    // meter-based DragStartDistance; neutralize it for deterministic tests.
    auto *scrollerHandle = QScroller::scroller(scrollArea.viewport());
    QScrollerProperties testProperties = scrollerHandle->scrollerProperties();
    testProperties.setScrollMetric(QScrollerProperties::DragStartDistance, 0.0);
    scrollerHandle->setScrollerProperties(testProperties);
    // QScroller's kinetic physics is meter-based; with a broken offscreen DPI
    // the velocity degenerates and gliding can never trigger, so the inertia
    // assertions only apply when the platform reports a sane pixel-per-meter.
    const auto ppm = scrollerHandle->pixelPerMeter();
    const bool sanePpm = qIsFinite(ppm.x()) && qIsFinite(ppm.y()) && ppm.x() > 0 && ppm.y() > 0;
    std::printf("  [probe] pixelPerMeter: (%f, %f) -> inertia checks %s\n", ppm.x(), ppm.y(),
                sanePpm ? "enabled" : "skipped (offscreen DPI artifact, verified on device)");

    // --- single-finger touch drag scrolls, release keeps gliding ---
    auto *viewport = scrollArea.viewport();
    auto *scrollBar = scrollArea.verticalScrollBar();
    touchDrag(viewport, touchDevice, QPoint(200, 260), QPoint(200, 40), 10);
    check(scrollBar->value() > 0, "touch drag scrolls the area");
    const bool gliding = QScroller::scroller(viewport)->state() == QScroller::Scrolling;
    if (!sanePpm) {
        std::printf("SKIP QScroller glide after flick (offscreen DPI artifact)\n");
    } else if (gliding) {
        const auto valueAtRelease = scrollBar->value();
        QTest::qWait(200);
        check(scrollBar->value() > valueAtRelease, "release keeps gliding (inertia)");
    } else {
        check(false, "QScroller enters Scrolling state after a flick");
    }

    // --- a wheel stops the glide and animates on its own ---
    // Not asserted here. A QWheelEvent built by hand is non-spontaneous, and
    // measured on Qt 6.11.2 offscreen it never reaches the viewport's event
    // filters at all: an identity filter installed alongside SmoothScroller's
    // does not see it either, and the scrollbar does not move. Sending it to the
    // window handle instead only reaches the viewport intermittently, so it
    // cannot carry an assertion. Real wheel events are spontaneous and do reach
    // the viewport, so this path is verified on device - see the tablet
    // regression checklist in docs/design/touch-and-pen-input-design.md.
    if (sanePpm && QScroller::scroller(viewport)->state() == QScroller::Scrolling) {
        std::printf("SKIP wheel-stops-the-glide (a synthesized wheel never reaches the "
                    "viewport filter offscreen)\n");
    }

    // --- drag-owning children are claimed automatically ---
    QScrollArea sliderArea;
    auto *sliderContent = makeTallContent(1200);
    const auto sliderLayout = new QVBoxLayout(sliderContent);
    sliderLayout->setContentsMargins(0, 0, 0, 0);
    auto *seekBar = new SVS::SeekBar;
    seekBar->setRange(0.0, 100.0);
    seekBar->setValue(10.0);
    sliderLayout->addWidget(seekBar);
    sliderArea.setWidget(sliderContent);
    sliderArea.resize(400, 300);
    sliderArea.show();
    SmoothScroller sliderScroller;
    sliderScroller.attachTo(&sliderArea);
    QApplication::processEvents(); // runs the queued claim sweep
    check(seekBar->testAttribute(Qt::WA_AcceptTouchEvents), "SVS::SeekBar child is claimed");

    const auto startValue = seekBar->value();
    // The seekbar sits vertically centered in the tall content; scroll it into
    // the viewport before touching it, then recompute its position.
    const auto center = [&] {
        const auto unscrolled = seekBarCenterInViewport(seekBar, &sliderArea);
        sliderArea.verticalScrollBar()->setValue(qMax(0, unscrolled.y() - 100));
        QApplication::processEvents();
        return seekBarCenterInViewport(seekBar, &sliderArea);
    }();
    const auto scrollBeforeDrag = sliderArea.verticalScrollBar()->value();
    touchDrag(sliderArea.viewport(), touchDevice, center, center + QPoint(120, 4), 6);
    check(seekBar->value() > startValue, "touch drag adjusts the claimed seekbar");
    check(sliderArea.verticalScrollBar()->value() == scrollBeforeDrag,
          "claimed seekbar does not scroll the page");

    // --- TouchKinetic::Disabled keeps the stock behavior ---
    QScrollArea plainArea;
    plainArea.setWidget(makeTallContent(1200));
    plainArea.resize(400, 300);
    plainArea.show();
    SmoothScroller plainScroller;
    plainScroller.attachTo(&plainArea, SmoothScroller::TouchKinetic::Disabled);
    QApplication::processEvents();
    check(!plainArea.viewport()->testAttribute(Qt::WA_AcceptTouchEvents),
          "opt-out leaves touch delivery off");
    touchDrag(plainArea.viewport(), touchDevice, QPoint(200, 260), QPoint(200, 40), 10);
    check(plainArea.verticalScrollBar()->value() == 0,
          "opt-out area does not scroll from a touch drag");

    // --- explicit claim (drag handles) replays touch as mouse ---
    QLabel handle;
    handle.resize(28, 28);
    handle.show();
    QApplication::processEvents();
    MouseButtonRecorder recorder;
    handle.installEventFilter(&recorder);
    SmoothScroller::installClaim(&handle);
    check(handle.testAttribute(Qt::WA_AcceptTouchEvents), "explicit claim enables touch delivery");
    QTest::touchEvent(&handle, touchDevice).press(0, QPoint(5, 5));
    QApplication::processEvents();
    check(recorder.sawPress, "claimed widget receives the replayed mouse press");

    // --- claim re-installs after the content tree is rebuilt ---
    auto *replacement = makeTallContent(1200);
    const auto replacementLayout = new QVBoxLayout(replacement);
    replacementLayout->setContentsMargins(0, 0, 0, 0);
    auto *replacementBar = new SVS::SeekBar;
    replacementBar->setRange(0.0, 100.0);
    replacementLayout->addWidget(replacementBar);
    sliderArea.setWidget(replacement);
    QApplication::processEvents();
    check(replacementBar->testAttribute(Qt::WA_AcceptTouchEvents),
          "claim sweep re-runs after setWidget rebuild");

    std::printf("%s (%d failure(s))\n", g_failures == 0 ? "ALL OK" : "FAILED", g_failures);
    return g_failures == 0 ? 0 : 1;
}
