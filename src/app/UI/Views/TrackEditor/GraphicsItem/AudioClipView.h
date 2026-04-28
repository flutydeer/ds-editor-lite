//
// Created by fluty on 2023/11/16.
//

#ifndef AUDIOCLIPGRAPHICSITEM_H
#define AUDIOCLIPGRAPHICSITEM_H

#include "AbstractClipView.h"
#include "Model/AppModel/AudioInfoModel.h"
#include "Global/AppGlobal.h"

#include <QVector>

namespace talcs {
class AbstractAudioFormatIO;
}

// Graphics item that displays an audio clip's waveform in the track editor.
//
// Rendering modes (selected automatically by zoom level):
//   1. Peak mode — uses pre-computed peakCache / peakCacheMipmap (vertical lines)
//   2. Sub-chunk peak mode — reads raw samples via IO, computes per-pixel min/max
//   3. Waveform curve mode — reads raw samples, applies Lanczos sinc interpolation,
//      draws a smooth continuous curve via QPainterPath
//
// The drawing uses physical-pixel stepping and a scene-aligned sampling grid to
// ensure correct HiDPI rendering and jitter-free display during clip trimming.
class AudioClipView final : public AbstractClipView {
public:
    [[nodiscard]] ClipType clipType() const override {
        return Audio;
    }

    explicit AudioClipView(int itemId, QGraphicsItem *parent = nullptr);
    ~AudioClipView() override;

    [[nodiscard]] QString path() const;
    void setPath(const QString &path);
    [[nodiscard]] double tempo() const;
    void setTempo(double tempo);
    void setAudioInfo(const AudioInfoModel &info);
    void setStatus(AppGlobal::AudioLoadStatus status);
    void setErrorMessage(const QString &errorMessage);
    [[nodiscard]] int contentLength() const override;

public slots:
    void onTempoChange(double tempo);

private:
    enum RenderResolution { High, Low };

    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, QColor color) override;
    [[nodiscard]] QString clipTypeName() const override;
    [[nodiscard]] QString iconPath() const override;

    bool ensureIO();
    void resetIO();

    static double sincInterpolate(const QVector<float> &samples, qint64 offset,
                                  qint64 totalFrames, double position, int halfKernel = 16);

    void drawPeakMode(QPainter *painter, const QRectF &previewRect, const QColor &color);
    void drawSubChunkPeakMode(QPainter *painter, const QRectF &previewRect, const QColor &color);
    void drawWaveformCurve(QPainter *painter, const QRectF &previewRect, const QColor &color);

    AppGlobal::AudioLoadStatus m_status = AppGlobal::Init;
    AudioInfoModel m_audioInfo;
    QString m_errorMessage;
    double m_tempo = 60;
    RenderResolution m_resolution = High;
    QString m_path;

    talcs::AbstractAudioFormatIO *m_io = nullptr;
    QVector<float> m_ioBuffer;
};



#endif // AUDIOCLIPGRAPHICSITEM_H
