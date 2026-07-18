//
// Created on 2026/4/27.
//

#ifndef TRACKCOLORSWATCHWIDGET_H
#define TRACKCOLORSWATCHWIDGET_H

#include <QWidget>

class TrackColorSwatchWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor selectedBorderColor READ selectedBorderColor WRITE setSelectedBorderColor)
    Q_PROPERTY(QColor hoverBorderColor READ hoverBorderColor WRITE setHoverBorderColor)

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
    [[nodiscard]] QColor selectedBorderColor() const;
    void setSelectedBorderColor(const QColor &color);
    [[nodiscard]] QColor hoverBorderColor() const;
    void setHoverBorderColor(const QColor &color);

    int m_currentIndex = -1;
    int m_hoveredIndex = -1;
    // NOTE: transitional; may move to AppColorPalette (per-index or
    // contrast-derived) since these borders draw on top of all base colors
    QColor m_selectedBorderColor = QColor(255, 255, 255);
    QColor m_hoverBorderColor = QColor(255, 255, 255, 128);

    static constexpr int columns = 4;
    static constexpr int rows = 3;
    static constexpr int swatchSize = 20;
    static constexpr int spacing = 4;
    static constexpr int padding = 6;
};

#endif // TRACKCOLORSWATCHWIDGET_H
