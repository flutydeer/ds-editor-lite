#include "tst_application_gui.h"

#include "../TestSupport/MainWindowFixture.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"
#include "UI/Views/MainTitleBar/MainTitleBar.h"
#include "UI/Views/MainTitleBar/TitleBarComboBox.h"
#include "UI/Dialogs/Base/DialogTitleBar.h"
#include "UI/Dialogs/Base/MessageDialog.h"

#include <lite/GUI/Controls/DividerLine.h>

#include <QAction>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QScopeGuard>
#include <QWindow>
#include <QtTest/QTest>

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

void ApplicationGuiTests::dialogTitleBarFollowsFrameMode_data() {
    QTest::addColumn<bool>("storedFrame");
    QTest::addColumn<int>("forcedFrame");
    QTest::newRow("stored-native") << true << -1;
    QTest::newRow("stored-frameless") << false << -1;
    QTest::newRow("capture-native") << false << 1;
    QTest::newRow("capture-frameless") << true << 0;
}

void ApplicationGuiTests::dialogTitleBarFollowsFrameMode() {
    QFETCH(bool, storedFrame);
    QFETCH(int, forcedFrame);
    auto *appearance = appOptions->appearance();
    const auto previousFrame = appearance->useNativeFrame;
    const auto previousOverride = appearance->nativeFrameOverride;
    const auto restore = qScopeGuard([&] {
        appearance->useNativeFrame = previousFrame;
        appearance->nativeFrameOverride = previousOverride;
    });
    appearance->useNativeFrame = storedFrame;
    appearance->nativeFrameOverride = forcedFrame < 0 ? std::nullopt
                                                     : std::optional<bool>(forcedFrame != 0);
    const bool nativeFrame = appearance->effectiveUseNativeFrame();
    MessageDialog dialog(QStringLiteral("Window frame test"), QStringLiteral("Dialog content"));
    dialog.show();
    QVERIFY(QTest::qWaitForWindowExposed(&dialog));
    settleLayout();
    auto *titleBar = dialog.findChild<DialogTitleBar *>();
    QCOMPARE(titleBar == nullptr, nativeFrame);
#ifdef Q_OS_MAC
    QCOMPARE(dialog.windowFlags().testFlag(Qt::ExpandedClientAreaHint), !nativeFrame);
    QCOMPARE(dialog.windowFlags().testFlag(Qt::NoTitleBarBackgroundHint), !nativeFrame);
    QCOMPARE(dialog.testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea), nativeFrame);
    if (!nativeFrame) {
        QTRY_COMPARE(dialog.contentsMargins().top(), 0);
        QCOMPARE(dialog.contentsRect().top(), 0);
        QTRY_COMPARE(titleBar->geometry().top(), 0);
        QVERIFY(dialog.windowHandle());
        QTRY_COMPARE(dialog.windowHandle()->frameMargins().top(), 0);
    }
#endif
    // The runtime capture selection must never rewrite the user's preference.
    QCOMPARE(appearance->useNativeFrame, storedFrame);
}

void ApplicationGuiTests::titleBarLayoutPlanFollowsPlatformChrome() {
    // Windows frameless: the app icon inset and the custom system buttons both
    // belong to the chrome, and the menu bar is drawn in the title bar.
    const auto frameless = TitleBarSections::forMainWindow(false, ChromePlatform::Windows);
    QVERIFY(frameless.appIconInset);
    QVERIFY(frameless.mainMenu);
    QVERIFY(frameless.systemButtons);
    QVERIFY(!frameless.trafficLightArea);
    QVERIFY(frameless.projectSelector);
    QVERIFY(frameless.documentActions);
    QVERIFY(frameless.playback);

    // macOS frameless: the platform keeps its traffic lights, so the leading
    // slot is theirs and no Windows-style button may be drawn next to them.
    const auto macFrameless = TitleBarSections::forMainWindow(false, ChromePlatform::MacOS);
    QVERIFY(!macFrameless.appIconInset);
    QVERIFY(!macFrameless.systemButtons);
    QVERIFY(macFrameless.trafficLightArea);
    // The lights sit against the window edge, so the title bar starts flush.
    QCOMPARE(macFrameless.leftMargin, 0);

    // Native frame on either platform: no leading slot, no custom buttons.
    const auto nativeFrame = TitleBarSections::forMainWindow(true, ChromePlatform::Windows);
    QVERIFY(!nativeFrame.appIconInset);
    QVERIFY(!nativeFrame.systemButtons);
    QVERIFY(!nativeFrame.trafficLightArea);

    const auto macNativeFrame = TitleBarSections::forMainWindow(true, ChromePlatform::MacOS);
    QVERIFY(!macNativeFrame.appIconInset);
    QVERIFY(!macNativeFrame.systemButtons);
    QVERIFY(!macNativeFrame.trafficLightArea);
    QCOMPARE(macNativeFrame, nativeFrame);

    QVERIFY(frameless != macFrameless);
    QVERIFY(frameless != nativeFrame);
}

