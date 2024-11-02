//
// Created by fluty on 2024/2/6.
//

#ifndef APPGLOBAL_H
#define APPGLOBAL_H

#include <QList>
#include <QString>

namespace AppGlobal {
    // Global Constants
    constexpr int resizeTolerance = 8;

    enum AudioLoadStatus { Init, Loading, Loaded, Error };

    enum PanelType { Generic, TracksEditor, ClipEditor };

    enum ParamType {};
}

#endif // APPGLOBAL_H