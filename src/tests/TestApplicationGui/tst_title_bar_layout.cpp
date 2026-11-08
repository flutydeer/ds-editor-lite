#include "tst_application_gui.h"

#include "../TestSupport/MainWindowFixture.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/MainTitleBar/MainTitleBar.h"
#include "UI/Views/MainTitleBar/TitleBarComboBox.h"
#include "UI/Views/MainTitleBar/TitleBarLayoutPlan.h"

#include <lite/GUI/Controls/DividerLine.h>

#include <QAction>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLayoutItem>

namespace {
    void settleLayout() {
        for (int i = 0; i < 3; ++i) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QCoreApplication::processEvents();
        }
    }

    QWidget *firstSection(const MainTitleBar &titleBar) {
        const auto *layout = titleBar.layout();
        const auto *item = layout ? layout->itemAt(0) : nullptr;
        return item ? item->widget() : nullptr;
    }
} // namespace

void ApplicationGuiTests::titleBarLayoutPlanFollowsPlatformChrome() {
    // Frameless window: the app icon inset and the custom system buttons both
    // belong to the chrome, and the menu bar is drawn in the title bar.
    const auto frameless = TitleBarLayoutPlan::forChrome(false, true);
    QVERIFY(frameless.appIconInset);
    QVERIFY(frameless.inWindowMenu);
    QVERIFY(frameless.systemButtons);
    QVERIFY(frameless.projectSelector);
    QVERIFY(frameless.documentActions);
    QVERIFY(frameless.playback);

    // Native frame with a platform menu bar: no leading inset, no system
    // buttons, and the menu section is gone.
    const auto nativeFrame = TitleBarLayoutPlan::forChrome(true, false);
    QVERIFY(!nativeFrame.appIconInset);
    QVERIFY(!nativeFrame.inWindowMenu);
    QVERIFY(!nativeFrame.systemButtons);

    // Only the sections differ. The spacing contract stays platform independent.
    QCOMPARE(frameless.leftMargin, nativeFrame.leftMargin);
    QVERIFY(frameless != nativeFrame);
    QVERIFY(TitleBarLayoutPlan::forChrome(true, true) != nativeFrame);
}

void ApplicationGuiTests::titleBarDropsTheLeadingDividerWhenTheMenuLeavesTheWindow() {
    TestSupport::MainWindowFixture fixture;
    fixture.show();

    auto *titleBar = fixture.window->findChild<MainTitleBar *>();
    auto *menu = fixture.window->findChild<MainMenuView *>();
    QVERIFY(titleBar);
    QVERIFY(menu);
    auto *combo = titleBar->titleComboBox();
    QVERIFY(combo);
    settleLayout();

    // The fixture keeps the menu bar in the window: the leading cluster is
    // menu | combo | actions, separated by two dividers.
    QVERIFY(titleBar->layoutPlan().inWindowMenu);
    QCOMPARE(titleBar->dividers().size(), 2);
    for (const auto *divider : titleBar->dividers())
        QVERIFY(!divider->isHidden());
    QVERIFY(menu->isVisible());
    QVERIFY(firstSection(*titleBar) == menu);
    QVERIFY(titleBar->dividers().first()->x() >= menu->geometry().right());

    // A platform that owns the menu bar removes the section. The divider that
    // used to separate the menu from the project selector has to go with it,
    // otherwise it is left alone at the leading edge.
    auto hiddenMenuPlan = titleBar->layoutPlan();
    hiddenMenuPlan.inWindowMenu = false;
    titleBar->setLayoutPlan(hiddenMenuPlan);
    settleLayout();

    QVERIFY(!titleBar->layoutPlan().inWindowMenu);
    QVERIFY(!menu->isVisible());
    QCOMPARE(titleBar->dividers().size(), 1);
    QVERIFY(firstSection(*titleBar) == combo);
    QVERIFY(titleBar->dividers().first()->x() > combo->x());
    QVERIFY(combo->x() >= titleBar->layoutPlan().leftMargin);

    // Restoring the section rebuilds the same chrome as before, so the toggle
    // is symmetric and safe to expose to the developer options page.
    hiddenMenuPlan.inWindowMenu = true;
    titleBar->setLayoutPlan(hiddenMenuPlan);
    settleLayout();

    QVERIFY(titleBar->layoutPlan().inWindowMenu);
    QVERIFY(menu->isVisible());
    QCOMPARE(titleBar->dividers().size(), 2);
    QVERIFY(firstSection(*titleBar) == menu);
}

void ApplicationGuiTests::hiddenMenuBarKeepsEveryTopLevelMenuInline() {
    TestSupport::MainWindowFixture fixture;
    fixture.show();

    auto *menu = fixture.window->findChild<MainMenuView *>();
    QVERIFY(menu);
    settleLayout();

    const auto menus = menu->topLevelMenuActions();
    QCOMPARE(menus.size(), 4);
    QCOMPARE(menu->actions(), menus);

    // An in-window menu bar collapses from the right once it loses width.
    menu->setFixedWidth(0);
    settleLayout();
    QVERIFY(menu->isOverflowing());
    QVERIFY(!menu->overflowedMenuActions().isEmpty());

    // Without a width budget of its own (a platform menu bar, or the hidden
    // preview) the split must not run at all: removeAction reaches
    // QMenuBar::actionEvent, which drops the menu from the platform menu bar.
    menu->setInWindowMenu(false);
    settleLayout();

    QVERIFY(!menu->isOverflowing());
    QCOMPARE(menu->actions(), menus);
    QVERIFY(menu->overflowedMenuActions().isEmpty());
}
