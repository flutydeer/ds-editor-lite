#include "tst_gui_components.h"

#include <lite/GUI/Controls/ToolTip.h>

#include <QApplication>
#include <QLayout>
#include <QScreen>
#include <QtTest/QTest>

namespace {
    QRect visibleCard(const ToolTip &toolTip) {
        const auto margins = toolTip.layout()->contentsMargins();
        return toolTip.geometry().adjusted(margins.left(), margins.top(), -margins.right(),
                                           -margins.bottom());
    }

    bool closeEnough(int actual, int expected) {
        return qAbs(actual - expected) <= 1;
    }
}

void GuiComponentTests::tooltipPointerAnchorsFollowContentAndVisibility_data() {
    QTest::addColumn<bool>("flipAtTop");
    QTest::newRow("placement-content-and-visibility") << false;
    QTest::newRow("flip-below-at-top-edge") << true;
}

void GuiComponentTests::tooltipPointerAnchorsFollowContentAndVisibility() {
    QFETCH(bool, flipAtTop);
    const auto *screen = QApplication::primaryScreen();
    if (!screen)
        QSKIP("Pointer placement requires a screen");
    const auto available = screen->availableGeometry();
    ToolTip toolTip(QStringLiteral("123 ms"));
    toolTip.setAttribute(Qt::WA_DontShowOnScreen);
    toolTip.setAnimationEnabled(false);
    const auto clearance = ToolTip::pointerClearance(screen);
    QVERIFY(clearance >= 24);
    QCOMPARE(clearance, ToolTip::pointerClearance(nullptr));
    toolTip.showAbovePointer(available.center());
    if (flipAtTop) {
        if (available.height() < visibleCard(toolTip).height() * 3)
            QSKIP("The screen is too short to verify the below-pointer fallback");
        const QPoint pointer(available.center().x(), available.top() + 2);
        toolTip.showAbovePointer(pointer);
        const auto card = visibleCard(toolTip);
        QVERIFY(card.top() >= pointer.y());
        QVERIFY(card.bottom() <= available.bottom());
        return;
    }

    auto pointer = available.center();
    auto card = visibleCard(toolTip);
    QVERIFY(closeEnough(card.center().x(), pointer.x()));
    QVERIFY(closeEnough(pointer.y() - card.bottom(), clearance));
    QVERIFY(card.bottom() < pointer.y());
    toolTip.setTitle(QStringLiteral("1234 ms"));
    toolTip.moveAbovePointer(pointer);
    card = visibleCard(toolTip);
    QVERIFY(closeEnough(card.center().x(), pointer.x()));
    QVERIFY(closeEnough(pointer.y() - card.bottom(), clearance));

    toolTip.setTitle(QStringLiteral("12345678 ms"));
    toolTip.moveAbovePointer(pointer);
    const auto wideCard = visibleCard(toolTip);
    toolTip.setTitle(QStringLiteral("5 ms"));
    toolTip.moveAbovePointer(pointer);
    QVERIFY(visibleCard(toolTip).width() < wideCard.width());
    QVERIFY(closeEnough(visibleCard(toolTip).center().x(), pointer.x()));

    pointer = QPoint(available.left() + 2, available.center().y());
    toolTip.showAbovePointer(pointer);
    QVERIFY(visibleCard(toolTip).left() >= available.left());
    toolTip.hide();
    toolTip.setWindowOpacity(0);
    toolTip.showAbovePointer(available.center());
    QVERIFY(toolTip.isVisible());
    QVERIFY(qFuzzyCompare(toolTip.windowOpacity(), 1.0));

    toolTip.hide();
    toolTip.setWindowOpacity(0);
    const auto beforeMove = toolTip.geometry().topLeft();
    pointer = available.center() + QPoint(40, 0);
    toolTip.moveAbovePointer(pointer);
    QVERIFY(toolTip.geometry().topLeft() != beforeMove);
    QVERIFY(!toolTip.isVisible());
    QVERIFY(qFuzzyCompare(toolTip.windowOpacity(), 0.0));
    QVERIFY(closeEnough(visibleCard(toolTip).center().x(), pointer.x()));

    const QRect anchor(available.center() - QPoint(20, 10), QSize(40, 20));
    toolTip.showAbove(anchor);
    card = visibleCard(toolTip);
    QVERIFY(closeEnough(anchor.top() - card.bottom(), 4));
    QVERIFY(closeEnough(card.center().x(), anchor.center().x()));
    pointer = available.center();
    toolTip.showAbovePointer(pointer, nullptr);
    card = visibleCard(toolTip);
    QVERIFY(closeEnough(card.center().x(), pointer.x()));
    QVERIFY(closeEnough(pointer.y() - card.bottom(), clearance));
}
