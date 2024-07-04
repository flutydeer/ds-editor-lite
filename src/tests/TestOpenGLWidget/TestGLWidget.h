//
// Created by fluty on 2024/7/4.
//

#ifndef TESTGLWIDGET_H
#define TESTGLWIDGET_H

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

class TestGLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
public:
    explicit TestGLWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

private:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    void drawRect(float x, float y, float width, float height);
    void drawRoundRect(float x, float y, float width, float height, float radius);

    QPointF m_mousePos;
};



#endif // TESTGLWIDGET_H
