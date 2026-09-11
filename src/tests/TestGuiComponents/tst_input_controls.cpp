#include "tst_gui_components.h"

#include <lite/GUI/Controls/Fader.h>
#include <lite/GUI/Controls/PanSlider.h>
#include <lite/GUI/Controls/SvsSeekbar.h>
#include <lite/GUI/Controls/OverlayScrollBar.h>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QContextMenuEvent>
#include <QCursor>
#include <QMenu>
#include <QMouseEvent>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <QtTest/QTest>

#include <type_traits>

namespace {
    void moveWithLeftButton(QWidget &widget, const QPoint &position) {
        QMouseEvent move(QEvent::MouseMove, QPointF(position),
                         QPointF(widget.mapToGlobal(position)), Qt::NoButton, Qt::LeftButton,
                         Qt::NoModifier);
        QApplication::sendEvent(&widget, &move);
    }
}

void GuiComponentTests::seekBarTrackingControlsWhenDraggedValuesCommit_data() {
    QTest::addColumn<bool>("tracking");
    QTest::newRow("live-value") << true;
    QTest::newRow("commit-on-release") << false;
}

void GuiComponentTests::seekBarTrackingControlsWhenDraggedValuesCommit() {
    QFETCH(bool, tracking);
    const auto originalCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(originalCursor); });
    SVS::SeekBar slider;
    slider.setAnimationEnabled(false);
    slider.setRange(0, 100);
    slider.setInterval(5);
    slider.setValue(50);
    slider.setTracking(tracking);
    slider.resize(220, 20);
    slider.show();
    slider.activateWindow();
    QTRY_VERIFY(slider.isVisible());
    const QPoint press(110, 10);
    const QPoint release(150, 10);
    QCursor::setPos(slider.mapToGlobal(press));
    QCoreApplication::processEvents();
    QSignalSpy moved(&slider, &SVS::SeekBar::sliderMoved);
    QSignalSpy changed(&slider, &SVS::SeekBar::valueChanged);
    const auto releaseOnFailure = qScopeGuard([&] {
        if (slider.isSliderDown())
            QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, release);
    });
    QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, press);
    QVERIFY(slider.isSliderDown());
    moveWithLeftButton(slider, release);
    QCOMPARE(slider.sliderPosition(), 70.0);
    QCOMPARE(slider.value(), tracking ? 70.0 : 50.0);
    QCOMPARE(moved.count(), 1);
    QCOMPARE(moved.first().first().toDouble(), 70.0);
    QCOMPARE(changed.count(), tracking ? 1 : 0);
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, release);
    QVERIFY(!slider.isSliderDown());
    QCOMPARE(slider.value(), 70.0);
    QCOMPARE(changed.count(), 1);
    QCOMPARE(changed.first().first().toDouble(), 70.0);
    slider.setValue(20);
    QCOMPARE(slider.value(), 20.0);
    QCOMPARE(moved.count(), 1);
    QCOMPARE(changed.count(), 2);
}

void GuiComponentTests::seekBarKeyboardStepsClampAndDoubleClickResets() {
    const auto originalCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(originalCursor); });
    SVS::SeekBar slider;
    slider.setAnimationEnabled(false);
    slider.setRange(-10, 10);
    slider.setSingleStep(2);
    slider.setPageStep(6);
    slider.setDefaultValue(2);
    slider.resize(220, 20);
    slider.show();
    slider.activateWindow();
    slider.setFocus();
    QTRY_VERIFY(slider.hasFocus());
    QSignalSpy moved(&slider, &SVS::SeekBar::sliderMoved);
    QSignalSpy changed(&slider, &SVS::SeekBar::valueChanged);
    QTest::keyClick(&slider, Qt::Key_Right);
    QCOMPARE(slider.value(), 2.0);
    QTest::keyClick(&slider, Qt::Key_Up);
    QCOMPARE(slider.value(), 4.0);
    QTest::keyClick(&slider, Qt::Key_Left);
    QCOMPARE(slider.value(), 2.0);
    QTest::keyClick(&slider, Qt::Key_PageUp);
    QCOMPARE(slider.value(), 8.0);
    QTest::keyClick(&slider, Qt::Key_PageDown);
    QCOMPARE(slider.value(), 2.0);
    QTest::keyClick(&slider, Qt::Key_End);
    QCOMPARE(slider.value(), 10.0);
    const auto atMaximum = changed.count();
    QTest::keyClick(&slider, Qt::Key_Right);
    QCOMPARE(slider.value(), 10.0);
    QCOMPARE(changed.count(), atMaximum);
    QTest::keyClick(&slider, Qt::Key_Home);
    QCOMPARE(slider.value(), -10.0);
    const auto atMinimum = changed.count();
    QTest::keyClick(&slider, Qt::Key_Down);
    QCOMPARE(slider.value(), -10.0);
    QCOMPARE(changed.count(), atMinimum);
    QVERIFY(moved.isEmpty());
    slider.setValue(0);
    const auto beforeReset = changed.count();
    const QPoint center(110, 10);
    QCursor::setPos(slider.mapToGlobal(center));
    QCoreApplication::processEvents();
    // The QWidget overload sends only the double-click event, so supply the first click.
    QTest::mouseClick(&slider, Qt::LeftButton, Qt::NoModifier, center);
    QCOMPARE(slider.value(), 0.0);
    QCOMPARE(changed.count(), beforeReset);
    QTest::mouseDClick(&slider, Qt::LeftButton, Qt::NoModifier, center);
    QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, center);
    QCOMPARE(slider.value(), 2.0);
    QCOMPARE(changed.count(), beforeReset + 1);
    QVERIFY(!slider.isSliderDown());
}

