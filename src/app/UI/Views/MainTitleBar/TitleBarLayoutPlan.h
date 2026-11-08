#ifndef TITLEBARLAYOUTPLAN_H
#define TITLEBARLAYOUTPLAN_H

#include <QtGlobal>

// Which sections the main title bar is built from.
//
// The builder in MainTitleBar walks this plan in a fixed order and inserts a
// divider only between two present sections, so a platform or an option that
// drops a section can never leave an orphan divider at the leading edge. That
// is exactly what macOS did: QMenuBar hides itself once the platform owns the
// menu bar (qmenubar.cpp, QMenuBarPrivate::init), the menu section collapsed to
// zero width, and the divider that used to separate it from the project
// selector stayed behind at the far left.
enum class ChromePlatform {
    Windows,
    MacOS,
    Other,
};

struct TitleBarLayoutPlan {
    // Frameless window: reserve the leading slot for the app icon / drag area.
    bool appIconInset = false;
    // The menu bar is drawn inside this title bar. False when the platform owns
    // the menu bar (macOS global menu bar, Unity's dbusmenu) or when the
    // in-window menu bar is hidden on purpose.
    bool inWindowMenu = true;
    // Frameless window on macOS: the leading slot belongs to the native traffic
    // lights, which QWindowKit centres inside the widget handed to
    // WidgetWindowAgent::setSystemButtonArea(). Takes the place of both the app
    // icon inset and the custom system buttons.
    bool trafficLightArea = false;
    bool projectSelector = true;
    bool documentActions = true;
    bool playback = true;
    // Custom minimize / maximize / close buttons, for a frameless window on a
    // platform that has no native ones left to reuse.
    bool systemButtons = false;
    int leftMargin = 6;

    friend bool operator==(const TitleBarLayoutPlan &, const TitleBarLayoutPlan &) = default;

    // menuInWindow is !QMenuBar::isNativeMenuBar() unless a caller forces the
    // in-window menu bar away, e.g. the developer preview toggle.
    [[nodiscard]] static TitleBarLayoutPlan forChrome(ChromePlatform platform, bool useNativeFrame,
                                                      bool menuInWindow) {
        TitleBarLayoutPlan plan;
        plan.inWindowMenu = menuInWindow;
        if (useNativeFrame)
            return plan;

        if (platform == ChromePlatform::MacOS) {
            // macOS keeps its traffic lights and only moves them into our row.
            // Drawing Windows-style buttons next to them was never right.
            plan.trafficLightArea = true;
            plan.leftMargin = 0;
        } else {
            plan.appIconInset = true;
            plan.systemButtons = true;
        }
        return plan;
    }

    [[nodiscard]] static constexpr ChromePlatform currentPlatform() {
#ifdef Q_OS_MAC
        return ChromePlatform::MacOS;
#elif defined(Q_OS_WIN)
        return ChromePlatform::Windows;
#else
        return ChromePlatform::Other;
#endif
    }
};

#endif // TITLEBARLAYOUTPLAN_H
