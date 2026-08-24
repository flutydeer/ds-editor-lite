#ifndef APPGLOBAL_H
#define APPGLOBAL_H

#include <QList>
#include <QString>

#include <lite/MusicBase/MusicTime.h>
#include <lite/AutomationWire/PublicConstants.h>

namespace AppGlobal {
    // Global Constants
    // The tick time base now lives in lite::MusicBase; re-exported here so the
    // existing AppGlobal::ticksPer* call sites keep resolving.
    inline constexpr int ticksPerQuarterNote = MusicTime::ticksPerQuarterNote;
    inline constexpr int ticksPerWholeNote = MusicTime::ticksPerWholeNote;
    inline constexpr int resizeTolerance = 8;

    // Number of colors in the application-wide color palette.
    // Used by AppColorPalette, track colors, speaker mix colors, etc.
    inline constexpr int paletteColorCount = AutomationWire::TrackPaletteColorCount;

    enum AudioLoadStatus { Init, Loading, Loaded, Error };

    enum PanelType { Generic, TracksEditor, ClipEditor };

    enum ParamType {};

    inline const QStringList languageNames = {"cmn", "eng", "jpn", "yue", "unknown"};

}

#endif // APPGLOBAL_H
