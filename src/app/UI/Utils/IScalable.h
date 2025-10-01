//
// Created by fluty on 2024/2/3.
//

#ifndef ISCALABLE_H
#define ISCALABLE_H

class IScalable {
public:
    virtual ~IScalable() = default;

    double scaleX() const;
    void setScaleX(double scaleX);
    double scaleY() const;
    void setScaleY(double scaleY);
    void setScaleXY(double scaleX, double scaleY);

    bool useFixedScaleX() const;
    bool useFixedScaleY() const;
    void setUseFixedScaleX(bool on);
    void setUseFixedScaleY(bool on);
    double fixedScaleX() const;
    void setFixedScaleX(double scaleX);
    double fixedScaleY() const;
    void setFixedScaleY(double scaleY);
    void setFixedScaleXY(double scaleX, double scaleY);

protected:
    virtual void afterSetScale() = 0;

private:
    double m_scaleX = 1;
    double m_scaleY = 1;

    bool m_useFixedScaleX = false;
    bool m_useFixedScaleY = false;
    double m_fixedScaleX = 1;
    double m_fixedScaleY = 1;
};

inline double IScalable::scaleX() const {
    return m_useFixedScaleX ? m_fixedScaleX : m_scaleX;
}

inline void IScalable::setScaleX(const double scaleX) {
    m_scaleX = scaleX;
    afterSetScale();
}

inline double IScalable::scaleY() const {
    return m_useFixedScaleY ? m_fixedScaleY : m_scaleY;
}

inline void IScalable::setScaleY(const double scaleY) {
    m_scaleY = scaleY;
    afterSetScale();
}

inline void IScalable::setScaleXY(const double scaleX, const double scaleY) {
    if (!m_useFixedScaleX)
        m_scaleX = scaleX;
    if (!m_useFixedScaleY)
        m_scaleY = scaleY;
    afterSetScale();
}

inline bool IScalable::useFixedScaleX() const {
    return m_useFixedScaleX;
}

inline bool IScalable::useFixedScaleY() const {
    return m_useFixedScaleY;
}

inline void IScalable::setUseFixedScaleX(const bool on) {
    m_useFixedScaleX = on;
}

inline void IScalable::setUseFixedScaleY(const bool on) {
    m_useFixedScaleY = on;
}

inline double IScalable::fixedScaleX() const {
    return m_fixedScaleX;
}

inline void IScalable::setFixedScaleX(const double scaleX) {
    m_fixedScaleX = scaleX;
    setScaleX(scaleX);
}

inline double IScalable::fixedScaleY() const {
    return m_fixedScaleY;
}

inline void IScalable::setFixedScaleY(const double scaleY) {
    m_fixedScaleY = scaleY;
    setScaleY(scaleY);
}

inline void IScalable::setFixedScaleXY(const double scaleX, const double scaleY) {
    m_fixedScaleX = scaleX;
    m_fixedScaleY = scaleY;
    setScaleXY(scaleX, scaleY);
}

#endif // ISCALABLE_H
