#include "tst_application_gui.h"

#include "../TestSupport/MainWindowFixture.h"
#include "UI/Views/MainTitleBar/MainMenuView.h"

#include <lite/GUI/Controls/Menu.h>

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QKeySequence>
#include <QMenu>
#include <QPixmap>
#include <QProxyStyle>
#include <QScopeGuard>
#include <QSizePolicy>
#include <QStyle>
#include <QToolButton>

namespace {

    void settleMenuBar() {
        for (int i = 0; i < 3; ++i) {
            QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
            QCoreApplication::processEvents();
        }
    }

    QStringList titles(const QList<QAction *> &actions) {
        QStringList result;
        for (auto *action : actions)
            result.append(QString(action->text()).remove(QLatin1Char('&')));
        return result;
    }

    // Optional visual evidence for the native style sheet; off by default.
    void saveMenuBarEvidence(QWidget &menuBar, const QString &stage) {
        const auto directory = qEnvironmentVariable("LITE_MENU_SCREENSHOT_DIR");
        if (directory.isEmpty())
            return;
        const QDir output(directory);
        if (output.mkpath(QStringLiteral(".")))
            menuBar.grab().save(output.filePath(QStringLiteral("main-menu-%1.png").arg(stage)));
    }

    struct MenuBarProbe {
        TestSupport::MainWindowFixture fixture;
        MainMenuView *menuBar = nullptr;
        QAction *overflowAction = nullptr;

        MenuBarProbe() {
            fixture.show();
            menuBar = fixture.window->findChild<MainMenuView *>();
            overflowAction = menuBar ? menuBar->overflowAction() : nullptr;
        }

        void setWidth(int width) {
            menuBar->setFixedWidth(width);
            settleMenuBar();
        }
    };

} // namespace

