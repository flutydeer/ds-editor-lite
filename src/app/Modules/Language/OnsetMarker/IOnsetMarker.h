#ifndef IONSETMARKER_H
#define IONSETMARKER_H

#include "Model/AppModel/Phonemes.h"

#include <QStringList>

class IOnsetMarker {
public:
    virtual ~IOnsetMarker() = default;
    virtual QList<PhonemeName> mark(const QStringList &phonemeNames,
                                    const QString &language) const = 0;
};

#endif // IONSETMARKER_H
