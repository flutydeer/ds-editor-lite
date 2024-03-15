#ifndef LANGCOMMON_H
#define LANGCOMMON_H

#include <QObject>
#include <QString>
#include <utility>

class LangCommon final : public QObject {
    Q_OBJECT
public:
    enum Language {
        Mandarin,
        Cantonese,
        English,
        Kana,
        Slur,
        Number,
        Space,
        Linebreak,
        Punctuation,
        Unknown
    };
    Q_ENUM(Language);
};

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
#endif // LANGCOMMON_H
