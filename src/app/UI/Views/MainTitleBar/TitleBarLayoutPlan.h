#ifndef TITLEBARLAYOUTPLAN_H
#define TITLEBARLAYOUTPLAN_H

// Which sections the main title bar is built from.
//
// The builder in MainTitleBar walks this plan in a fixed order and inserts a
// divider only between two present sections, so a platform or an option that
// drops a section can never leave an orphan divider at the leading edge. That
// is exactly what macOS did: QMenuBar hides itself once the platform owns the
// menu bar (qmenubar.cpp, QMenuBarPrivate::init), the menu section collapsed to
// zero width, and the divider that used to separate it from the project
// selector stayed behind at the far left.
struct TitleBarLayoutPlan {
    // Frameless window: reserve the leading slot for the app icon / drag area.
    bool appIconInset = false;
    // The menu bar is drawn inside this title bar. False when the platform owns
    // the menu bar (macOS global menu bar, Unity's dbusmenu) or when the
    // in-window menu bar is hidden on purpose.
    bool inWindowMenu = true;
    bool projectSelector = true;
    bool documentActions = true;
    bool playback = true;
    // Custom minimize / maximize / close buttons, only for a frameless window.
    bool systemButtons = false;
    int leftMargin = 6;

    friend bool operator==(const TitleBarLayoutPlan &, const TitleBarLayoutPlan &) = default;

    // menuInWindow is !QMenuBar::isNativeMenuBar() unless a caller forces the
    // in-window menu bar away, e.g. the developer preview toggle.
    [[nodiscard]] static TitleBarLayoutPlan forChrome(bool useNativeFrame, bool menuInWindow) {
        TitleBarLayoutPlan plan;
        plan.appIconInset = !useNativeFrame;
        plan.inWindowMenu = menuInWindow;
        plan.systemButtons = !useNativeFrame;
        return plan;
    }
};

#endif // TITLEBARLAYOUTPLAN_H
