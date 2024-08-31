//
// Created by fluty on 24-8-19.
//

#ifndef PIANOKEYBOARDVIEW_H
#define PIANOKEYBOARDVIEW_H

#include <QWidget>

class PianoKeyboardView : public QWidget {
    Q_OBJECT

public:
    enum KeyboardStyle { Uniform, Classic };

    explicit PianoKeyboardView(QWidget *parent = nullptr);

public slots:
    void setKeyRange(double top, double bottom);

signals:
    void wheelScroll(QWheelEvent *event);

private:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *e) override;
    void drawUniformKeyboard(QPainter &painter);
    void drawClassicKeyboard(QPainter &painter) const;

    double m_top = 0;
    double m_bottom = 127;
    KeyboardStyle m_style = Classic;

    const double penWidth = 1;
    const QColor colorWhite = QColor(220, 220, 220);
    const QColor colorBlack = QColor(62, 63, 68);
    const QColor lineColor = QColor(160, 160, 160);
};



#endif // PIANOKEYBOARDVIEW_H