void ApplicationGuiTests::mainMenuOverflowsFromRightToLeft() {
    MenuBarProbe probe;
    QVERIFY(probe.menuBar);
    QVERIFY(probe.overflowAction);
    QVERIFY(probe.menuBar->overflowMenu());

    // The title bar must be allowed to shrink the menu bar. A fixed policy
    // would keep the full sizeHint and the overflow mechanism would never run.
    QVERIFY(probe.menuBar->sizePolicy().horizontalPolicy() != QSizePolicy::Fixed);
    const auto restoreWidth = qScopeGuard([&probe] {
        probe.menuBar->setMinimumWidth(0);
        probe.menuBar->setMaximumWidth(QWIDGETSIZE_MAX);
        settleMenuBar();
    });

    const auto menus = probe.menuBar->topLevelMenuActions();
    const auto allTitles = titles(menus);
    QCOMPARE(allTitles.size(), 4);
    QVERIFY(!probe.overflowAction->icon().isNull());
    QVERIFY(!probe.overflowAction->toolTip().isEmpty());
    // The button is a normal QAction, so its mnemonic opens the overflow menu
    // without any shortcut interception.
    QVERIFY(!QKeySequence::mnemonic(probe.overflowAction->text()).isEmpty());

    // Qt's private extension widget must never take over. If our measurement
    // drifts, this is the assertion that catches it.
    auto *nativeExtension =
        probe.menuBar->findChild<QToolButton *>(QStringLiteral("qt_menubar_ext_button"));

    const int fullWidth = probe.menuBar->sizeHint().width();
    // The title bar never shrinks the menu bar below its minimumSizeHint, so
    // widths below that are unreachable and would only force Qt's private
    // extension widget to appear because even the button no longer fits.
    const int minWidth = probe.menuBar->minimumSizeHint().width();
    QVERIFY(fullWidth > minWidth);
    QVERIFY(minWidth > 40);
    probe.setWidth(fullWidth);
    QVERIFY(!probe.menuBar->isOverflowing());
    QVERIFY(probe.menuBar->overflowedMenuActions().isEmpty());
    QCOMPARE(probe.menuBar->actions(), menus);
    saveMenuBarEvidence(*probe.menuBar, QStringLiteral("full"));

    QStringList firstOverflow;
    bool sawOverflow = false;
    for (int width = fullWidth - 4; width >= minWidth; width -= 4) {
        probe.setWidth(width);
        QCOMPARE(probe.menuBar->width(), width);

        const auto overflowed = probe.menuBar->overflowedMenuActions();
        // Every collapsed entry is the original menu action, not a proxy: it
        // still carries its QMenu, so the ellipsis popup shows real submenus.
        for (auto *action : overflowed)
            QVERIFY(action->menu() != nullptr);

        const auto current = titles(overflowed);
        // Collapse from the right. The popup keeps the original left-to-right
        // order, so each state is a suffix of the full list.
        QCOMPARE(current, allTitles.mid(allTitles.size() - current.size()));

        if (!current.isEmpty()) {
            QVERIFY(probe.menuBar->isOverflowing());
            QVERIFY(probe.menuBar->actions().contains(probe.overflowAction));
            QString diagnostic;
            if (nativeExtension && !nativeExtension->isHidden()) {
                diagnostic = QStringLiteral("bar=%1x%2 ")
                                 .arg(probe.menuBar->width())
                                 .arg(probe.menuBar->height());
                for (auto *action : probe.menuBar->actions()) {
                    const auto r = probe.menuBar->actionGeometry(action);
                    diagnostic += QStringLiteral("[%1 %2,%3 %4x%5] ")
                                      .arg(action->text())
                                      .arg(r.left())
                                      .arg(r.top())
                                      .arg(r.width())
                                      .arg(r.height());
                }
            }
            QVERIFY2(!nativeExtension || nativeExtension->isHidden(),
                     qPrintable(diagnostic));

            // The button is laid out as an ordinary action, so the gap before
            // it equals the gap between any two normal menu items. Leftover
            // space from collapsing a menu stays on the right instead.
            const auto buttonRect = probe.menuBar->actionGeometry(probe.overflowAction);
            QList<int> lefts;
            QList<int> rights;
            for (auto *action : menus) {
                if (overflowed.contains(action))
                    continue;
                const auto rect = probe.menuBar->actionGeometry(action);
                if (!rect.isNull()) {
                    lefts.append(rect.left());
                    rights.append(rect.right());
                }
            }
            if (!rights.isEmpty()) {
                const int gap = buttonRect.left() - rights.last();
                QVERIFY(gap >= 0);
                if (lefts.size() > 1)
                    QCOMPARE(gap, lefts.at(1) - rights.at(0));
                else
                    QVERIFY(gap <= 8);
                QVERIFY(probe.menuBar->width() - buttonRect.right() > 0);
            }
        }

        if (!current.isEmpty() && !sawOverflow) {
            sawOverflow = true;
            firstOverflow = current;
            saveMenuBarEvidence(*probe.menuBar, QStringLiteral("overflow"));
        }
        if (sawOverflow)
            QVERIFY(current.size() >= firstOverflow.size());
    }

    QVERIFY(sawOverflow);
    QCOMPARE(firstOverflow.last(), allTitles.last());
    QVERIFY(probe.menuBar->isOverflowing());
    QVERIFY(!probe.menuBar->overflowedMenuActions().isEmpty());
}

void ApplicationGuiTests::mainMenuOverflowRestoresWithoutHysteresis() {
    MenuBarProbe probe;
    QVERIFY(probe.menuBar);
    QVERIFY(probe.overflowAction);
    const auto restoreWidth = qScopeGuard([&probe] {
        probe.menuBar->setMinimumWidth(0);
        probe.menuBar->setMaximumWidth(QWIDGETSIZE_MAX);
        settleMenuBar();
    });

    const int fullWidth = probe.menuBar->sizeHint().width();
    const int minWidth = probe.menuBar->minimumSizeHint().width();
    probe.setWidth(fullWidth);
    const auto wideHint = probe.menuBar->sizeHint();
    const auto wideMinimum = probe.menuBar->minimumSizeHint();

    int threshold = -1;
    for (int width = fullWidth - 4; width >= minWidth; width -= 4) {
        probe.setWidth(width);
        if (probe.menuBar->isOverflowing()) {
            threshold = width;
            break;
        }
    }
    QVERIFY(threshold > 0);
    const auto narrowTitles = titles(probe.menuBar->overflowedMenuActions());
    // Both hints come from the full menu set, never from the current split.
    // A hint that shrinks with the split would make the title bar oscillate.
    QCOMPARE(probe.menuBar->sizeHint(), wideHint);
    QCOMPARE(probe.menuBar->minimumSizeHint(), wideMinimum);

    probe.setWidth(fullWidth);
    settleMenuBar();
    QVERIFY(!probe.menuBar->isOverflowing());
    QVERIFY(probe.menuBar->overflowedMenuActions().isEmpty());

    probe.setWidth(threshold);
    settleMenuBar();
    QCOMPARE(titles(probe.menuBar->overflowedMenuActions()), narrowTitles);
    QVERIFY(probe.menuBar->isOverflowing());
}

