//
// Created by fluty on 2024/2/6.
//

#ifndef APPGLOBAL_H
#define APPGLOBAL_H

#include <QList>
#include <QString>

namespace AppGlobal {
    // Global Constants
    inline constexpr int resizeTolerance = 8;

    enum AudioLoadStatus { Init, Loading, Loaded, Error };

    enum PanelType { Generic, TracksEditor, ClipEditor };

    enum NotePropertyType { Language, Lyric, Pronunciation, Phonemes };

    enum ParamType {};

    inline const QStringList languageNames = {"cmn", "eng", "jpn", "yue", "unknown"};

}

#endif // APPGLOBAL_H