void ApplicationGuiTests::titleBarLayoutFollowsThePlatformMenuBar() {
    TestSupport::MainWindowFixture fixture(
        TestSupport::MainWindowFixture::MenuPlacement::PlatformDefault);
    fixture.show();

    auto *titleBar = fixture.window->findChild<MainTitleBar *>();
    auto *menu = fixture.window->findChild<MainMenuView *>();
    QVERIFY(titleBar);
    QVERIFY(menu);
    settleLayout();
#ifdef Q_OS_MAC
    QVERIFY(menu->isNativeMenuBar());
#endif
    const bool inWindow = !menu->isNativeMenuBar();
    QCOMPARE(titleBar->sections().mainMenu, inWindow);
    QCOMPARE(titleBar->dividers().size(), inWindow ? 2 : 1);
    QCOMPARE(firstSection(*titleBar),
             inWindow ? static_cast<QWidget *>(menu) : titleBar->titleComboBox());
    QCOMPARE(menu->topLevelMenuActions().size(), 4);

    if (!inWindow) {
        // A global menu must retain every top-level action, regardless of widget width.
        menu->setFixedWidth(0);
        settleLayout();
        QVERIFY(!menu->isOverflowing());
        QVERIFY(menu->overflowedMenuActions().isEmpty());
        QCOMPARE(menu->actions(), menu->topLevelMenuActions());
    }
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
    QVERIFY(titleBar->sections().mainMenu);
    QVERIFY(!titleBar->trafficLightArea());
    QCOMPARE(titleBar->dividers().size(), 2);
    for (const auto *divider : titleBar->dividers())
        QVERIFY(!divider->isHidden());
    QVERIFY(menu->isVisible());
    QVERIFY(firstSection(*titleBar) == menu);
    QVERIFY(titleBar->dividers().first()->x() >= menu->geometry().right());

    // A platform that owns the menu bar removes the section. The divider that
    // used to separate the menu from the project selector has to go with it,
    // otherwise it is left alone at the leading edge.
    titleBar->setInWindowMenu(false);
    settleLayout();

    QVERIFY(!titleBar->sections().mainMenu);
    QVERIFY(!menu->isVisible());
    QCOMPARE(titleBar->dividers().size(), 1);
    QVERIFY(firstSection(*titleBar) == combo);
    QVERIFY(titleBar->dividers().first()->x() > combo->x());
    QVERIFY(combo->x() >= titleBar->sections().leftMargin);

    // Restoring the section rebuilds the same chrome as before, so the toggle
    // is symmetric and safe to expose to the developer options page.
    titleBar->setInWindowMenu(true);
    settleLayout();

    QVERIFY(titleBar->sections().mainMenu);
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

void ApplicationGuiTests::titleBarDoubleClickOnTheEmptyBackgroundZoomsTheWindow() {
    TestSupport::MainWindowFixture fixture;
    fixture.show();

    auto *titleBar = fixture.window->findChild<MainTitleBar *>();
    QVERIFY(titleBar);
    settleLayout();

    // Any pixel of the bar not covered by a child counts as background. Narrow
    // windows can squeeze the stretch between the actions and the playback
    // controls down to nothing, so search instead of assuming a fixed spot.
    const auto emptyPoint = [&] {
        const int y = titleBar->height() / 2;
        for (int x = 0; x < titleBar->width(); ++x) {
            if (!titleBar->childAt(QPoint(x, y)))
                return QPoint(x, y);
        }
        return QPoint(-1, -1);
    };

    QVERIFY(!fixture.window->isMaximized());
    const auto point = emptyPoint();
    QVERIFY(point.x() >= 0);
    // A native frame has no window agent to turn a double click on the title bar
    // into a zoom, and the platform title bar is another strip above this row,
    // so the row itself has to provide the gesture.
    QTest::mouseDClick(titleBar, Qt::LeftButton, Qt::NoModifier, point);
    QTRY_VERIFY(fixture.window->isMaximized());

    settleLayout();
    const auto restorePoint = emptyPoint();
    QVERIFY(restorePoint.x() >= 0);
    QTest::mouseDClick(titleBar, Qt::LeftButton, Qt::NoModifier, restorePoint);
    QTRY_VERIFY(!fixture.window->isMaximized());
}
