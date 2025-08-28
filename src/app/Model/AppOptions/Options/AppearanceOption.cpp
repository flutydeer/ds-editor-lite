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
}

void AppearanceOption::save(QJsonObject &object) {
    object.insert(useNativeFrameKey, useNativeFrame);
    object.insert(enableDirectManipulationKey, enableDirectManipulation);
    object.insert(animationLevelKey, animationLevelToString(animationLevel));
    object.insert(animationTimeScaleKey, animationTimeScale);
}

AnimationGlobal::AnimationLevels AppearanceOption::animationLevelFromString(const QString &name) {
    if (name == "decreased")
        return AnimationGlobal::Decreased;

    if (name == "none")
        return AnimationGlobal::None;

    // if (name == "full")
    //     return AnimationGlobal::Full;
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