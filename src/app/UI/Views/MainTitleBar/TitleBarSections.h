#ifndef TITLEBARSECTIONS_H
#define TITLEBARSECTIONS_H

#include <QtGlobal>

enum class ChromePlatform { Windows, MacOS, Other };

// The window chrome is shared by the main window and the detached panel window.
// A plan describes which sections a particular window shows; MainTitleBar lays
// them out in a fixed order and only inserts a divider between two adjacent
// leading sections. A reduced title bar is therefore a configuration change,
// not a second implementation of the chrome.
struct TitleBarSections {
    // Reserves the application icon / drag slot at the left edge of a
    // frameless window. Native frames already provide that area.
    bool appIconInset = false;
    bool mainMenu = true;
    bool trafficLightArea = false;
    bool projectSelector = false;
    bool documentActions = false;
    bool playback = true;
    bool systemButtons = false;
    int leftMargin = 6;

    [[nodiscard]] bool operator==(const TitleBarSections &) const = default;

    [[nodiscard]] static constexpr ChromePlatform currentPlatform() {
#ifdef Q_OS_MAC
        return ChromePlatform::MacOS;
#elif defined(Q_OS_WIN)
        return ChromePlatform::Windows;
#else
        return ChromePlatform::Other;
#endif
    }

    [[nodiscard]] static TitleBarSections
        forMainWindow(const bool useNativeFrame, const ChromePlatform platform = currentPlatform()) {
        const bool trafficLightArea = !useNativeFrame && platform == ChromePlatform::MacOS;
        const bool customChrome = !useNativeFrame && !trafficLightArea;
        return {.appIconInset = customChrome,
                .trafficLightArea = trafficLightArea,
                .projectSelector = true,
                .documentActions = true,
                .systemButtons = customChrome,
                .leftMargin = trafficLightArea ? 0 : 6};
    }

    // The detached panel keeps the menu, the transport and the window chrome,
    // but has no document identity or document actions of its own.
    [[nodiscard]] static TitleBarSections
        forDetachedPanel(const bool useNativeFrame,
                         const ChromePlatform platform = currentPlatform()) {
        auto sections = forMainWindow(useNativeFrame, platform);
        sections.projectSelector = false;
        sections.documentActions = false;
        return sections;
    }
};

// Menu and transport controls are duplicated per window, but the application
// level key sequences must have exactly one owner. The secondary window still
// shows the same controls and receives the same controller updates.
enum class TitleBarRole { Primary, Secondary };

#endif // TITLEBARSECTIONS_H
