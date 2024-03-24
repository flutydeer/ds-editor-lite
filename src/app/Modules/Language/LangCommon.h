#ifndef LANGCOMMON_H
#define LANGCOMMON_H

#include <QString>

#include "Model/Note.h"

struct Phonic {
    QString text;
    Pronunciation pronunciation;
    QList<QString> candidates;
    bool isSlur = false;
    bool g2pError = false;
};

struct LangNote {
    QString lyric;
    QString syllable = QString();
    QStringList candidates = QStringList();
    QString language = "Unknown";
    QString category = "Unknown";
    bool g2pError = false;

    LangNote() = default;
    explicit LangNote(QString lyric) : lyric(std::move(lyric)){};
    LangNote(QString lyric, QString language)
        : lyric(std::move(lyric)), language(std::move(language)){};
    LangNote(QString lyric, QString language, QString categrory)
        : lyric(std::move(lyric)), language(std::move(language)), category(std::move(categrory)){};
};

struct LangConfig {
    QString id;
    QString language;
    bool enabled = true;
    bool discardResult = false;
    QString author;
    QString description;
};
#endif // LANGCOMMON_H
