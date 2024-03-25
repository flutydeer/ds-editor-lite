//
// Created by fluty on 2023/8/17.
//

#ifndef DATASET_TOOLS_SWITCHBUTTON_H
#define DATASET_TOOLS_SWITCHBUTTON_H

#include <QAbstractButton>

#include "UI/Utils/IAnimatable.h"

class QPropertyAnimation;

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

    [[nodiscard]] bool value() const;

public slots:
    void setValue(bool value);

signals:
    void valueChanged(bool value);

protected:
    using QAbstractButton::isChecked;
    using QAbstractButton::setChecked;
    // bool m_value = false;

    QRect m_rect;
    //    int m_penWidth;
    int m_thumbRadius{};
    int m_halfRectHeight{};
    QPoint m_trackStart;
    QPoint m_trackEnd;
    int m_trackLength{};
    QPropertyAnimation *m_valueAnimation{};
    QPropertyAnimation *m_thumbHoverAnimation{};

    void initUi();
    void calculateParams();
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *object, QEvent *event) override;
    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override;
    void afterSetTimeScale(double scale) override;

private:
    // Animation
    int m_apparentValue = 0;
    [[nodiscard]] int apparentValue() const;
    void setApparentValue(int x);

    int m_thumbScaleRatio = 100; // max = 100%
    [[nodiscard]] int thumbScaleRatio() const;
    void setThumbScaleRatio(int ratio);

    void updateAnimationDuration();
};



#endif // DATASET_TOOLS_SWITCHBUTTON_H
