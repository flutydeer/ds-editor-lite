//
// Created by fluty on 2024/2/3.
//

#ifndef ISCALABLE_H
#define ISCALABLE_H

class IScalable {
public:
    virtual ~IScalable() = default;

    [[nodiscard]] double scaleX() const;
    virtual void setScaleX(double scaleX);
    [[nodiscard]] double scaleY() const;
    virtual void setScaleY(double scaleY);
    void setScaleXY(double scaleX, double scaleY);

protected:
    virtual void afterSetScale() = 0;

private:
    double m_scaleX = 1;
    double m_scaleY = 1;
};
inline double IScalable::scaleX() const {
    return m_scaleX;
}
inline void IScalable::setScaleX(double scaleX) {
    m_scaleX = scaleX;
    afterSetScale();
}
inline double IScalable::scaleY() const {
    return m_scaleY;
}
inline void IScalable::setScaleY(double scaleY) {
    m_scaleY = scaleY;
    afterSetScale();
}
inline void IScalable::setScaleXY(double scaleX, double scaleY) {
    m_scaleX = scaleX;
    m_scaleY = scaleY;
    afterSetScale();
}

#endif // ISCALABLE_H
