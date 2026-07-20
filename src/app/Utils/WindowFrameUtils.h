//
// Created by fluty on 24-2-19.
//

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
};

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
inline void WindowFrameUtils::applyFrameEffects(QWidget *widget) {
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
