#ifndef DS_EDITOR_LITE_PHONICCOMMON_H
#define DS_EDITOR_LITE_PHONICCOMMON_H

#include "Modules/Language/LangCommon.h"

namespace FillLyric {
    using TextType = LangCommon::Language;
    using LyricTypeList = QList<TextType>;

    enum SplitType { Auto, ByChar, Custom, ByReg };

    enum PhonicRole {
        Tooltip = Qt::ToolTipRole,
        Syllable = Qt::UserRole,
        Candidate,
        SyllableRevised,
        LyricType,
        FermataAddition,
        LineFeed
    };

    struct Phonic {
        QString lyric;
        QString syllable;
        QStringList candidates;
        QString syllableRevised;
        TextType lyricType;
        QList<QString> fermata;
        bool lineFeed = false;
    };
}

#endif // DS_EDITOR_LITE_PHONICCOMMON_H
