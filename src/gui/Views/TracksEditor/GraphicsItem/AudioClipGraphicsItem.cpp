//
// Created by fluty on 2023/11/16.
//

#include <QDebug>
#include <QPainter>
#include <QThread>
#include <QFileDialog>

#include "AudioClipBackgroundWorker.h"
#include "AudioClipGraphicsItem.h"

AudioClipGraphicsItem::AudioClipGraphicsItem(int itemId, QGraphicsItem *parent)
    : AbstractClipGraphicsItem(itemId, parent) {
}
QString AudioClipGraphicsItem::path() const {
    return m_path;
}
void AudioClipGraphicsItem::setPath(const QString &path) {
    m_path = path;
    m_status = Loading;
    setName(QFileInfo(m_path).fileName());
    // if (length() == 0)
    //     setLength(3840);
    // if (clipLen() == 0)
    //     setClipLen(3840);

    m_worker = new AudioClipBackgroundWorker(path);
    connect(m_worker, &AudioClipBackgroundWorker::finished, this,
            &AudioClipGraphicsItem::onLoadComplete);
    // m_threadPool->start(m_worker);
    auto thread = new QThread;
    m_worker->moveToThread(thread);
    connect(thread, &QThread::started, m_worker, &AudioClipBackgroundWorker::run);
    thread->start();
}
double AudioClipGraphicsItem::tempo() const {
    return m_tempo;
}
void AudioClipGraphicsItem::setTempo(double tempo) {
    m_tempo = tempo;
    if (m_status == Loaded)
        updateLength();
}
void AudioClipGraphicsItem::onLoadComplete(bool success, QString errorMessage) {
    if (!success) {
        m_status = Error;
        qDebug() << "open file error" << errorMessage;
        return;
    }
    m_sampleRate = m_worker->sampleRate;
    m_channels = m_worker->channels;
    m_chunkSize = m_worker->chunkSize;
    m_mipmapScale = m_worker->mipmapScale;
    m_frames = m_worker->frames;

    m_peakCache.swap(m_worker->peakCache);
    m_peakCacheMipmap.swap(m_worker->peakCacheMipmap);
    delete m_worker;

    // setClipStart(0);
    updateLength();
    if (clipLen() == 0)
        setClipLen(length());
    m_status = Loaded;

    update();
    emit propertyChanged();
}
void AudioClipGraphicsItem::onTempoChange(double tempo) {
    qDebug() << "AudioClipGraphicsItem::onTempoChange" << tempo;
    m_tempo = tempo;
    updateLength();
}
void AudioClipGraphicsItem::drawPreviewArea(QPainter *painter, const QRectF &previewRect,
                                            int opacity) {
    // QElapsedTimer mstimer;
    painter->setRenderHint(QPainter::Antialiasing, false);

    auto rectLeft = previewRect.left();
    auto rectTop = previewRect.top();
    auto rectWidth = previewRect.width();
    auto rectHeight = previewRect.height();
    auto halfRectHeight = rectHeight / 2;
    auto peakColor = QColor(10, 10, 10, opacity);

    QPen pen;

    if (m_status == Loading) {
        pen.setColor(peakColor);
        painter->setPen(pen);
        painter->drawText(previewRect, "Loading...", QTextOption(Qt::AlignCenter));
    }

    pen.setColor(peakColor);
    pen.setWidth(1);
    painter->setPen(pen);

    // mstimer.start();
    if (m_peakCache.count() == 0 || m_peakCacheMipmap.count() == 0)
        return;

    m_resolution = scaleX() >= 0.3 ? High : Low;
    const auto peakData = m_resolution == Low ? m_peakCacheMipmap : m_peakCache;
    const auto chunksPerTick =
        m_resolution == Low ? m_chunksPerTick / m_mipmapScale : m_chunksPerTick;

    auto rectLeftScene = mapToScene(previewRect.topLeft()).x();
    auto rectRightScene = mapToScene(previewRect.bottomRight()).x();
    auto waveRectLeft =
        visibleRect().left() < rectLeftScene ? 0 : visibleRect().left() - rectLeftScene;
    auto waveRectRight = visibleRect().right() < rectRightScene
                             ? visibleRect().right() - rectLeftScene
                             : rectRightScene - rectLeftScene;
    auto waveRectWidth = waveRectRight - waveRectLeft + 1; // 1 px spaceing at right

    auto start = clipStart() * chunksPerTick;
    auto end = (clipStart() + clipLen()) * chunksPerTick;
    auto divideCount = ((end - start) / rectWidth);
    // qDebug() << start << end << rectWidth << (int)divideCount;

    auto drawPeak = [&](int x, short min, short max) {
        auto yMin = -min * halfRectHeight / 32767 + halfRectHeight + rectTop;
        auto yMax = -max * halfRectHeight / 32767 + halfRectHeight + rectTop;
        painter->drawLine(x, static_cast<int>(yMin), x, static_cast<int>(yMax));
    };

    // qDebug() << m_peakCacheThumbnail.count() << divideCount;
    for (int i = static_cast<int>(waveRectLeft); i <= static_cast<int>(waveRectRight); i++) {
        short min = 0;
        short max = 0;
        auto updateMinMax = [](const std::tuple<short, short> &frame, short &min, short &max) {
            auto frameMin = std::get<0>(frame);
            auto frameMax = std::get<1>(frame);
            if (frameMin < min)
                min = frameMin;
            if (frameMax > max)
                max = frameMax;
        };

        for (int j = start + i * divideCount; j < (start + i * divideCount + divideCount); j++) {
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
void AudioClipGraphicsItem::updateLength() {
    m_chunksPerTick = static_cast<double>(m_sampleRate) / m_chunkSize * 60 / m_tempo / 480;
    auto targetLength = m_frames / (m_sampleRate * 60 / m_tempo / 480);
    qDebug() << targetLength;
    if (length() != targetLength)
        setLength(targetLength);
    // TODO: improve length changing experience
    if (clipStart() > targetLength)
        setClipStart(0);
    if (clipStart() + clipLen() > targetLength)
        setClipLen(targetLength - clipStart());
    emit propertyChanged();
}
void AudioClipGraphicsItem::addMenuActions(Menu *menu) {
    auto actionLocateFile = menu->addAction("Locate audio file");
    connect(actionLocateFile, &QAction::triggered, this, [=] {
        auto fileName =
            QFileDialog::getOpenFileName(context(), "Select an Audio File", ".",
                                         "All Audio File (*.wav *.flac *.mp3);;Wave File "
                                         "(*.wav);;Flac File (*.flac);;MP3 File (*.mp3)");
        if (fileName.isNull())
            return;
        setPath(fileName);
        emit propertyChanged();
    });
    menu->addSeparator();
}