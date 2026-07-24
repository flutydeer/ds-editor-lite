//
// Created by assistant on 2025/04/29.
//

#ifndef WAVEFORMPAINTER_H
#define WAVEFORMPAINTER_H

#include "Model/AppModel/AudioInfoModel.h"
#include "UI/Utils/WaveformRenderUtils.h"

#include <QVector>
#include <QString>
#include <QPixmap>
#include <QSize>

class QPainter;
class QRectF;

namespace talcs {
class AbstractAudioFormatIO;
}

class WaveformPainter {
public:
    WaveformPainter();
    ~WaveformPainter();

    void setAudioPath(const QString &path);
    void setAudioInfo(const AudioInfoModel &info);
    void setTempo(double tempo);

    void setRenderMode(WaveformRenderUtils::Mode mode);
    [[nodiscard]] WaveformRenderUtils::Mode renderMode() const;

    void paint(QPainter *painter, const QRectF &rect, const QColor &color,
               double rectStartTick, double rectEndTick);

private:
    bool ensureIO();
    void resetIO();

    void doPaint(QPainter *painter, const QRectF &rect, const QColor &color,
                 double rectStartTick, double rectEndTick);

    void drawPeakMode(QPainter *painter, const QRectF &rect, const QColor &color,
                      double rectStartTick, double ticksPerPixel, double samplesPerTick);
    void drawSubChunkPeakMode(QPainter *painter, const QRectF &rect, const QColor &color,
                              double rectStartTick, double ticksPerPixel, double samplesPerTick);

    static double logAmplify(double value);

    WaveformRenderUtils::Mode m_renderMode = WaveformRenderUtils::FilledMode;

    AudioInfoModel m_audioInfo;
    double m_tempo = 120;
    QString m_path;
    talcs::AbstractAudioFormatIO *m_io = nullptr;
    QVector<float> m_ioBuffer;

    QPixmap m_cache;
    QSize m_cacheSize;
    qreal m_cacheDpr = 0;
};

#endif // WAVEFORMPAINTER_H
