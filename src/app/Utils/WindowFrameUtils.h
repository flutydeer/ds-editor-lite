//
// Created by fluty on 24-2-19.
//

#ifndef WINDOWFRAMEUTILS_H
#define WINDOWFRAMEUTILS_H

#include <QtGlobal>

class QWidget;

class WindowFrameUtils {
public:
    static void applyFrameEffects(QWidget *widget);
};

#if !defined(Q_OS_WIN) && !defined(Q_OS_MAC)
inline void WindowFrameUtils::applyFrameEffects(QWidget *widget) {
    Q_UNUSED(widget);
}
#endif

#endif // WINDOWFRAMEUTILS_H
