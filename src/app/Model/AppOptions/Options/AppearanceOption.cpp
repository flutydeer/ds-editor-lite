//
// Created by fluty on 24-3-13.
//

#include "AppearanceOption.h"

void AppearanceOption::load(const QJsonObject &object) {
    if (object.contains(useNativeFrameKey))
        useNativeFrame = object.value(useNativeFrameKey).toBool();
    if (object.contains(enableDirectManipulationKey))
        enableDirectManipulation = object.value(enableDirectManipulationKey).toBool();
    if (object.contains(animationLevelKey))
        animationLevel = animationLevelFromString(object.value(animationLevelKey).toString());
    if (object.contains(animationTimeScaleKey))
        animationTimeScale = object.value(animationTimeScaleKey).toDouble();
    if (object.contains(themeIdKey)) {
        const auto saved = object.value(themeIdKey).toString().trimmed();
        // Accept only this setting's own valid preference ids; legacy concrete
        // theme ids ("lite-dark") or unknown values fall back to the default.
        // These preference strings are the setting's own vocabulary — the theme
        // system's ThemeIds carries the same names independently, so there is no
        // cross-layer (Model -> theme system) dependency.
        if (saved == QStringLiteral("system") || saved == QStringLiteral("light") ||
            saved == QStringLiteral("dark"))
            themeId = saved;
    }
}

void AppearanceOption::save(QJsonObject &object) {
    object.insert(useNativeFrameKey, useNativeFrame);
    object.insert(enableDirectManipulationKey, enableDirectManipulation);
    object.insert(animationLevelKey, animationLevelToString(animationLevel));
    object.insert(animationTimeScaleKey, animationTimeScale);
    object.insert(themeIdKey, themeId);
}

AnimationGlobal::AnimationLevels AppearanceOption::animationLevelFromString(const QString &name) {
    if (name == "decreased")
        return AnimationGlobal::Decreased;

    if (name == "none")
        return AnimationGlobal::None;

    return AnimationGlobal::Full;
}

QString AppearanceOption::animationLevelToString(const AnimationGlobal::AnimationLevels level) {
    switch (level) {
        case AnimationGlobal::Decreased:
            return "decreased";

        case AnimationGlobal::None:
            return "none";

        case AnimationGlobal::Full:
        default:
            return "full";
    }
}

