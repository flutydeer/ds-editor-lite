#ifndef APPEARANCEOPTION_H
#define APPEARANCEOPTION_H

#include "Model/AppOptions/IOption.h"

class AppearanceOption : public IOption {
public:
    explicit AppearanceOption() : IOption("appearance") {
    }

    void load(const QJsonObject &object) override;

#ifdef Q_OS_MAC
    bool useNativeFrame = true;
#else
    bool useNativeFrame = false;
#endif
    bool enableDirectManipulation = true;
    // themeId is stored as an opaque persisted string; the theme system owns
    // the vocabulary and converts at the boundary. Animation is a plain on/off
    // toggle; a missing or unparseable value defaults to enabled.
    bool animationEnabled = true;
    double animationTimeScale = 1;
    QString themeId = QStringLiteral("system");
    // Empty string means the platform-default UI font (see FontManager).
    // The font setting is hot-swappable; no restart required.
    QString uiFontFamily;

protected:
    void save(QJsonObject &object) override;

private:
    const QString useNativeFrameKey = "useNativeFrame";
    const QString enableDirectManipulationKey = "enableDirectManipulation";
    const QString animationEnabledKey = "animationEnabled";
    const QString animationTimeScaleKey = "animationTimeScale";
    const QString themeIdKey = "themeId";
    const QString uiFontFamilyKey = "uiFontFamily";
};

#endif // APPEARANCEOPTION_H
