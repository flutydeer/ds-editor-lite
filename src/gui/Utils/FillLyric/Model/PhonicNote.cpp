#include "PhonicNote.h"

namespace FillLyric {

    QString PhonicNote::lyric() const {
        return m_lyric;
    }

    void PhonicNote::setLyric(const QString &lyric) {
        m_lyric = lyric;
    }

    Syllable PhonicNote::pronunciation() const {
        return m_syllable;
    }

    void PhonicNote::setPronunciation(const Syllable &pronunciation) {
        m_syllable = pronunciation;
    }

    QStringList PhonicNote::pronCandidates() const {
        return m_pronCandidates;
    }

    void PhonicNote::setPronCandidates(const QStringList &pronCandidates) {
        m_pronCandidates = pronCandidates;
    }

    LyricType PhonicNote::lyricType() const {
        return m_lyricType;
    }

    void PhonicNote::setLyricType(LyricType lyricType) {
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