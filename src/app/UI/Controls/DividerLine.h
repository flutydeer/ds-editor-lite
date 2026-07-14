//
// Created by FlutyDeer on 24-3-18.
//

#ifndef DIVIDERLINE_H
#define DIVIDERLINE_H

#include <QColor>
#include <QWidget>

class DividerLine : public QWidget {
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth)
    Q_PROPERTY(int lineMargin READ lineMargin WRITE setLineMargin)
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor)

public:
    explicit DividerLine(QWidget *parent = nullptr);
    explicit DividerLine(Qt::Orientation orientation, QWidget *parent = nullptr);

    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);

    int lineWidth() const;
    void setLineWidth(int width);

    int lineMargin() const;
    void setLineMargin(int margin);

    QColor lineColor() const;
    void setLineColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

private:
    Qt::Orientation m_orientation = Qt::Horizontal;
    int m_lineWidth = 1;
    int m_lineMargin = 0;
    QColor m_lineColor{29, 31, 38}; // #1D1F26

    void initSizePolicy();
};

#endif // DIVIDERLINE_H
