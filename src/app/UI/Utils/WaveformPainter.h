//
// Created by assistant on 2025/04/29.
//

#ifndef WAVEFORMPAINTER_H
#define WAVEFORMPAINTER_H

#include "Model/AppModel/AudioInfoModel.h"

#include <QVector>
#include <QString>

class QPainter;
class QRectF;
class QColor;

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

    void paint(QPainter *painter, const QRectF &rect, const QColor &color,
               double rectStartTick, double rectEndTick);

private:
    bool ensureIO();
    void resetIO();

    void drawPeakMode(QPainter *painter, const QRectF &rect, const QColor &color,
                      double rectStartTick, double ticksPerPixel, double samplesPerTick);
    void drawSubChunkPeakMode(QPainter *painter, const QRectF &rect, const QColor &color,
                              double rectStartTick, double ticksPerPixel, double samplesPerTick);

    static double logAmplify(double value);

    AudioInfoModel m_audioInfo;
    double m_tempo = 120;
    QString m_path;
    talcs::AbstractAudioFormatIO *m_io = nullptr;
    QVector<float> m_ioBuffer;
};

#endif // WAVEFORMPAINTER_H
