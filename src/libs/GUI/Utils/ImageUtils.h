//
// Created by fluty on 24-8-16.
//

#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QPixmap>
#include <QImage>

// #include <opencv2/opencv.hpp>

class ImageUtils {
public:
    static QPixmap gaussianBlur(const QPixmap &pixmap, int blurRadius = 18,
                                double brightFactor = 1.0, QSize blurPicSize = QSize());
};



#endif // IMAGEUTILS_H
