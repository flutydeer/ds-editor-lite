#include "PhonicNote.h"

namespace FillLyric {

    QString PhonicNote::lyric() const {
        return m_lyric;
    }

    void PhonicNote::setLyric(const QString &lyric) {
        m_lyric = lyric;
    }

    Pron PhonicNote::pronunciation() const {
        return m_pron;
    }

    void PhonicNote::setPronunciation(const Pron &pronunciation) {
        m_pron = pronunciation;
    }

    QStringList PhonicNote::pronCandidates() const {
        return m_pronCandidates;
    }

    void PhonicNote::setPronCandidates(const QStringList &pronCandidates) {
        m_pronCandidates = pronCandidates;
    }

    TextType PhonicNote::lyricType() const {
        return m_lyricType;
    }

    void PhonicNote::setLyricType(TextType lyricType) {
        m_lyricType = lyricType;
    }

    bool PhonicNote::lineFeed() const {
        return m_lineFeed;
    }

    void PhonicNote::setLineFeed(bool lineFeed) {
        m_lineFeed = lineFeed;
    }

    bool PhonicNote::isSlur() const {
        return m_lyric.isEmpty();
    }
} // FillLyric