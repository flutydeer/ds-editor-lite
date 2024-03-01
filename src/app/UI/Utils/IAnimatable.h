//
// Created by fluty on 24-3-1.
//

#ifndef IANIMATABLE_H
#define IANIMATABLE_H

#include "Global/AnimationGlobal.h"

using namespace AnimationGlobal;

class IAnimatable {
public:
    virtual ~IAnimatable() = default;
    [[nodiscard]] AnimationLevel animationLevel() const;
    void setAnimationLevel(AnimationLevel level);
    [[nodiscard]] double animationTimeScale() const;
    void setTimeScale(double scale);

protected:
    [[nodiscard]] int getScaledAnimationTime(int ms) const;

    virtual void afterSetAnimationLevel(AnimationLevel level) = 0;
    virtual void afterSetTimeScale(double scale) = 0;

private:
    AnimationLevel m_level = AnimationGlobal::animationLevel;
    double m_scale = AnimationGlobal::timeScale;
};
inline AnimationLevel IAnimatable::animationLevel() const {
    return m_level;
}
inline void IAnimatable::setAnimationLevel(AnimationLevel level) {
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
