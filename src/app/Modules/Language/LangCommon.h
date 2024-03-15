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
    LangCommon::Language language = LangCommon::Unknown;

    LangNote() = default;
    LangNote(QString lyric, const LangCommon::Language language)
        : lyric(std::move(lyric)), language(language){};
};
#endif // LANGCOMMON_H
