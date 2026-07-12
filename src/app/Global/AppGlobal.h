//
// Created by fluty on 2024/2/6.
//

#ifndef APPGLOBAL_H
#define APPGLOBAL_H

#include <QList>
#include <QString>

namespace AppGlobal {
    // Global Constants
    inline constexpr int ticksPerQuarterNote = 480;
    inline constexpr int ticksPerWholeNote = ticksPerQuarterNote * 4;
    inline constexpr int resizeTolerance = 8;

    // Number of colors in the application-wide color palette.
    // Used by AppColorPalette, track colors, speaker mix colors, etc.
    inline constexpr int paletteColorCount = 12;

    enum AudioLoadStatus { Init, Loading, Loaded, Error };

    enum PanelType { Generic, TracksEditor, ClipEditor };

    enum ParamType {};

    inline const QStringList languageNames = {"cmn", "eng", "jpn", "yue", "unknown"};

}

#endif // APPGLOBAL_H
