//
// Created by fluty on 24-8-16.
//

#include "AcrylicBrush.h"

#include "ImageUtils.h"

#include <QApplication>
#include <QPainter>
#include <QScreen>

AcrylicBrush::AcrylicBrush(QWidget *device, int blurRadius, QColor tintColor,
                           QColor luminosityColor, double noiseOpacity)
    : device(device), blurRadius(blurRadius), tintColor(tintColor),
      luminosityColor(luminosityColor), noiseOpacity(noiseOpacity) {
}

void AcrylicBrush::setBlurRadius(int radius) {
    if (radius == blurRadius)
        return;
    blurRadius = radius;
    setImage(originalImage);
}

void AcrylicBrush::setTintColor(const QColor &color) {
    tintColor = color;
    device->update();
}

void AcrylicBrush::setLuminosityColor(const QColor &color) {
    luminosityColor = color;
    device->update();
}

void AcrylicBrush::grabImage(const QRect &rect) {
    QScreen *screen = QApplication::primaryScreen();
    if (!screen)
        return;

    QRect screenRect = screen->geometry();
    QRect grabRect(rect.x() - screenRect.x(), rect.y() - screenRect.y(), rect.width(),
                   rect.height());

    auto image =
        screen->grabWindow(0, grabRect.x(), grabRect.y(), grabRect.width(), grabRect.height());
    setImage(image);
}

void AcrylicBrush::setImage(const QPixmap &image) {
    originalImage = image;
    if (!image.isNull()) {
        this->image = ImageUtils::gaussianBlur(image, blurRadius);
    }
    device->update();
}

void AcrylicBrush::setClipPath(const QPainterPath &path) {
    clipPath = path;
    device->update();
}

QImage AcrylicBrush::textureImage() const {
    QImage texture(64, 64, QImage::Format_ARGB32_Premultiplied);
    texture.fill(luminosityColor);

    QPainter painter(&texture);
    painter.fillRect(texture.rect(), tintColor);
    painter.setOpacity(noiseOpacity);
    // painter.drawImage(texture.rect(), noiseImage);

    return texture;
}

void AcrylicBrush::paint() const {
    QPainter painter(device);
    painter.setRenderHints(QPainter::Antialiasing);

    if (!clipPath.isEmpty()) {
        painter.setClipPath(clipPath);
    }

    QPixmap scaledImage =
            image.scaled(device->size() * 1.25, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    painter.drawPixmap(0, 0, scaledImage);
    // painter.fillRect(device->rect(), QBrush(textureImage()));
}