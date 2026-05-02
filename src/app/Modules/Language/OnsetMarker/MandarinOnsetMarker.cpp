#include "MandarinOnsetMarker.h"

#include <QDebug>

QList<PhonemeName> MandarinOnsetMarker::mark(const QStringList &phonemeNames,
                                              const QString &language) const {
    QList<PhonemeName> result;
    if (phonemeNames.size() == 1) {
        PhonemeName phoneme;
        phoneme.name = phonemeNames.at(0);
        phoneme.language = language;
        phoneme.isOnset = true;
        result.append(phoneme);
    } else if (phonemeNames.size() == 2) {
        PhonemeName first;
        first.name = phonemeNames.at(0);
        first.language = language;
        first.isOnset = false;
        result.append(first);

        PhonemeName second;
        second.name = phonemeNames.at(1);
        second.language = language;
        second.isOnset = true;
        result.append(second);
    } else {
        qCritical() << "MandarinOnsetMarker: unexpected phoneme count" << phonemeNames.size()
                     << phonemeNames;
        for (const auto &name : phonemeNames) {
            PhonemeName phoneme;
            phoneme.name = name;
            phoneme.language = language;
            phoneme.isOnset = false;
            result.append(phoneme);
        }
    }
    return result;
}
