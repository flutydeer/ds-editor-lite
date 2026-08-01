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
    // animationLevel and themeId are stored as opaque persisted strings; the
    // UI/animation and theme systems own the AnimationLevels enum and the theme
    // vocabulary respectively and convert at the boundary.
    QString animationLevel = QStringLiteral("full");
    double animationTimeScale = 1;
    QString themeId = QStringLiteral("system");

protected:
    void save(QJsonObject &object) override;

private:
    const QString useNativeFrameKey = "useNativeFrame";
    const QString enableDirectManipulationKey = "enableDirectManipulation";
    const QString animationLevelKey = "animationLevel";
    const QString animationTimeScaleKey = "animationTimeScale";
    const QString themeIdKey = "themeId";
};

#endif // APPEARANCEOPTION_H
