#include "tst_editor_interaction.h"

#include <QtTest/QTest>
#include <lite/GUI/Controls/Button.h>
#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>
#include <lite/GUI/Controls/WheelInputController.h>

#include "UI/Views/Common/EditorRhiScrollBarController.h"
#include "UI/Views/Common/TimeGraphicsScene.h"

#include <QGraphicsRectItem>
#include <QGraphicsView>
#include <QEventLoop>
#include <QScrollBar>
#include <QTextStream>
#include <QApplication>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>
#include <QPointingDevice>

#include <algorithm>

// Verify overlay metrics after deferred scene/range initialization.
namespace {

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

namespace {
    void flush() {
        QCoreApplication::sendPostedEvents();
        QCoreApplication::processEvents();
    }
}

void EditorInteractionTests::sceneAttachment() {
    g_capturedWarningCount = 0;
    TimeGraphicsScene timeScene;
    SceneAwareScalableItem sceneAwareItem;
    timeScene.addCommonItem(&sceneAwareItem);
    QVERIFY2(
        (sceneAwareItem.scaleInitializedInScene && sceneAwareItem.visibleRectInitializedInScene),
        "scene-dependent item geometry must initialize after scene attachment");
    timeScene.removeCommonItem(&sceneAwareItem);
    const auto previousMessageHandler = qInstallMessageHandler(
        [](const QtMsgType type, const QMessageLogContext &, const QString &) {
            if (type == QtWarningMsg)
                ++g_capturedWarningCount;
        });
    timeScene.removeCommonItem(nullptr);
    qInstallMessageHandler(previousMessageHandler);
    QVERIFY2((g_capturedWarningCount == 0),
             "removing an absent optional graphics item must not emit a Qt warning");
}

void EditorInteractionTests::overlayStartupAndZoom() {
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

    QVERIFY2((bar->maximum() > 0), "a scrolled view must show a range");
    QVERIFY2((bar->maximum() == source->maximum()), "bar range must mirror the view range");
    QVERIFY2((bar->pageStep() == source->pageStep()),
             "bar pageStep must copy the source after the recompute");
    QVERIFY2((bar->value() == source->value()), "bar value must follow the source");
    const double handleFraction =
        double(source->pageStep()) / (source->maximum() + source->pageStep());
    QVERIFY2((handleFraction > 0.4 && handleFraction < 0.6),
             "source handle length must be ~50% of the track, got " +
                 QString::number(handleFraction).toUtf8());

    // Subsequent zoom keeps the copy in sync, and the value follows Qt.
    view.scale(1.4, 1.4);
    flush();
    QVERIFY2((bar->maximum() == source->maximum()), "bar range must follow a zoom");
    QVERIFY2((bar->pageStep() == source->pageStep()), "bar pageStep must follow a zoom");
}

void EditorInteractionTests::rhiScrollbars() {
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
    QVERIFY2((rhiHorizontal->maximum() == 900 && rhiHorizontal->pageStep() == 900),
             "RHI horizontal metrics must describe one visible page");
    QVERIFY2((rhiVertical->maximum() == 500 && rhiVertical->pageStep() == 500),
             "RHI vertical metrics must describe one visible page");
    QVERIFY2((rhiHorizontal->isVisible() && rhiVertical->isVisible()),
             "RHI bars with overflow must be visible");
    QVERIFY2((rhiHorizontal->width() == 884 && rhiVertical->height() == 484),
             "companion RHI bars must leave the bottom-right corner unobstructed");

    QTest::keyClick(rhiHorizontal, Qt::Key_End);
    flush();
    QVERIFY2((requestedOffset == QPointF(900, 0)),
             "keyboard input on an RHI bar must request the corresponding camera offset");

    rhiBars.setMetrics(QSizeF(900, 500), QPointF(0, 0));
    flush();
    QVERIFY2((!rhiHorizontal->isVisible() && !rhiVertical->isVisible()),
             "RHI bars without overflow must be hidden");

    rhiBars.setMetrics(QSizeF(900.4, 500.4), QPointF(0, 0));
    flush();
    QVERIFY2((!rhiHorizontal->isVisible() && !rhiVertical->isVisible()),
             "subpixel layout noise must not create a false RHI scroll range");
}

void EditorInteractionTests::touchpadAndWheelDelta() {
    QWheelEvent angleWheel(QPointF(10, 10), QPointF(10, 10), {}, QPoint(120, -240), Qt::NoButton,
                           Qt::NoModifier, Qt::NoScrollPhase, false);
    QVERIFY2((WheelInput::zoomDelta(&angleWheel, Qt::Horizontal) == 120.0 &&
              WheelInput::zoomDelta(&angleWheel, Qt::Vertical) == -240.0),
             "shared wheel delta extraction must select the requested angle axis");
    QWheelEvent pixelWheel(QPointF(10, 10), QPointF(10, 10), QPoint(3, -5), {}, Qt::NoButton,
                           Qt::NoModifier, Qt::ScrollUpdate, false);
    QVERIFY2((WheelInput::zoomDelta(&pixelWheel, Qt::Horizontal) == 12.0 &&
              WheelInput::zoomDelta(&pixelWheel, Qt::Vertical) == -20.0),
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
    QVERIFY2((pixelHorizontalValue == 97.0 && pixelVerticalValue == 105.0),
             "touchpad scrolling must apply pixel displacement directly");
    WheelInput::DeviceState pixelInputState;
    QVERIFY2((!pixelInputState.isDiscrete(&pixelWheel)),
             "pixel-only wheel events must use touchpad direct-scroll semantics");
}

void EditorInteractionTests::discreteWheelWithPixelDelta() {
    QWheelEvent combinedWheel(QPointF(10, 10), QPointF(10, 10), QPoint(0, -120), QPoint(0, -120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
    QVERIFY2((WheelInput::zoomDelta(&combinedWheel, Qt::Vertical) == -120.0),
             "mouse wheels with both delta forms must retain their angle-step magnitude");
    double combinedValue = 100.0;
    WheelInputController combinedScroll;
    combinedScroll.setAnimationEnabled(false);
    configureScrollTarget(combinedScroll, Qt::Vertical, combinedValue, 1000.0, 28.0);
    combinedScroll.handleWheel(&combinedWheel, WheelInputController::Action::VerticalScroll,
                               Qt::Vertical);
    QVERIFY2((combinedValue == 128.0),
             "one mouse wheel event must retain the legacy viewport-relative distance");
    WheelInput::DeviceState combinedInputState;
    QVERIFY2((combinedInputState.isDiscrete(&combinedWheel)),
             "a discrete mouse wheel must not be reclassified by an auxiliary pixel delta");
}

void EditorInteractionTests::horizontalAndShiftGestures() {
    QWheelEvent horizontalTouchPad(QPointF(10, 10), QPointF(10, 10), QPoint(5, 0), {}, Qt::NoButton,
                                   Qt::NoModifier, Qt::ScrollUpdate, false);
    double touchPadHorizontalValue = 100.0;
    WheelInputController touchPadHorizontalScroll;
    touchPadHorizontalScroll.setAnimationEnabled(false);
    configureScrollTarget(touchPadHorizontalScroll, Qt::Horizontal, touchPadHorizontalValue, 1000.0,
                          38.0);
    touchPadHorizontalScroll.handleWheel(&horizontalTouchPad);
    QVERIFY2((WheelInput::dominantAxis(&horizontalTouchPad) == Qt::Horizontal &&
              touchPadHorizontalValue == 95.0),
             "unmodified horizontal touchpad gestures must retain their natural scroll axis");
    QWheelEvent shiftedWheel(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -120), Qt::NoButton,
                             Qt::ShiftModifier, Qt::NoScrollPhase, false);
    double shiftedValue = 100.0;
    WheelInputController shiftedScroll;
    shiftedScroll.setAnimationEnabled(false);
    configureScrollTarget(shiftedScroll, Qt::Horizontal, shiftedValue, 1000.0, 40.0);
    shiftedScroll.handleWheel(&shiftedWheel);
    QVERIFY2((shiftedValue == 140.0),
             "Shift-wheel gestures must continue mapping the vertical wheel to horizontal scroll");
}

void EditorInteractionTests::fractionalAndReversedWheelMotion() {
    // macOS distinguishes continuous input by the event device, not angle magnitude.
    const QPointingDevice touchpad(
        QStringLiteral("Test touchpad"), 1, QInputDevice::DeviceType::TouchPad,
        QPointingDevice::PointerType::Finger,
        QInputDevice::Capability::Position | QInputDevice::Capability::PixelScroll, 1, 0);
    QWheelEvent fineWheelDown(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -1), Qt::NoButton,
                              Qt::NoModifier, Qt::ScrollUpdate, false, Qt::MouseEventNotSynthesized,
                              &touchpad);
    QWheelEvent fineWheelUp(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, 1), Qt::NoButton,
                            Qt::NoModifier, Qt::ScrollUpdate, false, Qt::MouseEventNotSynthesized,
                            &touchpad);
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
    QCOMPARE(fineDownOffset, 101.0);
    QCOMPARE(fineUpOffset, 99.0);

    WheelInputController reversingScroll;
    reversingScroll.setAnimationEnabled(false);
    double reversingOffset = 100.0;
    configureScrollTarget(reversingScroll, Qt::Vertical, reversingOffset, 1000.0, 36.0);
    for (int i = 0; i < 2; ++i)
        reversingScroll.handleWheel(&fineWheelDown, WheelInputController::Action::VerticalScroll,
                                    Qt::Vertical);
    QCOMPARE(reversingOffset, 100.0);
    for (int i = 0; i < 4; ++i)
        reversingScroll.handleWheel(&fineWheelUp, WheelInputController::Action::VerticalScroll,
                                    Qt::Vertical);
    QCOMPARE(reversingOffset, 99.0);
}

void EditorInteractionTests::pendingWheelTargetsRespectBounds() {
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
    QVERIFY2((boundedWheelScroll.logicalScrollValue(Qt::Vertical) == 80.0),
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
    QVERIFY2((resizedWheelScroll.logicalScrollValue(Qt::Vertical) == 30.0),
             "range changes must re-clamp a pending wheel target before applying the next step");
    resizedWheelScroll.stop();
}

void EditorInteractionTests::stoppingWheelMotionPreservesExternalInput() {
    QWheelEvent wheelTowardEnd(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, -120), Qt::NoButton,
                               Qt::NoModifier, Qt::NoScrollPhase, false);
    QWheelEvent wheelAwayFromEnd(QPointF(10, 10), QPointF(10, 10), {}, QPoint(0, 120), Qt::NoButton,
                                 Qt::NoModifier, Qt::NoScrollPhase, false);
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
    QVERIFY2((qFuzzyCompare(scrollbarValue, 25.0) &&
              !scrollbarWheelScroll.logicalScrollValue(Qt::Vertical).has_value()),
             "external scrollbar input must remain authoritative after stopping wheel motion");
}

void EditorInteractionTests::wheelAndNativeZoomAnchors() {
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
    QVERIFY2((qFuzzyCompare(zoomValue, 1.4) && qFuzzyCompare(zoomAnchor, 25.0)),
             "editor wheel zoom must use the shared action mapping and preserve its anchor");
    zoomInput.zoomByFactor(Qt::Horizontal, 2.0, 30.0);
    QVERIFY2((qFuzzyCompare(zoomValue, 2.0) && qFuzzyCompare(zoomAnchor, 30.0)),
             "native touchpad zoom must share the same target bounds and anchor application");
}

void EditorInteractionTests::controlWheelPolicies() {
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
    QVERIFY2((wheelParent.receivedWheelEvents == 0 && buttonIgnored && comboBoxIgnored &&
              defaultComboBoxIgnored && wheelComboBox.currentIndex() == 0 &&
              defaultWheelComboBox.currentIndex() == 0),
             "default and consume policies must preserve ignored wheel input without changing "
             "selection");
    wheelButton.setWheelEventPolicy(WheelEventPolicy::Pass);
    const auto buttonPassed = sendWheelEvent(&wheelButton);
    QVERIFY2((wheelParent.receivedWheelEvents == 1 && buttonPassed),
             "buttons must support forwarding wheel input to their parent");
    const auto comboBoxPassed = sendWheelEvent(&passWheelComboBox);
    QVERIFY2((wheelParent.receivedWheelEvents == 2 && comboBoxPassed &&
              passWheelComboBox.currentIndex() == 0),
             "combo boxes must support forwarding wheel input to their parent");
    const auto comboBoxHandled = sendWheelEvent(&handleWheelComboBox);
    QVERIFY2((wheelParent.receivedWheelEvents == 2 && comboBoxHandled &&
              handleWheelComboBox.currentIndex() == 1),
             "combo boxes must retain explicit wheel selection handling");
}
