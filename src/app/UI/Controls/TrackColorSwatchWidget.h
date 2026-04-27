//
// Created on 2026/4/27.
//

#ifndef TRACKCOLORSWATCHWIDGET_H
#define TRACKCOLORSWATCHWIDGET_H

#include <QWidget>

class TrackColorSwatchWidget : public QWidget {
    Q_OBJECT

public:
    explicit TrackColorSwatchWidget(int currentIndex = -1, QWidget *parent = nullptr);

signals:
    void colorIndexSelected(int index);
    void colorIndexHovered(int index);
    void previewCancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int indexAt(const QPoint &pos) const;
    QRect swatchRect(int index) const;

    int m_currentIndex = -1;
    int m_hoveredIndex = -1;

    static constexpr int columns = 4;
    static constexpr int rows = 3;
    static constexpr int swatchSize = 20;
    static constexpr int spacing = 4;
    static constexpr int padding = 6;
};

#endif // TRACKCOLORSWATCHWIDGET_H
