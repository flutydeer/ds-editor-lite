#include "tst_editor_interaction.h"

#include "UI/Views/Common/EditorViewportAnimation.h"
#include "UI/Views/Common/EditorViewportController.h"
#include "UI/Views/Common/TimeGraphicsScene.h"
#include "UI/Views/Common/TimeGraphicsView.h"
#include "UI/Views/ClipEditor/PianoRoll/PianoRollCoord.h"

#include <QtTest/QTest>
#include <QApplication>
#include <QWheelEvent>
#include <QSignalSpy>
#include <QPointingDevice>

#include <cmath>

namespace {
    void prepareTimeView(TimeGraphicsScene &scene, TimeGraphicsView &view, double scale = 1.0) {
        scene.setSceneBaseSize(QSizeF(5000, 2500));
        view.setEnsureSceneFillViewY(true);
        view.setAnimationEnabled(false);
        view.resize(900, 450);
        view.show();
        view.activateWindow();
        QTRY_VERIFY(view.isVisible());
        QVERIFY(view.setViewportScale(scale, scale));
        view.setHorizontalBarValue(1000);
        view.setVerticalBarValue(500);
    }

    void sendTimeViewWheel(TimeGraphicsView &view, QPoint position, int delta,
                           Qt::KeyboardModifiers modifiers) {
        QWheelEvent event(QPointF(position), QPointF(view.viewport()->mapToGlobal(position)), {},
                          QPoint(0, delta), Qt::NoButton, modifiers, Qt::NoScrollPhase, false);
        QApplication::sendEvent(view.viewport(), &event);
        QVERIFY(event.isAccepted());
    }
}

void EditorInteractionTests::legacyWheelZoomPreservesTheInputAnchor_data() {
    QTest::addColumn<bool>("vertical");
    QTest::newRow("time-axis") << false;
    QTest::newRow("vertical-axis") << true;
}

void EditorInteractionTests::legacyWheelZoomPreservesTheInputAnchor() {
    QFETCH(bool, vertical);
    TimeGraphicsScene scene;
    TimeGraphicsView view(&scene);
    prepareTimeView(scene, view);
    if (QTest::currentTestFailed())
        return;
    const QPoint position(view.viewport()->width() / 3, view.viewport()->height() / 3);
    const auto scale = [&] { return vertical ? view.scaleY() : view.scaleX(); };
    const auto modelAnchor = [&] {
        const auto scenePosition = view.mapToScene(position);
        return (vertical ? scenePosition.y() : scenePosition.x()) / scale();
    };
    const auto anchor = modelAnchor();
    const auto modifiers = vertical ? Qt::AltModifier : Qt::ControlModifier;
    QSignalSpy changed(&view, &TimeGraphicsView::scaleChanged);
    sendTimeViewWheel(view, position, 120, modifiers);
    QTRY_VERIFY(scale() > 1.0);
    QVERIFY(!changed.isEmpty());
    QVERIFY(std::abs(modelAnchor() - anchor) <= 1.0 / scale());
    QCOMPARE(vertical ? view.scaleX() : view.scaleY(), 1.0);
    const auto zoomed = scale();
    sendTimeViewWheel(view, position, -120, modifiers);
    QTRY_VERIFY(scale() < zoomed);
    QVERIFY(std::abs(modelAnchor() - anchor) <= 1.0 / scale());

    sendTimeViewWheel(view, position, -2400, modifiers);
    QCoreApplication::processEvents();
    if (vertical) {
        QVERIFY(view.scaleY() >= view.scaleYMin());
        QVERIFY(view.sceneRect().height() >= view.viewport()->height());
    } else {
        QVERIFY(view.sceneRect().width() >= view.viewport()->width());
    }
    const auto lowerBound = scale();
    sendTimeViewWheel(view, position, -120, modifiers);
    QCOMPARE(scale(), lowerBound);
}

void EditorInteractionTests::legacyViewportAnimationCanBeFinishedOrInterrupted_data() {
    QTest::addColumn<double>("scale");
    QTest::addColumn<bool>("pinch");
    QTest::newRow("normal-scale-wheel") << 1.0 << false;
    QTest::newRow("zoomed-pinch") << 2.0 << true;
}

