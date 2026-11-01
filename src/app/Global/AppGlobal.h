#ifndef APPGLOBAL_H
#define APPGLOBAL_H

#include <QList>
#include <QMetaType>
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

    // Whether an audio clip's waveform can be drawn yet. It folds together the two
    // independent things that keep a clip from being drawable: the work in flight for it
    // (path resolution or waveform decoding) and a file that resolved to nothing.
    // AudioDecodingController is the single producer, so every render backend shows the
    // same state instead of deriving its own.
    enum class AudioClipLoadState { Loading, Loaded, Missing };

    enum PanelType { Generic, TracksEditor, ClipEditor };

    enum ParamType {};

    inline const QStringList languageNames = {"cmn", "eng", "jpn", "yue", "unknown"};

}

// The load state travels through signals and QSignalSpy, so it needs a metatype registration
Q_DECLARE_METATYPE(AppGlobal::AudioClipLoadState)

#endif // APPGLOBAL_H
