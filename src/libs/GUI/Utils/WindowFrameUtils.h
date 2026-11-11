#ifndef WINDOWFRAMEUTILS_H
#define WINDOWFRAMEUTILS_H

#include <QtGlobal>

class QWidget;

class WindowFrameUtils {
public:
    enum class CornerPreference { Default = 0, DoNotRound = 1, Round = 2, RoundSmall = 3 };

    static void applyFrameEffects(QWidget *widget);
    static void applyPopupEffects(QWidget *widget,
                                  CornerPreference cornerPreference = CornerPreference::Round);
    // macOS: extends the content view under the title bar and hides the title,
    // so a custom title bar can reclaim that row and the traffic lights can be
    // moved inside it. Idempotent, safe to call on every show.
    static void mergeTitleBarIntoContent(QWidget *widget);
};

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
inline void WindowFrameUtils::applyFrameEffects(QWidget *widget) {
    Q_UNUSED(widget);
}
#endif

#if !defined(Q_OS_MAC)
inline void WindowFrameUtils::mergeTitleBarIntoContent(QWidget *widget) {
    Q_UNUSED(widget);
}
#endif

#if !defined(Q_OS_WIN)
inline void WindowFrameUtils::applyPopupEffects(QWidget *widget,
                                                CornerPreference cornerPreference) {
    Q_UNUSED(widget);
    Q_UNUSED(cornerPreference);
}
#endif

#endif // WINDOWFRAMEUTILS_H
