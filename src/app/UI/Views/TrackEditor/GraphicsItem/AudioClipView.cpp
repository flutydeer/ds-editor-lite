//
// Created by fluty on 2023/11/16.
//

#include "AudioClipView.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QThread>
#include <QFileDialog>

AudioClipView::AudioClipView(const int itemId, QGraphicsItem *parent)
    : AbstractClipView(itemId, parent) {
}

QString AudioClipView::path() const {
    return m_path;
}

void AudioClipView::setPath(const QString &path) {
    m_path = path;
    m_status = AppGlobal::Loaded;
}

double AudioClipView::tempo() const {
    return m_tempo;
}

void AudioClipView::setTempo(const double tempo) {
    m_tempo = tempo;
}

void AudioClipView::setAudioInfo(const AudioInfoModel &info) {
    m_audioInfo = info;
    update();
}

void AudioClipView::setStatus(const AppGlobal::AudioLoadStatus status) {
    m_status = status;
    update();
}

void AudioClipView::setErrorMessage(const QString &errorMessage) {
    m_errorMessage = errorMessage;
    update();
}

int AudioClipView::contentLength() const {
    return length();
}

void AudioClipView::onTempoChange(const double tempo) {
    // qDebug() << "AudioClipGraphicsItem::onTempoChange" << tempo;
    m_tempo = tempo;
}

void AudioClipView::drawPreviewArea(QPainter *painter, const QRectF &previewRect,
                                    const QColor color) {
    QElapsedTimer mstimer;
    mstimer.start();
    
    // Get device pixel ratio for high DPI support
    const qreal devicePixelRatio = painter->device()->devicePixelRatio();
    
    // Create off-screen pixmap with correct device pixel ratio
    const int pixmapWidth = static_cast<int>(previewRect.width() * devicePixelRatio);
    const int pixmapHeight = static_cast<int>(previewRect.height() * devicePixelRatio);
    
    QPixmap pixmap(pixmapWidth, pixmapHeight);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    
    QPainter pixmapPainter(&pixmap);
    pixmapPainter.setRenderHint(QPainter::Antialiasing, false);
    
    // auto rectLeft = previewRect.left();
    // qDebug() << rectLeft;
    const auto rectTop = 0; // Use 0 for pixmap coordinate system
    const auto rectWidth = previewRect.width();
    const auto rectHeight = previewRect.height();
    const auto halfRectHeight = rectHeight / 2;

    QPen pen;

    if (m_status == AppGlobal::Loading) {
        pen.setColor(color);
        pixmapPainter.setPen(pen);
        pixmapPainter.drawText(QRectF(0, 0, rectWidth, rectHeight), "Loading...", QTextOption(Qt::AlignCenter));
    }

    pen.setColor(color);
    pen.setWidth(1);
    pixmapPainter.setPen(pen);

    if (m_audioInfo.peakCache.count() == 0 || m_audioInfo.peakCacheMipmap.count() == 0) {
        // Draw the pixmap to the main painter
        painter->drawPixmap(previewRect.topLeft(), pixmap);
        return;
    }

    m_resolution = scaleX() >= 0.2 ? High : Low;
    const auto chunksPerTickBase =
        static_cast<double>(m_audioInfo.sampleRate) / m_audioInfo.chunkSize * 60 / m_tempo / 480;
    const auto peakData = m_resolution == Low ? m_audioInfo.peakCacheMipmap : m_audioInfo.peakCache;
    const auto chunksPerTick =
        m_resolution == Low ? chunksPerTickBase / m_audioInfo.mipmapScale : chunksPerTickBase;

    const auto rectLeftScene = mapToScene(previewRect.topLeft()).x();
    const auto rectRightScene = mapToScene(previewRect.bottomRight()).x();
    const auto waveRectLeft =
        visibleRect().left() < rectLeftScene ? 0 : visibleRect().left() - rectLeftScene;
    const auto waveRectRight = visibleRect().right() < rectRightScene
                                   ? visibleRect().right() - rectLeftScene
                                   : rectRightScene - rectLeftScene;
    // auto waveRectWidth = waveRectRight - waveRectLeft + 1; // 1 px spaceing at right

    const auto start = clipStart() * chunksPerTick;
    const auto end = (clipStart() + clipLen()) * chunksPerTick;
    const auto divideCount = (end - start) / rectWidth;
    // qDebug() << start << end << rectWidth << (int)divideCount;

    auto drawPeak = [&](const int x, const short min, const short max) {
        const auto yMin = -min * halfRectHeight / 32767 + halfRectHeight + rectTop;
        const auto yMax = -max * halfRectHeight / 32767 + halfRectHeight + rectTop;
        pixmapPainter.drawLine(x, static_cast<int>(yMin), x, static_cast<int>(yMax));
    };

    // qDebug() << m_peakCacheThumbnail.count() << divideCount;
    for (int i = static_cast<int>(waveRectLeft); i <= static_cast<int>(waveRectRight); i++) {
        short min = 0;
        short max = 0;
        auto updateMinMax = [](const std::tuple<short, short> &frame, short &min_, short &max_) {
            const auto frameMin = std::get<0>(frame);
            const auto frameMax = std::get<1>(frame);
            if (frameMin < min_)
                min_ = frameMin;
            if (frameMax > max_)
                max_ = frameMax;
        };

        for (int j = start + i * divideCount; j < start + i * divideCount + divideCount; j++) {
            if (j >= peakData.count())
                break;

            const auto &frame = peakData.at(j);
            updateMinMax(frame, min, max);
        }
        // if (i == rectWidth - 1) {
        //     // for (int j = start + i * (divideCount + 1); j < m_peakCache.count(); j++) {
        //     for (int j = start + i * (divideCount + 1); j < peakData.count(); j++) {
        //         // auto frame = m_peakCache.at(j);
        //         auto frame = peakData.at(j);
        //         updateMinMax(frame, min, max);
        //     }
        // }
        drawPeak(i, min, max);
    }

    // Draw the pixmap to the main painter
    painter->drawPixmap(previewRect.topLeft(), pixmap);

    // const auto time = static_cast<double>(mstimer.nsecsElapsed()) / 1000000.0;
    // qDebug() << time;

    // Draw visible area
    // auto waveRectTop = previewRect.top();
    // auto waveRectHeight = previewRect.height();
    // auto waveRect = QRectF(waveRectLeft, waveRectTop, waveRectWidth, waveRectHeight);
    //
    // pen.setColor(QColor(255, 0, 0));
    // pen.setWidth(1);
    // painter->setPen(pen);
    // painter->drawRect(waveRect);
    // painter->drawLine(waveRect.topLeft(), waveRect.bottomRight());
    // painter->drawLine(waveRect.topRight(), waveRect.bottomLeft());
}

QString AudioClipView::clipTypeName() const {
    return tr("[Audio] ");
}

QString AudioClipView::iconPath() const {
    return ":svg/icons/audio_clip_16_filled.svg";
}