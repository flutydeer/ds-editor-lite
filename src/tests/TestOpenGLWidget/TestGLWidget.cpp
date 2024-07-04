//
// Created by fluty on 2024/7/4.
//

#include "TestGLWidget.h"

#include <QMouseEvent>
#include <QOpenGLContext>

TestGLWidget::TestGLWidget(QWidget *parent, Qt::WindowFlags f) : QOpenGLWidget(parent, f) {
    setAttribute(Qt::WA_Hover, true);
    installEventFilter(this);

    // QSurfaceFormat surfaceFormat;
    // surfaceFormat.setSamples(4);
    // setFormat(surfaceFormat);
}
void TestGLWidget::initializeGL() {
    // QOpenGLWidget::initializeGL();
    auto f = QOpenGLContext::currentContext()->functions();
    initializeOpenGLFunctions();
    glClearColor(32 /255.0, 33 /255.0, 34 /255.0, 1.0f /255.0);
}
void TestGLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h, 0, -1, 1);
    // QOpenGLWidget::resizeGL(w, h);
}
void TestGLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);
    glColor3f(155 /255.0, 186 / 255.0, 255 / 255.0);
    for (int i = 0; i < 20000; i++) {
        // drawRoundRect(m_mousePos.x() + i * 8, m_mousePos.y() + i * 32, 64, 32, 6);
        drawRect(m_mousePos.x() + i * 4, m_mousePos.y() + i * 4, 4, 4);
    }
    glFlush();
    glEnd();
    // QOpenGLWidget::paintGL();
}
bool TestGLWidget::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() == QEvent::HoverMove) {
        auto hoverEvent = dynamic_cast<QHoverEvent *>(event);
        m_mousePos = hoverEvent->position();
        update();
    }
    return QOpenGLWidget::eventFilter(watched, event);
}
void TestGLWidget::drawRect(float x, float y, float width, float height) {
    glRectf(x, y, x + width, y + height);
    // glFlush();
    // glEnd();
}
void TestGLWidget::drawRoundRect(float x, float y, float width, float height, float radius) {
    glBegin(GL_POLYGON);
    for (int i = 0; i <= 90; ++i) {
        float angle = i * M_PI / 180;
        glVertex2f(x + width - radius + radius * cos(angle), y + radius - radius * sin(angle));
    }
    for (int i = 90; i <= 180; ++i) {
        float angle = i * M_PI / 180;
        glVertex2f(x + radius + radius * cos(angle), y + radius - radius * sin(angle));
    }
    for (int i = 180; i <= 270; ++i) {
        float angle = i * M_PI / 180;
        glVertex2f(x + radius + radius * cos(angle), y + height - radius - radius * sin(angle));
    }
    for (int i = 270; i <= 360; ++i) {
        float angle = i * M_PI / 180;
        glVertex2f(x + width - radius + radius * cos(angle),
                   y + height - radius - radius * sin(angle));
    }
    glEnd();
}