void EditorInteractionTests::legacyViewportAnimationCanBeFinishedOrInterrupted() {
    QFETCH(double, scale);
    QFETCH(bool, pinch);
    TimeGraphicsScene scene;
    TimeGraphicsView view(&scene);
    prepareTimeView(scene, view, scale);
    if (QTest::currentTestFailed())
        return;
    view.setAnimationEnabled(true);
    const QRectF target(3000 * scale, 1500 * scale, 200, 100);
    const auto start = view.visibleRect();
    QVERIFY(!start.contains(target));
    view.ensureSceneRectVisible(target, 20, 20, true);
    QVERIFY(view.logicalVisibleRect().contains(target));
    QCOMPARE(view.visibleRect(), start);
    const auto destination = view.logicalVisibleRect();
    view.setAnimationEnabled(false);
    QCOMPARE(view.visibleRect(), destination);
    QCOMPARE(view.logicalVisibleRect(), destination);

    view.setAnimationEnabled(true);
    view.ensureSceneRectVisible(start.adjusted(20, 20, -20, -20), 0, 0, true);
    QVERIFY(view.logicalVisibleRect() != view.visibleRect());
    if (pinch) {
        const QPointingDevice touchpad(
            QStringLiteral("Test touchpad"), 1, QInputDevice::DeviceType::TouchPad,
            QPointingDevice::PointerType::Finger, QInputDevice::Capability::Position, 2, 0);
        const auto position = view.viewport()->rect().center();
        const auto global = view.viewport()->mapToGlobal(position);
        const auto local = view.mapFromGlobal(global);
        const auto anchor = view.mapToScene(position).x() / view.scaleX();
        QNativeGestureEvent gesture(Qt::ZoomNativeGesture, &touchpad, 2, local, local, global, 0.25,
                                    {});
        QVERIFY(QApplication::sendEvent(&view, &gesture));
        QVERIFY(view.scaleX() > scale);
        QVERIFY(std::abs(view.mapToScene(position).x() / view.scaleX() - anchor) <=
                1.0 / view.scaleX());
        QCOMPARE(view.visibleRect().top(), destination.top());
    } else {
        sendTimeViewWheel(view, view.viewport()->rect().center(), -120, Qt::NoModifier);
        QVERIFY(view.visibleRect().top() > destination.top());
    }
    if (QTest::currentTestFailed())
        return;
    view.setAnimationEnabled(false);
    QTRY_COMPARE(view.logicalVisibleRect(), view.visibleRect());
}

void EditorInteractionTests::viewportMargin() {
    EditorViewportController marginViewport;
    marginViewport.setContentTickRange(0, 20000);
    marginViewport.setLeftMarginPx(10);
    marginViewport.setContentTickRange(0, 20000);
    marginViewport.setViewportSize(QSizeF(800, 300));
    QVERIFY2((qFuzzyIsNull(marginViewport.horizontalOffset()) &&
              qFuzzyCompare(marginViewport.tickToSceneX(0), 10.0) &&
              qFuzzyCompare(marginViewport.startTick(), -75.0)),
             "initializing an RHI margin must keep it visible before tick zero");
}

void EditorInteractionTests::viewportResizeClamping() {
    EditorViewportController viewport;
    viewport.setEnsureContentFillsViewport(false, false);
    viewport.setContentTickRange(0, 20000);
    viewport.setVerticalContent(20, 72);
    viewport.setViewportSize(QSizeF(800, 300));
    viewport.scrollBy(QPointF(300.25, 400.25));
    viewport.setViewportSize(QSizeF(1000, 500));
    QVERIFY2((qFuzzyCompare(viewport.horizontalOffset(), 300.0) &&
              qFuzzyCompare(viewport.verticalOffset(), 400.0)),
             "RHI viewport offsets must use the integer coordinates exposed by legacy scrollbars");

    viewport.scrollBy(QPointF(100000, 100000));
    viewport.setViewportSize(QSizeF(1900, 1300));
    QVERIFY2((qFuzzyCompare(viewport.horizontalOffset(), 767.0) &&
              qFuzzyCompare(viewport.verticalOffset(), 140.0)),
             "resizing must clamp preserved offsets to the new scroll range");
}

void EditorInteractionTests::pianoViewportZoomAnchor() {
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
    QVERIFY2((qFuzzyCompare(pianoCenter.centerTick, 4800.0) &&
              qFuzzyCompare(pianoCenter.centerUnit, 67.5) &&
              qFuzzyCompare(PianoRollCoord::centerYToKeyIndex(pianoCenter.centerUnit, 1.0),
                            targetKeyIndex)),
             "piano-roll key centers must round-trip through the shared RHI viewport");
    pianoViewport.setScale(2.0, 3.0, QPointF(400.0, 180.0));
    const auto zoomedPianoCenter = pianoViewport.state();
    QVERIFY2((qFuzzyCompare(zoomedPianoCenter.centerTick, pianoCenter.centerTick) &&
              qFuzzyCompare(zoomedPianoCenter.centerUnit, pianoCenter.centerUnit)),
             "piano-roll zooming through the shared viewport must preserve its center anchor");
}

