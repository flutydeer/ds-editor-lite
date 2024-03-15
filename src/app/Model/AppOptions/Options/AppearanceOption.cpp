//
// Created by fluty on 24-3-13.
//

#include "AppearanceOption.h"

AppearanceOption &AppearanceOption::operator=(const AppearanceOption &other) {
    if (this != &other) {
        IOption::operator=(other);
        animationLevel = other.animationLevel;
        animationTimeScale = other.animationTimeScale;
    }
    return *this;
}
void AppearanceOption::load(const QJsonObject &object) {
    m_model = object;
    if (m_model.contains(animationLevelKey))
        animationLevel = animationLevelFromString(m_model.value(animationLevelKey).toString());
    if (m_model.contains(animationTimeScaleKey))
        animationTimeScale = m_model.value(animationTimeScaleKey).toDouble();
}
void AppearanceOption::serialize() {
    if (m_model.contains(animationLevelKey))
        m_model.remove(animationLevelKey);
    m_model.insert(animationLevelKey, animationLevelToString(animationLevel));

    if (m_model.contains(animationTimeScaleKey))
        m_model.remove(animationTimeScaleKey);
    m_model.insert(animationTimeScaleKey, animationTimeScale);
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
QString AppearanceOption::animationLevelToString(AnimationGlobal::AnimationLevels level) {
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