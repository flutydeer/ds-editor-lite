//
// Created by fluty on 24-3-1.
//

#ifndef IANIMATABLE_H
#define IANIMATABLE_H

#include "ThemeManager.h"
#include "Global/AnimationGlobal.h"

class IAnimatable {
public:
    IAnimatable();
    virtual ~IAnimatable();
    [[nodiscard]] AnimationGlobal::AnimationLevels animationLevel() const;
    void setAnimationLevel(AnimationGlobal::AnimationLevels level);
    [[nodiscard]] double animationTimeScale() const;
    void setTimeScale(double scale);

protected:
    [[nodiscard]] int getScaledAnimationTime(int ms) const;

    virtual void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) = 0;
    virtual void afterSetTimeScale(double scale) = 0;

private:
    AnimationGlobal::AnimationLevels m_level = AnimationGlobal::Full;
    double m_scale = 1.0;
};
inline IAnimatable::IAnimatable() {
    ThemeManager::instance()->addSubscriber(this);
}
inline IAnimatable::~IAnimatable() {
    ThemeManager::instance()->removeSubscriber(this);
}
inline AnimationGlobal::AnimationLevels IAnimatable::animationLevel() const {
    return m_level;
}
inline void IAnimatable::setAnimationLevel(AnimationGlobal::AnimationLevels level) {
    m_level = level;
    afterSetAnimationLevel(level);
}
inline double IAnimatable::animationTimeScale() const {
    return m_scale;
}
inline void IAnimatable::setTimeScale(double scale) {
    m_scale = scale;
    afterSetTimeScale(scale);
}
inline int IAnimatable::getScaledAnimationTime(int ms) const {
    return static_cast<int>(ms * m_scale);
}

#endif // IANIMATABLE_H
