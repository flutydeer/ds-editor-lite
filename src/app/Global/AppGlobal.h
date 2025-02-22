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

    enum NotePropertyType { Language, Lyric, Pronunciation, Phonemes };

    enum ParamType {};

    const QStringList languageNames = {"cmn", "eng", "jpn", "unknown"};

}

#endif // APPGLOBAL_H