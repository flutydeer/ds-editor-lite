//
// Created by fluty on 24-2-19.
//

#ifndef WINDOWFRAMEUTILS_H
#define WINDOWFRAMEUTILS_H

#include <QWidget>


#ifdef Q_OS_WIN
#  include <dwmapi.h>
#endif

class WindowFrameUtils {
public:
    static void applyFrameEffects(QWidget *widget);
};


#endif // WINDOWFRAMEUTILS_H
