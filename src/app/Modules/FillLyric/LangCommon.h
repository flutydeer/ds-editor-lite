#ifndef LYRIC_TAB_LANG_COMMON_H
#define LYRIC_TAB_LANG_COMMON_H

#include <QString>
#include <QStringList>

struct LangNote {
    QString lyric;
    QString syllable;
    QString syllableRevised;
    QStringList candidates;
    QString language = "unknown";
    QString g2pId = "unknown";
    bool revised = false;

    LangNote() = default;

    explicit LangNote(QString lyric) : lyric(std::move(lyric)) {}
    explicit LangNote(QString lyric, QString language, QString g2pId) :
        lyric(std::move(lyric)), language(std::move(language)), g2pId(std::move(g2pId)) {}
};
#endif // LYRIC_TAB_LANG_COMMON_H
