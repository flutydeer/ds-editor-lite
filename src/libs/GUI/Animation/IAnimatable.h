#ifndef IANIMATABLE_H
#define IANIMATABLE_H

#include <lite/GUI/Theme/ThemeManager.h>

class IAnimatable {
public:
    IAnimatable();
    virtual ~IAnimatable();
    [[nodiscard]] bool animationEnabled() const;
    void setAnimationEnabled(bool enabled);
    [[nodiscard]] double animationTimeScale() const;
    void setTimeScale(double scale);

protected:
    [[nodiscard]] int getScaledAnimationTime(int ms) const;
    [[nodiscard]] int getEffectiveAnimationTime(int ms) const;

    virtual void afterSetAnimationEnabled(bool enabled) = 0;
    virtual void afterSetTimeScale(double scale) = 0;
    void initializeAnimation();

private:
    bool m_enabled = true;
    double m_scale = 1.0;
    bool m_initialized = false;
};

inline IAnimatable::IAnimatable() {
    ThemeManager::instance()->addAnimationObserver(this);
}

inline IAnimatable::~IAnimatable() {
    ThemeManager::instance()->removeAnimationObserver(this);
}

inline bool IAnimatable::animationEnabled() const {
    return m_enabled;
}

inline void IAnimatable::setAnimationEnabled(bool enabled) {
    m_enabled = enabled;
    if (m_initialized)
        afterSetAnimationEnabled(enabled);
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

inline int IAnimatable::getEffectiveAnimationTime(int ms) const {
    if (!m_enabled)
        return 0;
    return getScaledAnimationTime(ms);
}

inline void IAnimatable::initializeAnimation() {
    m_initialized = true;
    afterSetAnimationEnabled(animationEnabled());
    afterSetTimeScale(animationTimeScale());
}

#endif // IANIMATABLE_H
