//
// Created by fluty on 2024/7/18.
//

#include "PianoKeyboardView.h"

#include "Global/AppGlobal.h"

#include <QPainter>

PianoKeyboardView::PianoKeyboardView(QWidget *parent) : QOpenGLWidget(parent) {
    setFixedWidth(64);
}

void PianoKeyboardView::setKeyIndexRange(double startKeyIndex, double endKeyIndex) {
    m_startKeyIndex = startKeyIndex;
    m_endKeyIndex = endKeyIndex;
    update();
}

void PianoKeyboardView::initializeGL() {
    // QOpenGLWidget::initializeGL();
    initializeOpenGLFunctions();
    glClearColor(0 / 255.0, 0 / 255.0, 0 / 255.0, 1.0f);
}

void PianoKeyboardView::resizeGL(int w, int h) {
    glViewport(0, 0, w, h - AppGlobal::horizontalScrollBarWidth);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, w, h - AppGlobal::horizontalScrollBarWidth, 0, -1, 1);
    // QOpenGLWidget::resizeGL(w, h);
}

void PianoKeyboardView::paintGL() {
    QPainter painter(this);
    QPen pen;
    pen.setWidth(1);
    glClear(GL_COLOR_BUFFER_BIT);

    auto isWhiteKey = [](const int midiKey) -> bool {
        int index = midiKey % 12;
        bool pianoKeys[] = {true,  false, true,  false, true,  true,
                            false, true,  false, true,  false, true};
        return pianoKeys[index];
    };

    auto toNoteName = [](const int &midiKey) {
        QString noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
        int index = qAbs(midiKey) % 12;
        int octave = midiKey / 12 - 1;
        QString noteName = noteNames[index] + QString::number(octave);
        return noteName;
    };

    auto prevKeyIndex = static_cast<int>(m_startKeyIndex) + 1;
    auto keyHeight =
        (height() - AppGlobal::horizontalScrollBarWidth) / (m_startKeyIndex - m_endKeyIndex);
    qDebug() << keyHeight << m_startKeyIndex << m_endKeyIndex;
    for (int i = prevKeyIndex; i > m_endKeyIndex; i--) {
        auto y = (m_startKeyIndex - i) * keyHeight;

        auto b = isWhiteKey(i) ? 255.0f : 0.0f;
        glColor3f(b / 255.0f, b / 255.0f, b / 255.0f);
        glRectf(0, y, width(), y + keyHeight);
        qDebug() << y << y + keyHeight;

        auto keyIndexTextColor = isWhiteKey(i) ? QColor(20, 20, 20) : QColor(240, 240, 240);
        auto keyRect = QRect(0, y, width(), y + keyHeight);
        pen.setColor(keyIndexTextColor);
        painter.setPen(pen);
        painter.drawText(keyRect, toNoteName(i), QTextOption(Qt::AlignVCenter));
    }

    glFlush();
    glEnd();
    // QOpenGLWidget::paintGL();
}