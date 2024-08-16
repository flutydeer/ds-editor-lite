//
// Created by fluty on 24-8-16.
//

#include "ImageUtils.h"

#include <QPainter>

Q_GUI_EXPORT extern void qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality,
                                      bool alphaOnly, int transposed = 0);

QPixmap ImageUtils::gaussianBlur(const QPixmap &pixmap, int blurRadius, double brightFactor,
                                 QSize blurPicSize) {
    QImage image = pixmap.toImage();
    QPixmap dst(image.size());
    QPainter painter(&dst);
    qt_blurImage(&painter, image, blurRadius, true, false);
    return dst;
}