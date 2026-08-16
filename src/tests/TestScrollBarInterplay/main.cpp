#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>

#include "UI/Views/Common/EditorRhiScrollBarController.h"
#include "UI/Views/Common/EditorViewportController.h"
#include "UI/Views/Common/EditorWheelUtils.h"

#include <QGraphicsView>
#include <QScrollBar>
#include <QTextStream>
#include <QApplication>
#include <QWidget>

// Reproduces the piano-roll startup sequence: the custom bar is attached while
// the view is 0x0, then the view is resized, shown, and given a scene.
// Expected final state: handle length/position derived from the source range,
// exactly what the removed scene scrollbar (ScrollBarView) rendered. Before
// the fix, the overlay bar latched the source's default pageStep=10 on the
// first recompute (Qt emits rangeChanged before assigning the new pageStep),
// collapsing the handle to its 20px minimum and leaving it at mid-track.
namespace {
    int g_failures = 0;

    class WheelProbe final : public QWidget {
    public:
        int receivedWheelEvents = 0;

    protected:
        void wheelEvent(QWheelEvent *event) override {
            ++receivedWheelEvents;
            event->accept();
        }
    };

    void sendWheelEvent(QWidget *target) {
        const QPointF position(target->rect().center());
        const QPointF globalPosition(target->mapToGlobal(position.toPoint()));
        QWheelEvent event(position, globalPosition, {}, QPoint(0, -120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(target, &event);
    }

    void probe(const char *label, const QScrollBar *source, OverlayScrollBar *bar) {
        QTextStream(stdout) << "[" << label << "] src page=" << source->pageStep()
                            << " src max=" << source->maximum() << " | bar page=" << bar->pageStep()
                            << " bar max=" << bar->maximum() << " bar value=" << bar->value()
                            << Qt::endl;
    }

    void expect(const bool condition, const char *message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
    }
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QGraphicsView view;
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *bar = OverlayScrollBar::install(&view, Qt::Horizontal);
    bar->setFixedHeight(16);
    QScrollBar *source = view.horizontalScrollBar();

    const auto flush = [] {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    };

    // Scene ~2x the viewport, so the startup handle is ~50% and value is 0.
    const int sceneW = 1800, sceneH = 480;
    const int viewW = 900, viewH = 500;
    QGraphicsScene scene;
    scene.setSceneRect(0, 0, sceneW, sceneH);
    view.setScene(&scene);
    view.resize(viewW, viewH);
    view.show();
    flush();
    probe("startup", source, bar);

    expect(bar->maximum() > 0, "a scrolled view must show a range");
    expect(bar->maximum() == source->maximum(), "bar range must mirror the view range");
    expect(bar->pageStep() == source->pageStep(),
           "bar pageStep must copy the source after the recompute");
    expect(bar->value() == source->value(), "bar value must follow the source");
    const double handleFraction =
        double(source->pageStep()) / (source->maximum() + source->pageStep());
    expect(handleFraction > 0.4 && handleFraction < 0.6,
           "source handle length must be ~50% of the track, got " +
               QString::number(handleFraction).toUtf8());

    // Subsequent zoom keeps the copy in sync, and the value follows Qt.
    view.scale(1.4, 1.4);
    flush();
    expect(bar->maximum() == source->maximum(), "bar range must follow a zoom");
    expect(bar->pageStep() == source->pageStep(), "bar pageStep must follow a zoom");
    probe("after zoom", source, bar);

    QWidget rhiViewport;
    rhiViewport.resize(900, 500);
    EditorRhiScrollBarController rhiBars(&rhiViewport, &rhiViewport);
    QPointF requestedOffset(-1, -1);
    QObject::connect(&rhiBars, &EditorRhiScrollBarController::offsetChangeRequested, &rhiViewport,
                     [&requestedOffset](const QPointF &offset) { requestedOffset = offset; });
    rhiBars.setMetrics(QSizeF(1800, 1000), QPointF(0, 0), QSizeF(90, 50));
    rhiViewport.show();
    flush();

    auto *rhiHorizontal = rhiBars.horizontalBar();
    auto *rhiVertical = rhiBars.verticalBar();
    expect(rhiHorizontal->maximum() == 900 && rhiHorizontal->pageStep() == 900,
           "RHI horizontal metrics must describe one visible page");
    expect(rhiVertical->maximum() == 500 && rhiVertical->pageStep() == 500,
           "RHI vertical metrics must describe one visible page");
    expect(rhiHorizontal->isVisible() && rhiVertical->isVisible(),
           "RHI bars with overflow must be visible");
    expect(rhiHorizontal->width() == 884 && rhiVertical->height() == 484,
           "companion RHI bars must leave the bottom-right corner unobstructed");

    rhiHorizontal->setValue(450);
    flush();
    expect(requestedOffset == QPointF(450, 0),
           "dragging an RHI bar must request the corresponding camera offset");

    rhiBars.setMetrics(QSizeF(900, 500), QPointF(0, 0));
    flush();
    expect(!rhiHorizontal->isVisible() && !rhiVertical->isVisible(),
           "RHI bars without overflow must be hidden");

    rhiBars.setMetrics(QSizeF(900.4, 500.4), QPointF(0, 0));
    flush();
    expect(!rhiHorizontal->isVisible() && !rhiVertical->isVisible(),
           "subpixel layout noise must not create a false RHI scroll range");

    QWheelEvent angleWheel(QPointF(10, 10), QPointF(10, 10), {}, QPoint(120, -240), Qt::NoButton,
                           Qt::NoModifier, Qt::NoScrollPhase, false);
    expect(EditorWheelUtils::wheelDelta(&angleWheel, Qt::Horizontal) == 120.0 &&
               EditorWheelUtils::wheelDelta(&angleWheel, Qt::Vertical) == -240.0,
           "shared wheel delta extraction must select the requested angle axis");
    QWheelEvent pixelWheel(QPointF(10, 10), QPointF(10, 10), QPoint(3, -5), {}, Qt::NoButton,
                           Qt::NoModifier, Qt::ScrollUpdate, false);
    expect(EditorWheelUtils::wheelDelta(&pixelWheel, Qt::Horizontal) == 12.0 &&
               EditorWheelUtils::wheelDelta(&pixelWheel, Qt::Vertical) == -20.0,
           "shared wheel delta extraction must preserve pixel-only touchpad input");
    EditorWheelUtils::ScrollAccumulator pixelHorizontalScroll;
    EditorWheelUtils::ScrollAccumulator pixelVerticalScroll;
    expect(pixelHorizontalScroll.scrollTarget(100, 192, 0.15, &pixelWheel, Qt::Horizontal) == 97 &&
               pixelVerticalScroll.scrollTarget(100, 192, 0.15, &pixelWheel, Qt::Vertical) == 105,
           "touchpad scrolling must apply pixel displacement directly");
    EditorWheelUtils::InputState pixelInputState;
    expect(!pixelInputState.isMouseWheel(&pixelWheel),
           "pixel-only wheel events must use touchpad direct-scroll semantics");
    QWheelEvent combinedWheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, -120), QPoint(0, -120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    expect(EditorWheelUtils::wheelDelta(&combinedWheel, Qt::Vertical) == -120.0,
           "mouse wheels with both delta forms must retain their angle-step magnitude");
    EditorWheelUtils::ScrollAccumulator combinedScroll;
    expect(combinedScroll.scrollTarget(100, 192, 0.15, &combinedWheel, Qt::Vertical) == 128,
           "one mouse wheel event must retain the legacy viewport-relative distance");
    EditorWheelUtils::InputState combinedInputState;
    expect(combinedInputState.isMouseWheel(&combinedWheel),
           "a discrete mouse wheel must not be reclassified by an auxiliary pixel delta");
    QWheelEvent horizontalTouchPad(QPointF(10, 10), QPointF(10, 10), QPoint(5, 0), {}, Qt::NoButton,
                                   Qt::NoModifier, Qt::ScrollUpdate, false);
    EditorWheelUtils::ScrollAccumulator touchPadHorizontalScroll;
    expect(EditorWheelUtils::dominantAxis(&horizontalTouchPad) == Qt::Horizontal &&
               touchPadHorizontalScroll.scrollTarget(
                   100, 192, 0.2, &horizontalTouchPad,
                   EditorWheelUtils::horizontalScrollAxis(&horizontalTouchPad)) == 95,
           "unmodified horizontal touchpad gestures must retain their natural scroll axis");
    QWheelEvent shiftedWheel(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -120), Qt::NoButton,
                             Qt::ShiftModifier, Qt::NoScrollPhase, false);
    EditorWheelUtils::ScrollAccumulator shiftedScroll;
    expect(shiftedScroll.scrollTarget(100, 200, 0.2, &shiftedWheel,
                                      EditorWheelUtils::horizontalScrollAxis(&shiftedWheel)) == 140,
           "Shift-wheel gestures must continue mapping the vertical wheel to horizontal scroll");

    QWheelEvent fineWheelDown(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -1), Qt::NoButton,
                              Qt::NoModifier, Qt::ScrollUpdate, false);
    QWheelEvent fineWheelUp(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, 1), Qt::NoButton,
                            Qt::NoModifier, Qt::ScrollUpdate, false);
    EditorWheelUtils::ScrollAccumulator fineDownScroll;
    EditorWheelUtils::ScrollAccumulator fineUpScroll;
    auto fineDownOffset = 100;
    auto fineUpOffset = 100;
    for (int i = 0; i < 4; ++i) {
        fineDownOffset =
            fineDownScroll.scrollTarget(fineDownOffset, 240, 0.15, &fineWheelDown, Qt::Vertical);
        fineUpOffset =
            fineUpScroll.scrollTarget(fineUpOffset, 240, 0.15, &fineWheelUp, Qt::Vertical);
    }
    expect(fineDownOffset - 100 == 100 - fineUpOffset && fineDownOffset > 100,
           "fine angle deltas must accumulate symmetrically in both directions");

    EditorWheelUtils::ScrollAccumulator reversingScroll;
    auto reversingOffset = 100;
    for (int i = 0; i < 2; ++i)
        reversingOffset =
            reversingScroll.scrollTarget(reversingOffset, 240, 0.15, &fineWheelDown, Qt::Vertical);
    for (int i = 0; i < 4; ++i)
        reversingOffset =
            reversingScroll.scrollTarget(reversingOffset, 240, 0.15, &fineWheelUp, Qt::Vertical);
    expect(reversingOffset == 99,
           "reversing a fine wheel gesture must discard the opposite-direction remainder");

    WheelProbe wheelParent;
    wheelParent.resize(300, 200);
    QWidget wheelContainer(&wheelParent);
    wheelContainer.setGeometry(0, 0, 300, 200);
    Button wheelButton(&wheelContainer);
    wheelButton.setGeometry(10, 10, 80, 30);
    wheelButton.setWheelEventPolicy(WheelEventPolicy::Consume);
    ComboBox wheelComboBox(WheelEventPolicy::Consume, &wheelContainer);
    wheelComboBox.setGeometry(10, 50, 120, 30);
    wheelComboBox.addItems({"First", "Second"});
    wheelParent.show();
    flush();
    sendWheelEvent(&wheelButton);
    sendWheelEvent(&wheelComboBox);
    expect(wheelParent.receivedWheelEvents == 0 && wheelComboBox.currentIndex() == 0,
           "buttons and combo boxes must consume wheel input without changing selection");
    wheelButton.setWheelEventPolicy(WheelEventPolicy::Pass);
    wheelComboBox.setWheelEventPolicy(WheelEventPolicy::Pass);
    sendWheelEvent(&wheelButton);
    expect(wheelParent.receivedWheelEvents == 1,
           "buttons must support forwarding wheel input to their parent");
    sendWheelEvent(&wheelComboBox);
    expect(wheelParent.receivedWheelEvents == 2 && wheelComboBox.currentIndex() == 0,
           "combo boxes must support forwarding wheel input to their parent");
    wheelComboBox.setWheelEventPolicy(WheelEventPolicy::Handle);
    sendWheelEvent(&wheelComboBox);
    expect(wheelParent.receivedWheelEvents == 2 && wheelComboBox.currentIndex() == 1,
           "combo boxes must retain explicit wheel selection handling");

    EditorViewportController marginViewport;
    marginViewport.setContentTickRange(0, 20000);
    marginViewport.setLeftMarginPx(10);
    marginViewport.setContentTickRange(0, 20000);
    marginViewport.setViewportSize(QSizeF(800, 300));
    expect(qFuzzyIsNull(marginViewport.horizontalOffset()) &&
               qFuzzyCompare(marginViewport.tickToSceneX(0), 10.0) &&
               qFuzzyCompare(marginViewport.startTick(), -75.0),
           "initializing an RHI margin must keep it visible before tick zero");

    EditorViewportController viewport;
    viewport.setEnsureContentFillsViewport(false, false);
    viewport.setContentTickRange(0, 20000);
    viewport.setVerticalContent(20, 72);
    viewport.setViewportSize(QSizeF(800, 300));
    viewport.scrollBy(QPointF(300.25, 400.25));
    viewport.setViewportSize(QSizeF(1000, 500));
    expect(qFuzzyCompare(viewport.horizontalOffset(), 300.0) &&
               qFuzzyCompare(viewport.verticalOffset(), 400.0),
           "RHI viewport offsets must use the integer coordinates exposed by legacy scrollbars");

    EditorViewportController pianoViewport;
    pianoViewport.setPixelsPerQuarterNote(64.0);
    pianoViewport.setScaleBounds(0.01, 5.0, 0.5, 8.0);
    pianoViewport.setEnsureContentFillsViewport(true, true);
    pianoViewport.setContentTickRange(0.0, 9600.0);
    pianoViewport.setVerticalContent(128.0, 12.0);
    pianoViewport.setViewportSize(QSizeF(800.0, 360.0));
    pianoViewport.setScale(1.25, 2.0, QPointF(400.0, 180.0));
    pianoViewport.centerAt(4800.0, 66.5);
    const auto pianoCenter = pianoViewport.state();
    expect(qFuzzyCompare(pianoCenter.centerTick, 4800.0) &&
               qFuzzyCompare(pianoCenter.centerUnit, 66.5) &&
               qFuzzyCompare(127.0 - pianoCenter.centerUnit, 60.5),
           "the shared RHI viewport must represent piano-roll tick and descending-key centers");
    pianoViewport.setScale(2.0, 3.0, QPointF(400.0, 180.0));
    const auto zoomedPianoCenter = pianoViewport.state();
    expect(qFuzzyCompare(zoomedPianoCenter.centerTick, pianoCenter.centerTick) &&
               qFuzzyCompare(zoomedPianoCenter.centerUnit, pianoCenter.centerUnit),
           "piano-roll zooming through the shared viewport must preserve its center anchor");

    viewport.scrollBy(QPointF(100000, 100000));
    viewport.setViewportSize(QSizeF(1900, 1300));
    expect(qFuzzyCompare(viewport.horizontalOffset(), 767.0) &&
               qFuzzyCompare(viewport.verticalOffset(), 140.0),
           "resizing must clamp preserved offsets to the new scroll range");

    EditorViewportController focusViewport;
    focusViewport.setEnsureContentFillsViewport(false, false);
    focusViewport.setContentTickRange(0, 30000);
    focusViewport.setVerticalContent(20, 72);
    focusViewport.setViewportSize(QSizeF(800, 300));
    focusViewport.scrollBy(QPointF(300, 400));
    int focusViewportChanges = 0;
    QObject::connect(&focusViewport, &EditorViewportController::viewportChanged, &rhiViewport,
                     [&focusViewportChanges] { ++focusViewportChanges; });
    expect(focusViewport.ensureVisible(QRectF(500, 500, 100, 50), 24, 24) &&
               qFuzzyCompare(focusViewport.horizontalOffset(), 300.0) &&
               qFuzzyCompare(focusViewport.verticalOffset(), 400.0) && focusViewportChanges == 0,
           "revealing an already visible RHI focus must not move or notify the viewport");
    expect(focusViewport.ensureVisible(QRectF(1050, 680, 100, 40), 24, 24) &&
               qFuzzyCompare(focusViewport.horizontalOffset(), 374.0) &&
               qFuzzyCompare(focusViewport.verticalOffset(), 444.0) && focusViewportChanges == 1,
           "revealing an obscured RHI focus must scroll only the minimum required distance");
    expect(focusViewport.ensureVisible(QRectF(320, 420, 10, 10), 24, 24) &&
               qFuzzyCompare(focusViewport.horizontalOffset(), 296.0) &&
               qFuzzyCompare(focusViewport.verticalOffset(), 396.0) && focusViewportChanges == 2,
           "revealing toward the leading edges must preserve the requested margin");

    EditorViewportController boundedViewport;
    boundedViewport.setEnsureContentFillsViewport(false, false);
    boundedViewport.setContentTickRange(0, 100000);
    boundedViewport.setVerticalContent(2, 100);
    boundedViewport.setViewportSize(QSizeF(800, 300));
    int boundedViewportChanges = 0;
    QObject::connect(&boundedViewport, &EditorViewportController::viewportChanged, &rhiViewport,
                     [&boundedViewportChanges] { ++boundedViewportChanges; });
    boundedViewport.setStartTick(1000000);
    expect(boundedViewportChanges == 1,
           "scrolling to the content boundary must notify the viewport once");
    boundedViewport.setStartTick(1000000);
    boundedViewport.scrollBy(QPointF(100, 100));
    expect(boundedViewportChanges == 1,
           "repeated scrolling beyond a clamped boundary must not notify the viewport");

    if (g_failures == 0) {
        QTextStream(stdout) << "All ScrollBarInterplay tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
