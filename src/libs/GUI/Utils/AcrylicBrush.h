//
// Created by fluty on 24-8-16.
//

#ifndef ACRYLICBRUSH_H
#define ACRYLICBRUSH_H

#include <QWidget>
#include <QColor>
#include <QPixmap>
#include <QImage>
#include <QPainterPath>

// Port from
// https://github.com/zhiyiYo/PyQt-Fluent-Widgets/blob/master/qfluentwidgets/components/widgets/acrylic_label.py
class AcrylicBrush {
public:
    AcrylicBrush(QWidget *device, int blurRadius, QColor tintColor = QColor(242, 242, 242, 150),
                 QColor luminosityColor = QColor(255, 255, 255, 10), double noiseOpacity = 0.03);

    void setBlurRadius(int radius);
    void setTintColor(const QColor &color);
    void setLuminosityColor(const QColor &color);
    void grabImage(const QRect &rect);
    void setImage(const QPixmap &image);
    void setClipPath(const QPainterPath &path);
    QImage textureImage() const;
    void paint() const;

private:
    QWidget *m_device;
    int m_blurRadius;
    QColor m_tintColor;
    QColor m_luminosityColor;
    double m_noiseOpacity;
    QImage m_noiseImage;
    QPixmap m_originalImage;
    QPixmap m_image;
    QPainterPath m_clipPath;
};



#endif // ACRYLICBRUSH_H
