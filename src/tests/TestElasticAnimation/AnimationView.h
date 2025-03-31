//
// Created by FlutyDeer on 2025/4/1.
//

#ifndef ANIMATIONVIEW_H
#define ANIMATIONVIEW_H

#include <QWidget>

#include "ElasticAnimator.h"

class AnimationView : public QWidget {
    Q_OBJECT

public:
    explicit AnimationView(QWidget *parent = nullptr);

private slots:
    void onPositionUpdated(QPointF position, QPointF velocity);

private:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

    ElasticAnimator m_animator;
    QPointF m_objectPosition;
};

#endif // ANIMATIONVIEW_H