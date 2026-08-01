#ifndef IANIMATABLE_H
#define IANIMATABLE_H

#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Animation/AnimationGlobal.h>

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
    [[nodiscard]] int getEffectiveAnimationTime(
        int ms, AnimationGlobal::AnimationLevels minimumLevel = AnimationGlobal::Decreased) const;

    virtual void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) = 0;
    virtual void afterSetTimeScale(double scale) = 0;
    void initializeAnimation();

private:
    AnimationGlobal::AnimationLevels m_level = AnimationGlobal::Full;
    double m_scale = 1.0;
    bool m_initialized = false;
};

inline IAnimatable::IAnimatable() {
    ThemeManager::instance()->addAnimationObserver(this);
}

inline IAnimatable::~IAnimatable() {
    ThemeManager::instance()->removeAnimationObserver(this);
}

inline AnimationGlobal::AnimationLevels IAnimatable::animationLevel() const {
    return m_level;
}

inline void IAnimatable::setAnimationLevel(AnimationGlobal::AnimationLevels level) {
    m_level = level;
    if (m_initialized)
        afterSetAnimationLevel(level);
}

inline double IAnimatable::animationTimeScale() const {
    return m_scale;
}

inline void IAnimatable::setTimeScale(double scale) {
    m_scale = scale;
    if (m_initialized)
        afterSetTimeScale(scale);
}

inline int IAnimatable::getScaledAnimationTime(int ms) const {
    return static_cast<int>(ms * m_scale);
}

inline int
    IAnimatable::getEffectiveAnimationTime(int ms,
                                           AnimationGlobal::AnimationLevels minimumLevel) const {
    if (m_level == AnimationGlobal::None ||
        (minimumLevel == AnimationGlobal::Full && m_level != AnimationGlobal::Full)) {
        return 0;
    }
    return getScaledAnimationTime(ms);
}

inline void IAnimatable::initializeAnimation() {
    m_initialized = true;
    afterSetAnimationLevel(animationLevel());
    afterSetTimeScale(animationTimeScale());
}

#endif // IANIMATABLE_H
