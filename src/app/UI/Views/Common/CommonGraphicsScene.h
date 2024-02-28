//
// Created by fluty on 2024/1/23.
//

#ifndef COMMONGRAPHICSSCENE_H
#define COMMONGRAPHICSSCENE_H

#include <QGraphicsScene>

class CommonGraphicsScene : public QGraphicsScene {
public:
    explicit CommonGraphicsScene();
    ~CommonGraphicsScene() override = default;
    [[nodiscard]] QSizeF sceneSize() const;
    void setSceneSize(const QSizeF &size);
    [[nodiscard]] double scaleX() const;
    void setScaleX(double scaleX);
    [[nodiscard]] double scaleY() const;
    void setScaleY(double scaleY);

public slots:
    void setScale(qreal sx, qreal sy);

protected:
    virtual void updateSceneRect();

private:
    QSizeF m_sceneSize = QSizeF(1920, 1080);
    double m_scaleX = 1;
    double m_scaleY = 1;
};

#endif // COMMONGRAPHICSSCENE_H
