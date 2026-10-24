#ifndef RESERVEDPHONEMESET_H
#define RESERVEDPHONEMESET_H

#include <QSet>
#include <QString>

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/SynthrtEngine/ReservedPhonemes.h>

/// Forced and singer-declared reserved phonemes, used as direct-lyric rest markers.
namespace ReservedPhonemes {

    inline QString toQString(const std::string_view phoneme) {
        return QString::fromUtf8(phoneme.data(), static_cast<qsizetype>(phoneme.size()));
    }

    inline QSet<QString> forced() {
        QSet<QString> result;
        for (const auto phoneme : lite::synthrt::FORCED_RESERVED_PHONEMES)
            result.insert(toQString(phoneme));
        return result;
    }

    QSet<QString> of(const SingerIdentifier &identifier);

    inline QString silence() {
        return toQString(lite::synthrt::SILENCE_PHONEME);
    }

}

#endif // RESERVEDPHONEMESET_H
