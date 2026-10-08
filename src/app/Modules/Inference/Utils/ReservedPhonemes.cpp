#include "ReservedPhonemes.h"

#include <lite/SynthrtEngine/SynthrtEngine.h>

namespace ReservedPhonemes {

    QSet<QString> of(const SingerIdentifier &identifier) {
        QSet<QString> result;
        for (const auto &phoneme : SynthrtEngine::instance().reservedPhonemesOf(identifier))
            result.insert(QString::fromStdString(phoneme));
        return result;
    }

}
