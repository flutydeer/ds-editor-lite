#ifndef DS_EDITOR_LITE_PHONICNOTE_H
#define DS_EDITOR_LITE_PHONICNOTE_H

#include <QString>
#include <QDataStream>
#include <utility>
#include "PhonicModel.h"

#include "../Utils/CleanLyric.h"
#include "../Model/PhonicCommon.h"

namespace FillLyric {
    class Pron {
    public:
        QString original;
        QString edited;

        Pron() = default;
        Pron(QString original, QString edited = QString())
            : original(std::move(original)), edited(std::move(edited)) {
        }
    };

    class PhonicNote {
    public:
        explicit PhonicNote() = default;
        explicit PhonicNote(QString lyric, Pron pron, bool lineFeed = false)
            : m_lyric(std::move(lyric)), m_pron(std::move(pron)), m_lineFeed(lineFeed) {
        }

        QString lyric() const;
        void setLyric(const QString &lyric);
        Pron pronunciation() const;
        void setPronunciation(const Pron &pronunciation);
        QStringList pronCandidates() const;
        void setPronCandidates(const QStringList &pronCandidates);
        TextType lyricType() const;
        void setLyricType(TextType lyricType);
        bool lineFeed() const;
        void setLineFeed(bool lineFeed);
        bool isSlur() const;

    private:
        QString m_lyric;
        Pron m_pron;
        QStringList m_pronCandidates;
        TextType m_lyricType = TextType::Other;
        bool m_lineFeed = false;
    };

} // FillLyric

#endif // DS_EDITOR_LITE_PHONICNOTE_H
