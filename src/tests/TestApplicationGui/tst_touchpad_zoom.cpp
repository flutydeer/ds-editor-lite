#include "tst_application_gui.h"

#include "UI/Views/ClipEditor/PianoRoll/PianoRollGraphicsView.h"

#include <QApplication>
#include <QNativeGestureEvent>
#include <QTouchEvent>
#include <QScopeGuard>
#include <QTest>

void ApplicationGuiTests::pianoTouchpadInterruptedPinchDropsPendingZoom_data() {
    QTest::addColumn<QEvent::Type>("interruption");
    QTest::newRow("touch-cancel") << QEvent::TouchCancel;
    QTest::newRow("deactivate") << QEvent::WindowDeactivate;
    QTest::newRow("hide") << QEvent::Hide;
}

void ApplicationGuiTests::pianoTouchpadInterruptedPinchDropsPendingZoom() {
    QFETCH(QEvent::Type, interruption);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    const auto initialX = view->scaleX();
    const auto initialY = view->scaleY();
    auto *device = QTest::createTouchDevice(QInputDevice::DeviceType::TouchPad,
                                          QInputDevice::Capability::Position |
                                              QInputDevice::Capability::MouseEmulation);
    auto touch = QTest::touchEvent(view->viewport(), device, false);
    const QPoint first(250, 120);
    const QPoint second(450, 320);
    const auto global = view->viewport()->mapToGlobal(first);
    const auto local = view->mapFromGlobal(global);
    const auto sendNative = [&](Qt::NativeGestureType type, double value = 0.0) {
        QNativeGestureEvent event(type, device, 2, local, local, global, value, {});
        QVERIFY(QApplication::sendEvent(view.get(), &event));
    };
    const auto cleanup = qScopeGuard([&] {
        QTouchEvent cancel(QEvent::TouchCancel, device);
        QApplication::sendEvent(view->viewport(), &cancel);
    });
    touch.press(0, first).press(1, second).commit();
    sendNative(Qt::BeginNativeGesture);
    QTest::qWait(20);
    sendNative(Qt::ZoomNativeGesture, 0.2);
    QCOMPARE(view->scaleX(), initialX);
    QCOMPARE(view->scaleY(), initialY);
    if (interruption == QEvent::TouchCancel) {
        QTouchEvent cancel(QEvent::TouchCancel, device);
        QApplication::sendEvent(view->viewport(), &cancel);
    } else if (interruption == QEvent::Hide) {
        view->hide();
        view->show();
        QVERIFY(QTest::qWaitForWindowExposed(view.get()));
    } else {
        QEvent deactivate(QEvent::WindowDeactivate);
        QApplication::sendEvent(view.get(), &deactivate);
    }
    touch.release(0, first).release(1, second).commit();
    sendNative(Qt::EndNativeGesture);
    QTest::qWait(40);
    QCOMPARE(view->scaleX(), initialX);
    QCOMPARE(view->scaleY(), initialY);

    touch.press(0, first).press(1, second).commit();
    sendNative(Qt::BeginNativeGesture);
    touch.move(0, first - QPoint(0, 40)).move(1, second + QPoint(0, 40)).commit();
    sendNative(Qt::ZoomNativeGesture, 0.25);
    QCOMPARE(view->scaleX(), initialX);
    QCOMPARE(view->scaleY(), initialY * 1.25);
    touch.release(0, first - QPoint(0, 40)).release(1, second + QPoint(0, 40)).commit();
    sendNative(Qt::EndNativeGesture);
}

void ApplicationGuiTests::pianoTouchpadPinchUsesTouchAxisLock_data() {
    QTest::addColumn<Qt::Orientations>("axes");
    QTest::addColumn<bool>("contactsFirst");
    for (const bool contactsFirst : {false, true}) {
        const auto suffix = contactsFirst ? "contacts-first" : "native-first";
        QTest::newRow(qPrintable(QStringLiteral("horizontal-%1").arg(suffix)))
            << Qt::Orientations(Qt::Horizontal) << contactsFirst;
        QTest::newRow(qPrintable(QStringLiteral("vertical-%1").arg(suffix)))
            << Qt::Orientations(Qt::Vertical) << contactsFirst;
        QTest::newRow(qPrintable(QStringLiteral("diagonal-%1").arg(suffix)))
            << (Qt::Horizontal | Qt::Vertical) << contactsFirst;
    }
}

