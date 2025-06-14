//
// Created by FlutyDeer on 2025/6/14.
//

#include "HSLGradientArea.h"

#include <QPainter>

HSLGradientArea::HSLGradientArea(QWidget *parent) : QWidget(parent) {
}

void HSLGradientArea::resizeEvent(QResizeEvent *event) {
    QPainter painter(this);
    auto dpr = painter.device()->devicePixelRatio();
    auto physicalWidth = qRound(width() * dpr);
    auto physicalHeight = qRound(height() * dpr);
    QImage image(physicalWidth, physicalHeight, QImage::Format_ARGB32);

    for (int x = 0; x < image.width(); ++x) {
        auto factor = 1.0 * x / image.width();
        auto color = getInterpolatedColor(colorStart_, colorEnd_, factor);
        for (int y = 0; y < image.height(); ++y)
            image.setPixel(x, y, color.rgba());
    }

    pixmap_ = QPixmap::fromImage(image);
    pixmap_.setDevicePixelRatio(1);
    QWidget::resizeEvent(event);
}

void HSLGradientArea::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.drawPixmap(rect(), pixmap_);
}

float HSLGradientArea::normalizeHue(float hue) {
    hue = fmod(hue, 360.0);
    if (hue < 0)
        hue += 360.0;
    return hue;
}

float HSLGradientArea::hueDifference(float h1, float h2) {
    h1 = normalizeHue(h1);
    h2 = normalizeHue(h2);

    float diff = h2 - h1;

    // 考虑环形特性，找出最短路径
    if (diff > 180.0) {
        diff -= 360.0;
    } else if (diff < -180.0) {
        diff += 360.0;
    }

    return diff;
}

// 获取渐变中的中间颜色
QColor HSLGradientArea::getInterpolatedColor(const QColor &c1, const QColor &c2, double factor) {
    // 提取HSL值
    float h1, s1, l1, a1;
    float h2, s2, l2, a2;

    c1.getHslF(&h1, &s1, &l1, &a1);
    c2.getHslF(&h2, &s2, &l2, &a2);

    // 规范化色相
    h1 = normalizeHue(h1 * 360.0);
    h2 = normalizeHue(h2 * 360.0);

    // 计算两种可能的路径
    qreal directDiff = h2 - h1;
    qreal wrappedDiff = (h2 > h1) ? (h2 - 360.0 - h1) : (h2 + 360.0 - h1);

    // 选择最短路径，但保持原始顺序
    qreal hueDiff = (qAbs(directDiff) <= qAbs(wrappedDiff)) ? directDiff : wrappedDiff;

    // 插值计算
    double h = h1 + factor * hueDiff;
    double s = s1 + factor * (s2 - s1);
    double l = l1 + factor * (l2 - l1);
    double a = a1 + factor * (a2 - a1);

    // 规范化结果色相
    h = normalizeHue(h) / 360.0;

    QColor result;
    result.setHslF(h, s, l, a);
    return result;
}