#ifndef LANGCOMMON_H
#define LANGCOMMON_H

#include <QString>

#include <LangMgr/LangGlobal.h>

struct LANG_MANAGER_EXPORT LangNote {
    QString lyric;
    QString syllable = QString();
    QString syllableRevised = QString();
    QStringList candidates = QStringList();
    QString language = "unknown";
    QString category = "unknown";
    QString standard = "unknown";
    bool revised = false;
    bool error = false;

    LangNote() = default;

    explicit LangNote(QString lyric) : lyric(std::move(lyric)){};

    LangNote(QString lyric, QString language)
        : lyric(std::move(lyric)), language(std::move(language)){};

    LangNote(QString lyric, QString language, QString categrory)
        : lyric(std::move(lyric)), language(std::move(language)), category(std::move(categrory)){};
};
#endif // LANGCOMMON_H
