//
// Created by fluty on 2024/7/18.
//

#ifndef PIANOKEYBOARDVIEW_H
#define PIANOKEYBOARDVIEW_H

#include "UI/Utils/IScalable.h"

#include <QOpenGLFunctions>
#include <QOpenGLWidget>

class PianoKeyboardView : public QOpenGLWidget, QOpenGLFunctions {
    Q_OBJECT

public:
    explicit PianoKeyboardView(QWidget *parent = nullptr);

public slots:
    void setKeyIndexRange(double startKeyIndex, double endKeyIndex);

private:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    double m_startKeyIndex = 127;
    double m_endKeyIndex = 0;
};



#endif // PIANOKEYBOARDVIEW_H
