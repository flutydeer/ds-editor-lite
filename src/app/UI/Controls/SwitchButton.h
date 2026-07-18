//
// Created by fluty on 2023/8/17.
//

#ifndef DATASET_TOOLS_SWITCHBUTTON_H
#define DATASET_TOOLS_SWITCHBUTTON_H

#include <QAbstractButton>
#include <QPropertyAnimation>

#include "UI/Utils/IAnimatable.h"

class SwitchButton : public QAbstractButton, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(bool value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(QColor trackOffColor READ trackOffColor WRITE setTrackOffColor)
    Q_PROPERTY(QColor trackOnColor READ trackOnColor WRITE setTrackOnColor)
    Q_PROPERTY(QColor thumbOffColor READ thumbOffColor WRITE setThumbOffColor)
    Q_PROPERTY(QColor thumbOnColor READ thumbOnColor WRITE setThumbOnColor)

    // TODO: use QVariantAnimation
    Q_PROPERTY(double apparentValue READ apparentValue WRITE setApparentValue)
    Q_PROPERTY(int thumbScaleRatio READ thumbScaleRatio WRITE setThumbScaleRatio)

public:
    explicit SwitchButton(QWidget *parent = nullptr);
    explicit SwitchButton(bool on, QWidget *parent = nullptr);
    ~SwitchButton() override;

    [[nodiscard]] bool value() const;

public slots:
    void setValue(bool value);

signals:
    void valueChanged(bool value);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private:
    using QAbstractButton::isChecked;
    using QAbstractButton::setChecked;

    QPropertyAnimation m_valueAnimation;
    QPropertyAnimation m_thumbHoverAnimation;

    void initUi();

    // Theme colors (QSS-overridable via qproperty-*)
    QColor m_trackOffColor = QColor(255, 255, 255, 16);
    [[nodiscard]] QColor trackOffColor() const;
    void setTrackOffColor(const QColor &color);
    QColor m_trackOnColor = QColor(155, 186, 255);
    [[nodiscard]] QColor trackOnColor() const;
    void setTrackOnColor(const QColor &color);
    QColor m_thumbOffColor = QColor(255, 255, 255);
    [[nodiscard]] QColor thumbOffColor() const;
    void setThumbOffColor(const QColor &color);
    QColor m_thumbOnColor = QColor(0, 0, 0);
    [[nodiscard]] QColor thumbOnColor() const;
    void setThumbOnColor(const QColor &color);

    // Animation
    int m_apparentValue = 0;
    [[nodiscard]] int apparentValue() const;
    void setApparentValue(int x);

    int m_thumbScaleRatio = 100; // max = 100%
    [[nodiscard]] int thumbScaleRatio() const;
    void setThumbScaleRatio(int ratio);

    void updateAnimationDuration();

    double m_vPadding = 4;
};



#endif // DATASET_TOOLS_SWITCHBUTTON_H
