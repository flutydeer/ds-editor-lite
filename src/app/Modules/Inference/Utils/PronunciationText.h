#ifndef PRONUNCIATIONTEXT_H
#define PRONUNCIATIONTEXT_H

#include <string>

#include <QString>
#include <QStringList>

/// Text conversions between the editor and the language layer, shared by the pronunciation task
/// and the lyric filling dialog, which both convert lyrics through SynthrtEngine::convert().
namespace PronunciationText {

    /// Returns the UTF-8 encoding of \a value, which is the text encoding of the language layer.
    std::string toUtf8(const QString &value);

    /// Decodes a UTF-8 string returned by the language layer.
    QString fromUtf8(const std::string &value);

    /// Collapses candidates that are only the phoneme tokens of \a pronunciation to the whole
    /// pronunciation.
    ///
    /// Candidates from a dictionary step may be the split tokens of the pronunciation itself rather
    /// than alternative pronunciations, and the editor must not offer single phonemes as
    /// switchable candidates. Other candidates are trimmed, empty ones are dropped, and the list is
    /// returned otherwise unchanged.
    QStringList normalizeCandidates(const QString &pronunciation, QStringList candidates);

}

#endif // PRONUNCIATIONTEXT_H
