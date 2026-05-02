#ifndef DEFAULTONSETMARKER_H
#define DEFAULTONSETMARKER_H

#include "IOnsetMarker.h"

class DefaultOnsetMarker final : public IOnsetMarker {
public:
    QList<PhonemeName> mark(const QStringList &phonemeNames,
                            const QString &language) const override;
};

#endif // DEFAULTONSETMARKER_H
