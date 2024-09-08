//
// Created by fluty on 24-9-9.
//

#ifndef DROPSHADOW_H
#define DROPSHADOW_H

#define DROP_SHADOW_EFFECT                                                                         \
    Q_PROPERTY(QString dropShadow READ dropShadow WRITE setDropShadow)                             \
public:                                                                                            \
    [[nodiscard]] QString dropShadow() const {                                                     \
        return m_dropShadow;                                                                       \
    }                                                                                              \
                                                                                                   \
    void setDropShadow(const QString &text) {                                                      \
        m_dropShadow = text;                                                                       \
        if (m_dropShadow.isNull() || m_dropShadow.isEmpty())                                       \
            return;                                                                                \
        auto list = m_dropShadow.split(" ");                                                       \
        auto offsetX = list[0].toDouble();                                                         \
        auto offsetY = list[1].toDouble();                                                         \
        auto blurRadius = list[2].toDouble();                                                      \
        auto color = QColor(list[3]);                                                              \
        m_shadowEffect.setBlurRadius(blurRadius);                                                  \
        m_shadowEffect.setColor(color);                                                            \
        m_shadowEffect.setOffset(offsetX, offsetY);                                                \
        setGraphicsEffect(&m_shadowEffect);                                                        \
        update();                                                                                  \
    }                                                                                              \
                                                                                                   \
private:                                                                                           \
    QString m_dropShadow;                                                                          \
    QGraphicsDropShadowEffect m_shadowEffect;

#endif // DROPSHADOW_H
