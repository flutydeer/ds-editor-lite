#include "AppearanceOption.h"

void AppearanceOption::load(const QJsonObject &object) {
    if (object.contains(useNativeFrameKey))
        useNativeFrame = object.value(useNativeFrameKey).toBool();
    // Animation defaults to enabled; accept only a real bool so that legacy
    // string values ("full"/"decreased"/"none") never disable animations.
    if (object.value(animationEnabledKey).isBool())
        animationEnabled = object.value(animationEnabledKey).toBool();
    if (object.contains(animationTimeScaleKey))
        animationTimeScale = object.value(animationTimeScaleKey).toDouble();
    if (object.contains(themeIdKey)) {
        const auto saved = object.value(themeIdKey).toString().trimmed();
        // Accept only this setting's own valid preference ids; legacy concrete
        // theme ids ("lite-dark") or unknown values fall back to the default.
        // These preference strings are the setting's own vocabulary - the theme
        // system's ThemeIds carries the same names independently, so there is no
        // cross-layer (Model -> theme system) dependency.
        if (saved == QStringLiteral("system") || saved == QStringLiteral("light") ||
            saved == QStringLiteral("dark"))
            themeId = saved;
    }
    if (object.contains(uiFontFamilyKey))
        uiFontFamily = object.value(uiFontFamilyKey).toString().trimmed();
    if (object.value(showGhostNotesKey).isBool())
        showGhostNotes = object.value(showGhostNotesKey).toBool();
    if (object.value(showTempoLaneKey).isBool())
        showTempoLane = object.value(showTempoLaneKey).toBool();
    if (object.value(showTimeSignatureLaneKey).isBool())
        showTimeSignatureLane = object.value(showTimeSignatureLaneKey).toBool();
    if (object.value(showTimeLaneKey).isBool())
        showTimeLane = object.value(showTimeLaneKey).toBool();
    timecodeFormat =
        timecodeFormatFromId(object.value(timecodeFormatKey).toString(), timecodeFormat);
}

void AppearanceOption::save(QJsonObject &object) {
    object.insert(useNativeFrameKey, useNativeFrame);
    object.insert(animationEnabledKey, animationEnabled);
    object.insert(animationTimeScaleKey, animationTimeScale);
    object.insert(themeIdKey, themeId);
    object.insert(uiFontFamilyKey, uiFontFamily);
    object.insert(showGhostNotesKey, showGhostNotes);
    object.insert(showTempoLaneKey, showTempoLane);
    object.insert(showTimeSignatureLaneKey, showTimeSignatureLane);
    object.insert(showTimeLaneKey, showTimeLane);
    object.insert(timecodeFormatKey, timecodeFormatId(timecodeFormat));
}

QString AppearanceOption::timecodeFormatId(const TimecodeFormat format) {
    switch (format) {
        case TimecodeFormat::Time:
            return QStringLiteral("time");
        case TimecodeFormat::Both:
            return QStringLiteral("both");
        case TimecodeFormat::BarBeatTick:
            break;
    }
    return QStringLiteral("barBeatTick");
}

AppearanceOption::TimecodeFormat
    AppearanceOption::timecodeFormatFromId(const QString &id, const TimecodeFormat fallback) {
    if (id == QStringLiteral("time"))
        return TimecodeFormat::Time;
    if (id == QStringLiteral("both"))
        return TimecodeFormat::Both;
    if (id == QStringLiteral("barBeatTick"))
        return TimecodeFormat::BarBeatTick;
    return fallback;
}
