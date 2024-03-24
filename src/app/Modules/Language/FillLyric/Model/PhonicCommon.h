#ifndef DS_EDITOR_LITE_PHONICCOMMON_H
#define DS_EDITOR_LITE_PHONICCOMMON_H

namespace FillLyric {
    enum PhonicRole {
        Tooltip = Qt::ToolTipRole,
        Syllable = Qt::UserRole,
        Candidate,
        SyllableRevised,
        Language,
        category,
        FermataAddition,
        G2pError
    };

    struct Phonic {
        QString lyric;
        QString syllable;
        QStringList candidates;
        QString syllableRevised;
        QString language = "Unknown";
        QString category = "Unknown";
        QList<QString> fermata;
        bool g2pError = false;
        bool lineFeed = false;
    };
}

#endif // DS_EDITOR_LITE_PHONICCOMMON_H
