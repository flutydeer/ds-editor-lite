//
// Created by fluty on 24-8-19.
//

#ifndef PIANOKEYBOARDVIEW_H
#define PIANOKEYBOARDVIEW_H

#include <QWidget>

class PianoKeyboardView : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor whiteKeyColor READ whiteKeyColor WRITE setWhiteKeyColor)
    Q_PROPERTY(QColor blackKeyColor READ blackKeyColor WRITE setBlackKeyColor)
    Q_PROPERTY(QColor dividerColor READ dividerColor WRITE setDividerColor)

public:
    enum KeyboardStyle { Uniform, Classic };

    explicit PianoKeyboardView(QWidget *parent = nullptr);

public slots:
    void setKeyRange(double top, double bottom);

signals:
    void wheelScroll(QWheelEvent *event);

private:
    QColor whiteKeyColor() const;
    void setWhiteKeyColor(const QColor &whiteKeyColor);
    QColor blackKeyColor() const;
    void setBlackKeyColor(const QColor &blackKeyColor);
    QColor dividerColor() const;
    void setDividerColor(const QColor &dividerColor);

    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *e) override;
    void drawUniformKeyboard(QPainter &painter);
    void drawClassicKeyboard(QPainter &painter) const;

    double m_top = 0;
    double m_bottom = 127;
    KeyboardStyle m_style = Classic;

    const double penWidth = 1;
    QColor m_whiteKeyColor = {218, 219, 224};
    QColor m_blackKeyColor = {59, 63, 71};
    QColor m_dividerColor = {170, 172, 181};
};



#endif // PIANOKEYBOARDVIEW_H
