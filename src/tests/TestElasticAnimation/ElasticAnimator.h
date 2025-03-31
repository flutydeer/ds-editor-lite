//
// Created by FlutyDeer on 2025/4/1.
//

#ifndef ELASTICANIMATOR_H
#define ELASTICANIMATOR_H

#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QList>

class ElasticAnimator : public QObject {
    Q_OBJECT
public:
    explicit ElasticAnimator(QObject *parent = nullptr);

    void setSmoothness(qreal value);
    void setResponsiveness(qreal value);
    void setTarget(const QPointF &target);

    QPointF position() const;
    QPointF velocity() const;

signals:
    void positionUpdated(QPointF position, QPointF velocity);

private slots:
    void updatePosition();

private:
    QTimer *m_timer;
    qreal m_smoothness;
    qreal m_responsiveness;

    QPointF m_position;
    QPointF m_velocity;
    QPointF m_target;
};

#endif // ELASTICANIMATOR_H