//
// Created by fluty on 24-3-13.
//

#ifndef ANIMATIONGLOBAL_H
#define ANIMATIONGLOBAL_H

#include <QString>

// Animation-intensity vocabulary, owned by the UI/animation system. The settings
// layer (AppearanceOption) persists the string form opaquely and does not depend
// on this enum; app code converts at the boundary.
namespace AnimationGlobal {
    enum AnimationLevels { Full, Decreased, None };

    inline AnimationLevels fromString(const QString &name) {
        if (name == QStringLiteral("decreased"))
            return Decreased;
        if (name == QStringLiteral("none"))
            return None;
        return Full;
    }

    inline QString toString(AnimationLevels level) {
        switch (level) {
            case Decreased:
                return QStringLiteral("decreased");
            case None:
                return QStringLiteral("none");
            case Full:
            default:
                return QStringLiteral("full");
        }
    }
}

#endif // ANIMATIONGLOBAL_H
