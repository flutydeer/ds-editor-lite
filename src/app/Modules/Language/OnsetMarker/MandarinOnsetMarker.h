#ifndef MANDARINONSETMARKER_H
#define MANDARINONSETMARKER_H

#include "IOnsetMarker.h"

class MandarinOnsetMarker final : public IOnsetMarker {
public:
    QList<PhonemeName> mark(const QStringList &phonemeNames,
                            const QString &language) const override;
};

#endif // MANDARINONSETMARKER_H
