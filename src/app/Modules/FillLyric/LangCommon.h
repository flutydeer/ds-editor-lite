#ifndef LYRIC_TAB_LANG_COMMON_H
#define LYRIC_TAB_LANG_COMMON_H

#include <QVersionNumber>
#include <QString>
#include <QStringList>

// G2P 相关常量（项目内集中定义）
inline constexpr auto kUnknownG2pId = "unknown";
inline constexpr auto kSlurLyric = "slur";

struct LangNote {
    QString lyric;
    QString syllable;
    QString syllableRevised;
    QStringList candidates;
    QString language = "unknown";
    QString g2pId = kUnknownG2pId;
    bool revised = false;

    LangNote() = default;

    explicit LangNote(QString lyric) : lyric(std::move(lyric)) {}
    explicit LangNote(QString lyric, QString language, QString g2pId) :
        lyric(std::move(lyric)), language(std::move(language)), g2pId(std::move(g2pId)) {}
};
#endif // LYRIC_TAB_LANG_COMMON_H