void ApplicationGuiTests::pianoTouchpadPinchUsesTouchAxisLock() {
    QFETCH(Qt::Orientations, axes);
    QFETCH(bool, contactsFirst);
    createPianoRoll();
    if (QTest::currentTestFailed())
        return;
    auto &view = *this->view;
    const auto initialX = view.scaleX();
    const auto initialY = view.scaleY();
    auto *device = QTest::createTouchDevice(QInputDevice::DeviceType::TouchPad,
                                          QInputDevice::Capability::Position |
                                              QInputDevice::Capability::MouseEmulation);
    auto touch = QTest::touchEvent(view.viewport(), device, false);
    const auto cleanup = qScopeGuard([&] {
        QTouchEvent cancel(QEvent::TouchCancel, device);
        QApplication::sendEvent(view.viewport(), &cancel);
    });
    const QPoint first(250, 120);
    const QPoint second(450, 320);
    const QPoint delta(axes.testFlag(Qt::Horizontal) ? 40 : 0,
                       axes.testFlag(Qt::Vertical) ? 40 : 0);
    const auto position = view.viewport()->rect().center();
    const auto global = view.viewport()->mapToGlobal(position);
    const auto local = view.mapFromGlobal(global);
    const auto scenePosition = view.mapToScene(position);
    const QPointF anchor(scenePosition.x() / initialX, scenePosition.y() / initialY);
    const auto sendNative = [&](Qt::NativeGestureType type, double value = 0.0) {
        QNativeGestureEvent event(type, device, 2, local, local, global, value, {});
        QVERIFY(QApplication::sendEvent(&view, &event));
    };
    if (!contactsFirst)
        sendNative(Qt::BeginNativeGesture);
    touch.press(0, first).commit();
    touch.stationary(0).press(1, second).commit();
    if (contactsFirst)
        sendNative(Qt::BeginNativeGesture);

    sendNative(Qt::ZoomNativeGesture, 0.1);
    QCOMPARE(view.scaleX(), initialX);
    QCOMPARE(view.scaleY(), initialY);
    touch.move(0, first - delta).move(1, second + delta).commit();
    const auto checkScale = [&](double factor) {
        const auto expectedX = initialX * (axes.testFlag(Qt::Horizontal) ? factor : 1.0);
        const auto expectedY = initialY * (axes.testFlag(Qt::Vertical) ? factor : 1.0);
        QVERIFY(qAbs(view.scaleX() - expectedX) < 1e-9);
        QVERIFY(qAbs(view.scaleY() - expectedY) < 1e-9);
    };
    checkScale(1.1);
    QTest::qWait(20);
    sendNative(Qt::ZoomNativeGesture, 0.25);
    checkScale(1.375);
    QVERIFY(qAbs(view.mapToScene(position).x() / view.scaleX() - anchor.x()) <= 1.0);
    QVERIFY(qAbs(view.mapToScene(position).y() / view.scaleY() - anchor.y()) <= 1.0);

    // Later motion on both axes cannot change the decision within this pinch.
    const auto lastFirst = first - QPoint(60, 60);
    const auto lastSecond = second + QPoint(60, 60);
    touch.move(0, lastFirst).move(1, lastSecond).commit();
    checkScale(1.375);
    QTest::qWait(20);
    sendNative(Qt::ZoomNativeGesture, 0.2);
    checkScale(1.65);
    const auto lift = [&] {
        touch.stationary(0).release(1, lastSecond).commit();
        touch.release(0, lastFirst).commit();
    };
    if (contactsFirst)
        lift();
    sendNative(Qt::EndNativeGesture);
    if (!contactsFirst)
        lift();
    const auto releasedX = view.scaleX();
    const auto releasedY = view.scaleY();
    QTRY_VERIFY(view.scaleX() > releasedX || view.scaleY() > releasedY);
    if (!axes.testFlag(Qt::Horizontal))
        QCOMPARE(view.scaleX(), initialX);
    if (!axes.testFlag(Qt::Vertical))
        QCOMPARE(view.scaleY(), initialY);

    sendNative(Qt::BeginNativeGesture);
    const auto caughtX = view.scaleX();
    const auto caughtY = view.scaleY();
    QTest::qWait(40);
    QCOMPARE(view.scaleX(), caughtX);
    QCOMPARE(view.scaleY(), caughtY);
    touch.press(0, first).press(1, second).commit();
    const bool nextIsHorizontal = !axes.testFlag(Qt::Horizontal);
    const QPoint nextDelta(nextIsHorizontal ? 40 : 0, nextIsHorizontal ? 0 : 40);
    touch.move(0, first - nextDelta).move(1, second + nextDelta).commit();
    sendNative(Qt::ZoomNativeGesture, 0.25);
    QVERIFY(qAbs(view.scaleX() - caughtX * (nextIsHorizontal ? 1.25 : 1.0)) < 1e-9);
    QVERIFY(qAbs(view.scaleY() - caughtY * (nextIsHorizontal ? 1.0 : 1.25)) < 1e-9);
    touch.release(0, first - nextDelta).release(1, second + nextDelta).commit();
    sendNative(Qt::EndNativeGesture);
}
