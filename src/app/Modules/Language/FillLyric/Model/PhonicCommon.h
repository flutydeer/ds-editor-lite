#ifndef DS_EDITOR_LITE_PHONICCOMMON_H
#define DS_EDITOR_LITE_PHONICCOMMON_H

namespace FillLyric {
    enum PhonicRole {
        Tooltip = Qt::ToolTipRole,
        Syllable = Qt::UserRole,
        Candidate,
        SyllableRevised,
        Language,
        FermataAddition
    };

    struct Phonic {
        QString lyric;
        QString syllable;
        QStringList candidates;
        QString syllableRevised;
        QString language;
        QList<QString> fermata;
        bool lineFeed = false;
    };
}

#endif // DS_EDITOR_LITE_PHONICCOMMON_H
