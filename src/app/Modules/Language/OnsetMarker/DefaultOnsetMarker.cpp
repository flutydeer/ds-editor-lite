#include "DefaultOnsetMarker.h"

QList<PhonemeName> DefaultOnsetMarker::mark(const QStringList &phonemeNames,
                                             const QString &language) const {
    QList<PhonemeName> result;
    for (const auto &name : phonemeNames) {
        PhonemeName phoneme;
        phoneme.name = name;
        phoneme.language = language;
        phoneme.isOnset = true;
        result.append(phoneme);
    }
    return result;
}