void ApplicationGuiTests::mainMenuOverflowRecalculatesAfterTitleChange() {
    MenuBarProbe probe;
    QVERIFY(probe.menuBar);
    QVERIFY(probe.overflowAction);
    const auto restore = qScopeGuard([&probe] {
        QEvent languageChange(QEvent::LanguageChange);
        QCoreApplication::sendEvent(probe.menuBar, &languageChange);
        probe.menuBar->setMinimumWidth(0);
        probe.menuBar->setMaximumWidth(QWIDGETSIZE_MAX);
        settleMenuBar();
    });

    const int fullWidth = probe.menuBar->sizeHint().width();
    probe.setWidth(fullWidth);
    QVERIFY(!probe.menuBar->isOverflowing());

    auto *first = probe.menuBar->topLevelMenuActions().first();
    const auto originalTitle = first->text();
    first->setText(originalTitle + QStringLiteral(" AAAAAAAAAAAAAAAA"));
    settleMenuBar();

    QVERIFY(probe.menuBar->isOverflowing());
    QVERIFY(!probe.menuBar->overflowedMenuActions().isEmpty());
    QVERIFY(!probe.overflowAction->text().isEmpty());
}

void ApplicationGuiTests::mainMenuFitsAtSizeHintWithNativeMetrics() {
    MenuBarProbe probe;
    QVERIFY(probe.menuBar);
    QVERIFY(probe.overflowAction);

    // Fusion reports 6px of PM_MenuBarItemSpacing, which absorbs a budget
    // error in sizeHint(). The Windows native style reports 0 spacing and a
    // 0 horizontal margin, so the full sizeHint must be usable as-is.
    class NativeMetricsStyle final : public QProxyStyle {
    public:
        using QProxyStyle::QProxyStyle;

        int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                        const QWidget *widget = nullptr) const override {
            switch (metric) {
            case PM_MenuBarItemSpacing:
            case PM_MenuBarHMargin:
                return 0;
            case PM_MenuBarPanelWidth:
                return 2;
            default:
                return QProxyStyle::pixelMetric(metric, option, widget);
            }
        }
    };

    probe.menuBar->setStyle(new NativeMetricsStyle);
    const auto restore = qScopeGuard([&probe] {
        probe.menuBar->setMinimumWidth(0);
        probe.menuBar->setMaximumWidth(QWIDGETSIZE_MAX);
        probe.menuBar->setStyle(nullptr);
        settleMenuBar();
    });
    settleMenuBar();
    QCOMPARE(probe.menuBar->style()->pixelMetric(QStyle::PM_MenuBarItemSpacing), 0);

    const auto menus = probe.menuBar->topLevelMenuActions();
    const auto allTitles = titles(menus);
    const int fullWidth = probe.menuBar->sizeHint().width();
    const int minWidth = probe.menuBar->minimumSizeHint().width();
    QVERIFY(fullWidth > minWidth);

    probe.setWidth(fullWidth);
    QVERIFY2(!probe.menuBar->isOverflowing(),
             qPrintable(QStringLiteral("sizeHint=%1 collapsed=%2")
                            .arg(fullWidth)
                            .arg(titles(probe.menuBar->overflowedMenuActions()).join(','))));
    QCOMPARE(probe.menuBar->actions(), menus);

    auto *nativeExtension =
        probe.menuBar->findChild<QToolButton *>(QStringLiteral("qt_menubar_ext_button"));
    QVERIFY(!nativeExtension || nativeExtension->isHidden());

    bool sawOverflow = false;
    for (int width = fullWidth - 4; width >= minWidth; width -= 4) {
        probe.setWidth(width);
        const auto overflowed = probe.menuBar->overflowedMenuActions();
        if (overflowed.isEmpty())
            continue;
        sawOverflow = true;
        QCOMPARE(titles(overflowed), allTitles.mid(allTitles.size() - overflowed.size()));
        QVERIFY(!nativeExtension || nativeExtension->isHidden());
    }
    QVERIFY(sawOverflow);
}
