//
// Created by fluty on 24-3-13.
//

#ifndef APPEARANCEOPTION_H
#define APPEARANCEOPTION_H

#include "Global/AnimationGlobal.h"

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
    AnimationGlobal::AnimationLevels animationLevel = AnimationGlobal::Full;
    double animationTimeScale = 1;

    static AnimationGlobal::AnimationLevels animationLevelFromString(const QString &name);
    static QString animationLevelToString(AnimationGlobal::AnimationLevels level);

protected:
    void save(QJsonObject &object) override;

private:
    const QString useNativeFrameKey = "useNativeFrame";
    const QString enableDirectManipulationKey = "enableDirectManipulation";
    const QString animationLevelKey = "animationLevel";
    const QString animationTimeScaleKey = "animationTimeScale";
};

#endif // APPEARANCEOPTION_H
