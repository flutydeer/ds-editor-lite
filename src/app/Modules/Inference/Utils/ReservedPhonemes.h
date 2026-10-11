#ifndef RESERVEDPHONEMESET_H
#define RESERVEDPHONEMESET_H

#include <QSet>
#include <QString>

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/SynthrtEngine/ReservedPhonemes.h>

/// Reserved phonemes of the inference tasks, as Qt strings.
///
/// A reserved phoneme is a token that is entered directly as a lyric. The note is sung as that
/// phoneme without grapheme-to-phoneme conversion and is passed to the models as a rest. The set
/// is the union of the forced reserved phonemes (lite::synthrt::FORCED_RESERVED_PHONEMES) and the
/// reserved phonemes declared by the singer.
namespace ReservedPhonemes {

    inline QString toQString(const std::string_view phoneme) {
        return QString::fromUtf8(phoneme.data(), static_cast<qsizetype>(phoneme.size()));
    }

    /// Returns only the forced reserved phonemes, for a caller without a singer.
    inline QSet<QString> forced() {
        QSet<QString> result;
        for (const auto phoneme : lite::synthrt::FORCED_RESERVED_PHONEMES)
            result.insert(toQString(phoneme));
        return result;
    }

    /// Returns the union for the singer \a identifier, read from the engine catalog.
    QSet<QString> of(const SingerIdentifier &identifier);

    /// Returns the reserved phoneme that pads a phrase with silence.
    inline QString silence() {
        return toQString(lite::synthrt::SILENCE_PHONEME);
    }

}

#endif // RESERVEDPHONEMESET_H
