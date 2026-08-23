#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>
#include <lite/GUI/Controls/WheelInputController.h>

#include "UI/Views/Common/EditorRhiScrollBarController.h"
#include "UI/Views/Common/EditorViewportAnimation.h"
#include "UI/Views/Common/EditorViewportController.h"
#include "UI/Views/Common/TimeGraphicsScene.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"

#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QEventLoop>
#include <QScrollBar>
#include <QTextStream>
#include <QApplication>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>

// Reproduces the piano-roll startup sequence: the custom bar is attached while
// the view is 0x0, then the view is resized, shown, and given a scene.
// Expected final state: handle length/position derived from the source range,
// exactly what the removed scene scrollbar (ScrollBarView) rendered. Before
// the fix, the overlay bar latched the source's default pageStep=10 on the
// first recompute (Qt emits rangeChanged before assigning the new pageStep),
// collapsing the handle to its 20px minimum and leaving it at mid-track.
namespace {
    int g_failures = 0;
    int g_capturedWarningCount = 0;

    class WheelProbe final : public QWidget {
    public:
        int receivedWheelEvents = 0;

    protected:
        void wheelEvent(QWheelEvent *event) override {
            ++receivedWheelEvents;
            event->accept();
        }
    };

    bool sendWheelEvent(QWidget *target) {
        const QPointF position(target->rect().center());
        const QPointF globalPosition(target->mapToGlobal(position.toPoint()));
        QWheelEvent event(position, globalPosition, {}, QPoint(0, -120), Qt::NoButton,
                          Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(target, &event);
        return event.isAccepted();
    }

    class SceneAwareScalableItem final : public QGraphicsRectItem, public IScalableItem {
    public:
        bool scaleInitializedInScene = false;
        bool visibleRectInitializedInScene = false;

    protected:
        void afterSetScale() override {
            scaleInitializedInScene = scene() != nullptr;
        }

        void afterSetVisibleRect() override {
            visibleRectInitializedInScene = scene() != nullptr;
        }
    };

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

    void configureScrollTarget(WheelInputController &controller, const Qt::Orientation orientation,
                               double &value, const double maximum, const double step) {
        controller.setScrollTarget(
            orientation,
            {
                .value = [&value] { return value; },
                .setValue = [&value](const double newValue) { value = newValue; },
                .boundedValue =
                    [maximum](const double newValue) { return std::clamp(newValue, 0.0, maximum); },
                .step = [step] { return step; },
                .canScroll = [] { return true; },
            });
    }
} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    TimeGraphicsScene timeScene;
    SceneAwareScalableItem sceneAwareItem;
    timeScene.addCommonItem(&sceneAwareItem);
    expect(sceneAwareItem.scaleInitializedInScene && sceneAwareItem.visibleRectInitializedInScene,
           "scene-dependent item geometry must initialize after scene attachment");
    timeScene.removeCommonItem(&sceneAwareItem);
    const auto previousMessageHandler = qInstallMessageHandler(
        [](const QtMsgType type, const QMessageLogContext &, const QString &) {
            if (type == QtWarningMsg)
                ++g_capturedWarningCount;
        });
    timeScene.removeCommonItem(nullptr);
    qInstallMessageHandler(previousMessageHandler);
    expect(g_capturedWarningCount == 0,
           "removing an absent optional graphics item must not emit a Qt warning");

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
    expect(WheelInput::zoomDelta(&angleWheel, Qt::Horizontal) == 120.0 &&
               WheelInput::zoomDelta(&angleWheel, Qt::Vertical) == -240.0,
           "shared wheel delta extraction must select the requested angle axis");
    QWheelEvent pixelWheel(QPointF(10, 10), QPointF(10, 10), QPoint(3, -5), {}, Qt::NoButton,
                           Qt::NoModifier, Qt::ScrollUpdate, false);
    expect(WheelInput::zoomDelta(&pixelWheel, Qt::Horizontal) == 12.0 &&
               WheelInput::zoomDelta(&pixelWheel, Qt::Vertical) == -20.0,
           "shared wheel delta extraction must preserve pixel-only touchpad input");
    double pixelHorizontalValue = 100.0;
    double pixelVerticalValue = 100.0;
    WheelInputController pixelScroll;
    pixelScroll.setAnimationEnabled(false);
    configureScrollTarget(pixelScroll, Qt::Horizontal, pixelHorizontalValue, 1000.0, 28.0);
    configureScrollTarget(pixelScroll, Qt::Vertical, pixelVerticalValue, 1000.0, 28.0);
    pixelScroll.handleWheel(&pixelWheel, WheelInputController::Action::HorizontalScroll,
                            Qt::Horizontal);
    pixelScroll.handleWheel(&pixelWheel, WheelInputController::Action::VerticalScroll,
                            Qt::Vertical);
    expect(pixelHorizontalValue == 97.0 && pixelVerticalValue == 105.0,
           "touchpad scrolling must apply pixel displacement directly");
    WheelInput::DeviceState pixelInputState;
    expect(!pixelInputState.isDiscrete(&pixelWheel),
           "pixel-only wheel events must use touchpad direct-scroll semantics");
    QWheelEvent combinedWheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, -120), QPoint(0, -120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    expect(WheelInput::zoomDelta(&combinedWheel, Qt::Vertical) == -120.0,
           "mouse wheels with both delta forms must retain their angle-step magnitude");
    double combinedValue = 100.0;
    WheelInputController combinedScroll;
    combinedScroll.setAnimationEnabled(false);
    configureScrollTarget(combinedScroll, Qt::Vertical, combinedValue, 1000.0, 28.0);
    combinedScroll.handleWheel(&combinedWheel, WheelInputController::Action::VerticalScroll,
                               Qt::Vertical);
    expect(combinedValue == 128.0,
           "one mouse wheel event must retain the legacy viewport-relative distance");
    WheelInput::DeviceState combinedInputState;
    expect(combinedInputState.isDiscrete(&combinedWheel),
           "a discrete mouse wheel must not be reclassified by an auxiliary pixel delta");
    QWheelEvent horizontalTouchPad(QPointF(10, 10), QPointF(10, 10), QPoint(5, 0), {}, Qt::NoButton,
                                   Qt::NoModifier, Qt::ScrollUpdate, false);
    double touchPadHorizontalValue = 100.0;
    WheelInputController touchPadHorizontalScroll;
    touchPadHorizontalScroll.setAnimationEnabled(false);
    configureScrollTarget(touchPadHorizontalScroll, Qt::Horizontal, touchPadHorizontalValue, 1000.0,
                          38.0);
    touchPadHorizontalScroll.handleWheel(&horizontalTouchPad);
    expect(WheelInput::dominantAxis(&horizontalTouchPad) == Qt::Horizontal &&
               touchPadHorizontalValue == 95.0,
           "unmodified horizontal touchpad gestures must retain their natural scroll axis");
    QWheelEvent shiftedWheel(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -120), Qt::NoButton,
                             Qt::ShiftModifier, Qt::NoScrollPhase, false);
    double shiftedValue = 100.0;
    WheelInputController shiftedScroll;
    shiftedScroll.setAnimationEnabled(false);
    configureScrollTarget(shiftedScroll, Qt::Horizontal, shiftedValue, 1000.0, 40.0);
    shiftedScroll.handleWheel(&shiftedWheel);
    expect(shiftedValue == 140.0,
           "Shift-wheel gestures must continue mapping the vertical wheel to horizontal scroll");

    QWheelEvent fineWheelDown(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -1), Qt::NoButton,
                              Qt::NoModifier, Qt::ScrollUpdate, false);
    QWheelEvent fineWheelUp(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, 1), Qt::NoButton,
                            Qt::NoModifier, Qt::ScrollUpdate, false);
    WheelInputController fineDownScroll;
    WheelInputController fineUpScroll;
    fineDownScroll.setAnimationEnabled(false);
    fineUpScroll.setAnimationEnabled(false);
    double fineDownOffset = 100.0;
    double fineUpOffset = 100.0;
    configureScrollTarget(fineDownScroll, Qt::Vertical, fineDownOffset, 1000.0, 36.0);
    configureScrollTarget(fineUpScroll, Qt::Vertical, fineUpOffset, 1000.0, 36.0);
    for (int i = 0; i < 4; ++i) {
        fineDownScroll.handleWheel(&fineWheelDown, WheelInputController::Action::VerticalScroll,
                                   Qt::Vertical);
        fineUpScroll.handleWheel(&fineWheelUp, WheelInputController::Action::VerticalScroll,
                                 Qt::Vertical);
    }
    expect(fineDownOffset - 100 == 100 - fineUpOffset && fineDownOffset > 100,
           "fine angle deltas must accumulate symmetrically in both directions");

    WheelInputController reversingScroll;
    reversingScroll.setAnimationEnabled(false);
    double reversingOffset = 100.0;
    configureScrollTarget(reversingScroll, Qt::Vertical, reversingOffset, 1000.0, 36.0);
    for (int i = 0; i < 2; ++i)
        reversingScroll.handleWheel(&fineWheelDown, WheelInputController::Action::VerticalScroll,
                                    Qt::Vertical);
    for (int i = 0; i < 4; ++i)
        reversingScroll.handleWheel(&fineWheelUp, WheelInputController::Action::VerticalScroll,
                                    Qt::Vertical);
    expect(reversingOffset == 99,
           "reversing a fine wheel gesture must discard the opposite-direction remainder");

    double boundedValue = 90.0;
    WheelInputController boundedWheelScroll;
    boundedWheelScroll.setAnimationEnabled(true);
    boundedWheelScroll.setTimeScale(1.0);
    configureScrollTarget(boundedWheelScroll, Qt::Vertical, boundedValue, 100.0, 20.0);
    QWheelEvent wheelTowardEnd(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -120), Qt::NoButton,
                               Qt::NoModifier, Qt::NoScrollPhase, false);
    QWheelEvent wheelAwayFromEnd(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, 120), Qt::NoButton,
                                 Qt::NoModifier, Qt::NoScrollPhase, false);
    boundedWheelScroll.handleWheel(&wheelTowardEnd);
    boundedWheelScroll.handleWheel(&wheelTowardEnd);
    boundedWheelScroll.handleWheel(&wheelAwayFromEnd);
    expect(boundedWheelScroll.logicalScrollValue(Qt::Vertical) == 80.0,
           "wheel targets must clamp before stacking so reversing at an edge has no dead travel");
    boundedWheelScroll.stop();

    double resizedValue = 90.0;
    double resizedMaximum = 100.0;
    WheelInputController resizedWheelScroll;
    resizedWheelScroll.setAnimationEnabled(true);
    resizedWheelScroll.setTimeScale(1.0);
    resizedWheelScroll.setScrollTarget(
        Qt::Vertical, {
                          .value = [&resizedValue] { return resizedValue; },
                          .setValue = [&resizedValue](const double value) { resizedValue = value; },
                          .boundedValue =
                              [&resizedMaximum](const double value) {
                                  return std::clamp(value, 0.0, resizedMaximum);
                              },
                          .step = [] { return 20.0; },
                          .canScroll = [] { return true; },
                      });
    resizedWheelScroll.handleWheel(&wheelTowardEnd);
    resizedMaximum = 50.0;
    resizedWheelScroll.handleWheel(&wheelAwayFromEnd);
    expect(resizedWheelScroll.logicalScrollValue(Qt::Vertical) == 30.0,
           "range changes must re-clamp a pending wheel target before applying the next step");
    resizedWheelScroll.stop();

    double scrollbarValue = 90.0;
    WheelInputController scrollbarWheelScroll;
    scrollbarWheelScroll.setAnimationEnabled(true);
    scrollbarWheelScroll.setTimeScale(1.0);
    configureScrollTarget(scrollbarWheelScroll, Qt::Vertical, scrollbarValue, 100.0, 20.0);
    scrollbarWheelScroll.handleWheel(&wheelTowardEnd);
    scrollbarWheelScroll.stop();
    scrollbarValue = 25.0;
    QEventLoop scrollbarWait;
    QTimer::singleShot(300, &scrollbarWait, &QEventLoop::quit);
    scrollbarWait.exec();
    expect(qFuzzyCompare(scrollbarValue, 25.0) &&
               !scrollbarWheelScroll.logicalScrollValue(Qt::Vertical).has_value(),
           "external scrollbar input must remain authoritative after stopping wheel motion");

    double zoomValue = 1.0;
    double zoomAnchor = -1.0;
    WheelInputController zoomInput;
    zoomInput.setAnimationEnabled(false);
    zoomInput.setZoomTarget(
        Qt::Horizontal,
        {
            .value = [&zoomValue] { return zoomValue; },
            .setValueAt =
                [&zoomValue, &zoomAnchor](const double value, const double anchor) {
                    zoomValue = value;
                    zoomAnchor = anchor;
                },
            .boundedValue = [](const double value) { return std::clamp(value, 0.5, 2.0); },
            .step = 0.4,
        });
    QWheelEvent controlWheel(QPointF(25, 10), QPointF(25, 10), {}, QPoint(0, 120), Qt::NoButton,
                             Qt::ControlModifier, Qt::NoScrollPhase, false);
    zoomInput.handleWheel(&controlWheel);
    expect(qFuzzyCompare(zoomValue, 1.4) && qFuzzyCompare(zoomAnchor, 25.0),
           "editor wheel zoom must use the shared action mapping and preserve its anchor");
    zoomInput.zoomByFactor(Qt::Horizontal, 2.0, 30.0);
    expect(qFuzzyCompare(zoomValue, 2.0) && qFuzzyCompare(zoomAnchor, 30.0),
           "native touchpad zoom must share the same target bounds and anchor application");

    WheelProbe wheelParent;
    wheelParent.resize(300, 240);
    QWidget wheelContainer(&wheelParent);
    wheelContainer.setGeometry(0, 0, 300, 240);
    Button wheelButton(&wheelContainer);
    wheelButton.setGeometry(10, 10, 80, 30);
    wheelButton.setWheelEventPolicy(WheelEventPolicy::Consume);
    ComboBox wheelComboBox(WheelEventPolicy::Consume, &wheelContainer);
    wheelComboBox.setGeometry(10, 50, 120, 30);
    wheelComboBox.addItems({"First", "Second"});
    ComboBox defaultWheelComboBox(&wheelContainer);
    defaultWheelComboBox.setGeometry(10, 90, 120, 30);
    defaultWheelComboBox.addItems({"First", "Second"});
    ComboBox passWheelComboBox(WheelEventPolicy::Pass, &wheelContainer);
    passWheelComboBox.setGeometry(10, 130, 120, 30);
    passWheelComboBox.addItems({"First", "Second"});
    ComboBox handleWheelComboBox(WheelEventPolicy::Handle, &wheelContainer);
    handleWheelComboBox.setGeometry(10, 170, 120, 30);
    handleWheelComboBox.addItems({"First", "Second"});
    wheelParent.show();
    flush();
    const auto buttonIgnored = !sendWheelEvent(&wheelButton);
    const auto comboBoxIgnored = !sendWheelEvent(&wheelComboBox);
    const auto defaultComboBoxIgnored = !sendWheelEvent(&defaultWheelComboBox);
    expect(wheelParent.receivedWheelEvents == 0 && buttonIgnored && comboBoxIgnored &&
               defaultComboBoxIgnored && wheelComboBox.currentIndex() == 0 &&
               defaultWheelComboBox.currentIndex() == 0,
           "default and consume policies must preserve ignored wheel input without changing "
           "selection");
    wheelButton.setWheelEventPolicy(WheelEventPolicy::Pass);
    const auto buttonPassed = sendWheelEvent(&wheelButton);
    expect(wheelParent.receivedWheelEvents == 1 && buttonPassed,
           "buttons must support forwarding wheel input to their parent");
    const auto comboBoxPassed = sendWheelEvent(&passWheelComboBox);
    expect(wheelParent.receivedWheelEvents == 2 && comboBoxPassed &&
               passWheelComboBox.currentIndex() == 0,
           "combo boxes must support forwarding wheel input to their parent");
    const auto comboBoxHandled = sendWheelEvent(&handleWheelComboBox);
    expect(wheelParent.receivedWheelEvents == 2 && comboBoxHandled &&
               handleWheelComboBox.currentIndex() == 1,
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
    constexpr double targetKeyIndex = 60.0;
    const auto targetCenterUnit = PianoRollCoord::keyIndexToCenterY(targetKeyIndex, 1.0);
    pianoViewport.centerAt(4800.0, targetCenterUnit);
    const auto pianoCenter = pianoViewport.state();
    expect(qFuzzyCompare(pianoCenter.centerTick, 4800.0) &&
               qFuzzyCompare(pianoCenter.centerUnit, 67.5) &&
               qFuzzyCompare(PianoRollCoord::centerYToKeyIndex(pianoCenter.centerUnit, 1.0),
                             targetKeyIndex),
           "piano-roll key centers must round-trip through the shared RHI viewport");
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
    expect(focusViewport.ensureVisible(QRectF(1500, 900, 100, 40), 24, 24, true) &&
               focusViewport.logicalVisibleSceneRect().topLeft() == QPointF(824, 664),
           "an animated RHI focus reveal must publish its logical destination");
    expect(focusViewport.ensureVisible(QRectF(1500, 900, 100, 40), 24, 24, false) &&
               qFuzzyCompare(focusViewport.horizontalOffset(), 824.0) &&
               qFuzzyCompare(focusViewport.verticalOffset(), 664.0),
           "a non-animated RHI focus reveal must reach the same destination");
    expect(focusViewport.setOffset(QPointF(1200, 700), true) &&
               focusViewport.logicalVisibleSceneRect().topLeft() == QPointF(1200, 700),
           "an animated direct RHI viewport move must publish its logical destination");
    expect(focusViewport.setOffset(QPointF(300, 400)) &&
               focusViewport.visibleSceneRect().topLeft() == QPointF(300, 400) &&
               focusViewport.logicalVisibleSceneRect().topLeft() == QPointF(300, 400),
           "an immediate RHI viewport move must replace a pending animated destination");

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

    QPointF animatedOffset(10, 20);
    EditorViewportAnimation viewportAnimation(
        [&animatedOffset](const QPointF &offset) { animatedOffset = offset; });
    viewportAnimation.setAnimationEnabled(true);
    viewportAnimation.moveTo(animatedOffset, QPointF(100, 200), true);
    expect(viewportAnimation.isRunning() &&
               viewportAnimation.logicalOffset(animatedOffset) == QPointF(100, 200),
           "an animated RHI viewport move must expose its logical destination immediately");
    viewportAnimation.moveTo(animatedOffset, QPointF(100, 200), false);
    expect(!viewportAnimation.isRunning() && animatedOffset == QPointF(100, 200),
           "a non-animated RHI viewport move must apply the same destination immediately");

    if (g_failures == 0) {
        QTextStream(stdout) << "All ScrollBarInterplay tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
