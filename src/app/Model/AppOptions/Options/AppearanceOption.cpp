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
        const auto savedThemeId = object.value(themeIdKey).toString().trimmed();
        if (isThemePreferenceId(savedThemeId))
            themeId = savedThemeId;
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

QString AppearanceOption::defaultThemeId() {
    return QStringLiteral("lite-dark");
}

QString AppearanceOption::lightThemeId() {
    return QStringLiteral("lite-light");
}

QString AppearanceOption::systemThemePreferenceId() {
    return QStringLiteral("system");
}

QString AppearanceOption::lightThemePreferenceId() {
    return QStringLiteral("light");
}

QString AppearanceOption::darkThemePreferenceId() {
    return QStringLiteral("dark");
}

QString AppearanceOption::themeIdForPreference(const QString &themePreferenceId) {
    if (themePreferenceId == systemThemePreferenceId())
        return QString();
    if (themePreferenceId == lightThemePreferenceId())
        return lightThemeId();
    if (themePreferenceId == darkThemePreferenceId())
        return defaultThemeId();
    return themePreferenceId;
}

bool AppearanceOption::isBuiltInThemeId(const QString &themeId) {
    return themeId == defaultThemeId() || themeId == lightThemeId();
}

bool AppearanceOption::isThemePreferenceId(const QString &themeId) {
    return themeId == systemThemePreferenceId() || themeId == lightThemePreferenceId() ||
           themeId == darkThemePreferenceId();
}