void EditorInteractionTests::focusReveal() {
    EditorViewportController focusViewport;
    focusViewport.setEnsureContentFillsViewport(false, false);
    focusViewport.setContentTickRange(0, 30000);
    focusViewport.setVerticalContent(20, 72);
    focusViewport.setViewportSize(QSizeF(800, 300));
    focusViewport.scrollBy(QPointF(300, 400));
    int focusViewportChanges = 0;
    QObject::connect(&focusViewport, &EditorViewportController::viewportChanged, &focusViewport,
                     [&focusViewportChanges] { ++focusViewportChanges; });
    QVERIFY2((focusViewport.ensureVisible(QRectF(500, 500, 100, 50), 24, 24) &&
              qFuzzyCompare(focusViewport.horizontalOffset(), 300.0) &&
              qFuzzyCompare(focusViewport.verticalOffset(), 400.0) && focusViewportChanges == 0),
             "revealing an already visible RHI focus must not move or notify the viewport");
    QVERIFY2((focusViewport.ensureVisible(QRectF(1050, 680, 100, 40), 24, 24) &&
              qFuzzyCompare(focusViewport.horizontalOffset(), 374.0) &&
              qFuzzyCompare(focusViewport.verticalOffset(), 444.0) && focusViewportChanges == 1),
             "revealing an obscured RHI focus must scroll only the minimum required distance");
    QVERIFY2((focusViewport.ensureVisible(QRectF(320, 420, 10, 10), 24, 24) &&
              qFuzzyCompare(focusViewport.horizontalOffset(), 296.0) &&
              qFuzzyCompare(focusViewport.verticalOffset(), 396.0) && focusViewportChanges == 2),
             "revealing toward the leading edges must preserve the requested margin");
    QVERIFY2((focusViewport.ensureVisible(QRectF(1500, 900, 100, 40), 24, 24, true) &&
              focusViewport.logicalVisibleSceneRect().topLeft() == QPointF(824, 664)),
             "an animated RHI focus reveal must publish its logical destination");
    QVERIFY2((focusViewport.ensureVisible(QRectF(1500, 900, 100, 40), 24, 24, false) &&
              qFuzzyCompare(focusViewport.horizontalOffset(), 824.0) &&
              qFuzzyCompare(focusViewport.verticalOffset(), 664.0)),
             "a non-animated RHI focus reveal must reach the same destination");
    QVERIFY2((focusViewport.setOffset(QPointF(1200, 700), true) &&
              focusViewport.logicalVisibleSceneRect().topLeft() == QPointF(1200, 700)),
             "an animated direct RHI viewport move must publish its logical destination");
    QVERIFY2((focusViewport.setOffset(QPointF(300, 400)) &&
              focusViewport.visibleSceneRect().topLeft() == QPointF(300, 400) &&
              focusViewport.logicalVisibleSceneRect().topLeft() == QPointF(300, 400)),
             "an immediate RHI viewport move must replace a pending animated destination");
}

void EditorInteractionTests::repeatedBoundaryScrollDoesNotNotify() {
    EditorViewportController boundedViewport;
    boundedViewport.setEnsureContentFillsViewport(false, false);
    boundedViewport.setContentTickRange(0, 100000);
    boundedViewport.setVerticalContent(2, 100);
    boundedViewport.setViewportSize(QSizeF(800, 300));
    int boundedViewportChanges = 0;
    QObject::connect(&boundedViewport, &EditorViewportController::viewportChanged, &boundedViewport,
                     [&boundedViewportChanges] { ++boundedViewportChanges; });
    boundedViewport.setStartTick(1000000);
    QVERIFY2((boundedViewportChanges == 1),
             "scrolling to the content boundary must notify the viewport once");
    boundedViewport.setStartTick(1000000);
    boundedViewport.scrollBy(QPointF(100, 100));
    QVERIFY2((boundedViewportChanges == 1),
             "repeated scrolling beyond a clamped boundary must not notify the viewport");
}

void EditorInteractionTests::animatedAndImmediateViewportDestinations() {
    QPointF animatedOffset(10, 20);
    EditorViewportAnimation viewportAnimation(
        [&animatedOffset](const QPointF &offset) { animatedOffset = offset; });
    viewportAnimation.setAnimationEnabled(true);
    viewportAnimation.moveTo(animatedOffset, QPointF(100, 200), true);
    QVERIFY2((viewportAnimation.isRunning() &&
              viewportAnimation.logicalOffset(animatedOffset) == QPointF(100, 200)),
             "an animated RHI viewport move must expose its logical destination immediately");
    viewportAnimation.moveTo(animatedOffset, QPointF(100, 200), false);
    QVERIFY2((!viewportAnimation.isRunning() && animatedOffset == QPointF(100, 200)),
             "a non-animated RHI viewport move must apply the same destination immediately");
}
