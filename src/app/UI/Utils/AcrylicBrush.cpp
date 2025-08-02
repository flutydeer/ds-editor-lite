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
    : m_device(device), m_blurRadius(blurRadius), m_tintColor(tintColor),
      m_luminosityColor(luminosityColor), m_noiseOpacity(noiseOpacity) {
}

void AcrylicBrush::setBlurRadius(int radius) {
    if (radius == m_blurRadius)
        return;
    m_blurRadius = radius;
    setImage(m_originalImage);
}

void AcrylicBrush::setTintColor(const QColor &color) {
    m_tintColor = color;
    m_device->update();
}

void AcrylicBrush::setLuminosityColor(const QColor &color) {
    m_luminosityColor = color;
    m_device->update();
}

void AcrylicBrush::grabImage(const QRect &rect) {
    QScreen *screen = QApplication::primaryScreen();
    if (!screen)
        return;

    QRect screenRect = screen->geometry();
    QRect grabRect(rect.x() - screenRect.x(), rect.y() - screenRect.y(), rect.width(),
                   rect.height());

    auto grabbedImage =
        screen->grabWindow(0, grabRect.x(), grabRect.y(), grabRect.width(), grabRect.height());
    setImage(grabbedImage);
}

void AcrylicBrush::setImage(const QPixmap &image) {
    m_originalImage = image;
    if (!image.isNull()) {
        m_image = ImageUtils::gaussianBlur(image, m_blurRadius);
    }
    m_device->update();
}

void AcrylicBrush::setClipPath(const QPainterPath &path) {
    m_clipPath = path;
    m_device->update();
}

QImage AcrylicBrush::textureImage() const {
    QImage texture(64, 64, QImage::Format_ARGB32_Premultiplied);
    texture.fill(m_luminosityColor);

    QPainter painter(&texture);
    painter.fillRect(texture.rect(), m_tintColor);
    painter.setOpacity(m_noiseOpacity);
    // painter.drawImage(texture.rect(), noiseImage);

    return texture;
}

void AcrylicBrush::paint() const {
    QPainter painter(m_device);
    painter.setRenderHints(QPainter::Antialiasing);

    if (!m_clipPath.isEmpty()) {
        painter.setClipPath(m_clipPath);
    }

    QPixmap scaledImage = m_image.scaled(m_device->size() * 1.25, Qt::KeepAspectRatioByExpanding,
                                       Qt::SmoothTransformation);
    painter.drawPixmap(0, 0, scaledImage);
    // painter.fillRect(device->rect(), QBrush(textureImage()));
}