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

    // TODO: use QVariantAnimation
    Q_PROPERTY(double apparentValue READ apparentValue WRITE setApparentValue)
    Q_PROPERTY(int thumbScaleRatio READ thumbScaleRatio WRITE setThumbScaleRatio)

public:
    explicit SwitchButton(QWidget *parent = nullptr);
    explicit SwitchButton(bool on, QWidget *parent = nullptr);
    ~SwitchButton() override;

    bool value() const;

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
    // bool m_value = false;

    QPropertyAnimation m_valueAnimation;
    QPropertyAnimation m_thumbHoverAnimation;

    void initUi();

    // Animation
    int m_apparentValue = 0;
    int apparentValue() const;
    void setApparentValue(int x);

    int m_thumbScaleRatio = 100; // max = 100%
    int thumbScaleRatio() const;
    void setThumbScaleRatio(int ratio);

    void updateAnimationDuration();

    double m_vPadding = 4;
};



#endif // DATASET_TOOLS_SWITCHBUTTON_H