void GuiComponentTests::mixerSliderReleaseEndsPreview_data() {
    QTest::addColumn<bool>("vertical");
    QTest::newRow("gain-fader") << true;
    QTest::newRow("pan-slider") << false;
}

void GuiComponentTests::mixerSliderReleaseEndsPreview() {
    QFETCH(bool, vertical);
    const auto originalCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(originalCursor); });
    const auto exerciseDrag = [](auto &slider, const QPoint &press, const QPoint &release,
                                 const double externalValue) {
        using Slider = std::remove_reference_t<decltype(slider)>;
        slider.show();
        slider.activateWindow();
        QTRY_VERIFY(slider.isVisible());
        QCursor::setPos(slider.mapToGlobal(press));
        QCoreApplication::processEvents();
        QSignalSpy moved(&slider, &Slider::sliderMoved);
        QSignalSpy changed(&slider, &Slider::valueChanged);
        bool released = false;
        const auto releaseOnFailure = qScopeGuard([&] {
            if (!released)
                QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, release);
        });
        const auto initial = slider.value();
        QTest::mousePress(&slider, Qt::LeftButton, Qt::NoModifier, press);
        QCoreApplication::processEvents();
        // The controls warp the cursor to the thumb and ignore that synthetic move.
        moveWithLeftButton(slider, slider.mapFromGlobal(QCursor::pos()));
        moveWithLeftButton(slider, release);
        QVERIFY(!moved.isEmpty());
        QVERIFY(changed.isEmpty());
        const auto preview = slider.sliderPosition();
        QVERIFY(preview > initial);
        QCOMPARE(moved.last().first().toDouble(), preview);
        QTest::mouseRelease(&slider, Qt::LeftButton, Qt::NoModifier, release);
        released = true;
        QCOMPARE(slider.value(), preview);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(changed.first().first().toDouble(), preview);
        const auto completedMoves = moved.count();
        slider.setValue(externalValue);
        QCOMPARE(slider.value(), externalValue);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(changed.last().first().toDouble(), externalValue);
        QCOMPARE(moved.count(), completedMoves);
    };
    if (vertical) {
        Fader slider;
        slider.resize(64, 220);
        slider.setValue(-54);
        exerciseDrag(slider, QPoint(32, 199), QPoint(32, 120), -12);
    } else {
        PanSlider slider;
        slider.resize(220, 24);
        exerciseDrag(slider, QPoint(110, 12), QPoint(160, 12), -0.3);
    }
}

void GuiComponentTests::overlayScrollMenuNavigatesTheAttachedArea_data() {
    QTest::addColumn<bool>("horizontal");
    QTest::newRow("horizontal") << true;
    QTest::newRow("vertical") << false;
}

void GuiComponentTests::overlayScrollMenuNavigatesTheAttachedArea() {
    QFETCH(bool, horizontal);
    const auto originalCursor = QCursor::pos();
    const auto restoreCursor = qScopeGuard([&] { QCursor::setPos(originalCursor); });
    QAbstractScrollArea area;
    area.resize(420, 420);
    auto *source = horizontal ? area.horizontalScrollBar() : area.verticalScrollBar();
    source->setRange(10, 1010);
    source->setPageStep(100);
    source->setSingleStep(5);
    auto *bar = OverlayScrollBar::install(&area, horizontal ? Qt::Horizontal : Qt::Vertical);
    auto *style = QStyleFactory::create("Fusion");
    QVERIFY(style);
    style->setParent(bar);
    bar->setStyle(style);
    bar->setAnimationEnabled(false);
    source->setValue(510);
    area.show();
    area.activateWindow();
    QTRY_VERIFY(bar->isVisible());
    QCOMPARE(bar->value(), source->value());
    const auto choose = [&](const QString &label) {
        bool used = false;
        QTimer select;
        select.setInterval(10);
        connect(&select, &QTimer::timeout, bar, [&] {
            auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
            if (!menu)
                return;
            select.stop();
            const auto close = qScopeGuard([&] { menu->close(); });
            if (label.isEmpty()) {
                QTest::keyClick(menu, Qt::Key_Escape);
            } else {
                QAction *choice = nullptr;
                for (auto *action : menu->actions()) {
                    if (action->text() == label)
                        choice = action;
                }
                QVERIFY(choice && choice->isEnabled());
                QVERIFY(menu->windowHandle());
                const auto position = menu->actionGeometry(choice).center();
                QTest::mouseMove(menu->windowHandle(), position);
                QTRY_COMPARE(menu->activeAction(), choice);
                QSignalSpy activated(choice, &QAction::triggered);
                QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, position);
                QTRY_COMPARE(activated.size(), 1);
            }
            used = true;
        });
        const auto position = bar->rect().center();
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, bar->mapToGlobal(position));
        select.start();
        QApplication::sendEvent(bar, &event);
        QVERIFY(used);
        QVERIFY(event.isAccepted());
        QCOMPARE(bar->value(), source->value());
    };
    choose({});
    QCOMPARE(source->value(), 510);
    choose(OverlayScrollBar::tr(horizontal ? "Right edge" : "Bottom"));
    QCOMPARE(source->value(), 1010);
    choose(OverlayScrollBar::tr(horizontal ? "Page left" : "Page up"));
    QCOMPARE(source->value(), 910);
    choose(OverlayScrollBar::tr(horizontal ? "Scroll left" : "Scroll up"));
    QCOMPARE(source->value(), 905);
    choose(OverlayScrollBar::tr("Scroll here"));
    QVERIFY(source->value() > 400 && source->value() < 650);
}
