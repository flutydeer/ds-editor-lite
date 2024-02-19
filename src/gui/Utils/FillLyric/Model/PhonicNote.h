#ifndef DS_EDITOR_LITE_PHONICNOTE_H
#define DS_EDITOR_LITE_PHONICNOTE_H

#include <QString>
#include <QDataStream>
#include <utility>
#include "PhonicModel.h"

#include "../Utils/CleanLyric.h"

namespace FillLyric {
    using LyricType = CleanLyric::LyricType;
    class Syllable {
    public:
        QString original;
        QString edited;

        Syllable() = default;
        Syllable(QString original, QString edited = QString())
            : original(std::move(original)), edited(std::move(edited)) {
        }
    };

    class PhonicNote {
    public:
        explicit PhonicNote() = default;
        explicit PhonicNote(QString lyric, Syllable syllable, bool lineFeed = false)
            : m_lyric(std::move(lyric)), m_syllable(std::move(syllable)), m_lineFeed(lineFeed) {
        }

        QString lyric() const;
        void setLyric(const QString &lyric);
        Syllable pronunciation() const;
        void setPronunciation(const Syllable &pronunciation);
        QStringList pronCandidates() const;
        void setPronCandidates(const QStringList &pronCandidates);
        LyricType lyricType() const;
        void setLyricType(LyricType lyricType);
        bool lineFeed() const;
        void setLineFeed(bool lineFeed);
        bool isSlur() const;

    private:
        QString m_lyric;
        Syllable m_syllable;
        QStringList m_pronCandidates;
        LyricType m_lyricType = LyricType::Other;
        bool m_lineFeed = false;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PHONICNOTE_H
