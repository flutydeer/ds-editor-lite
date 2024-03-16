#ifndef LANGCOMMON_H
#define LANGCOMMON_H

#include <QString>

struct LangNote {
    QString lyric;
    QString syllable = QString();
    QStringList candidates = QStringList();
    QString language = "Unknown";

    LangNote() = default;
    explicit LangNote(QString lyric) : lyric(std::move(lyric)){};
    LangNote(QString lyric, QString language)
        : lyric(std::move(lyric)), language(std::move(language)){};
};

struct LangConfig {
    QString language;
    bool enabled = true;
    bool discardResult = false;
};
#endif // LANGCOMMON_H
