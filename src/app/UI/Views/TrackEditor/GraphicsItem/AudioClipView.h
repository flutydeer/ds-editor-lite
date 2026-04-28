//
// Created by fluty on 2023/11/16.
//

#ifndef AUDIOCLIPGRAPHICSITEM_H
#define AUDIOCLIPGRAPHICSITEM_H

#include "AbstractClipView.h"
#include "Model/AppModel/AudioInfoModel.h"
#include "Global/AppGlobal.h"

// Graphics item that displays an audio clip's waveform in the track editor.
//
// The waveform is rendered directly on the QPainter (no off-screen pixmap) for
// performance. Peak data is pre-computed at two mipmap levels (High / Low) and
// stored in AudioInfoModel. The drawing uses physical-pixel stepping and a
// scene-aligned sampling grid to ensure correct HiDPI rendering and jitter-free
// waveform display during clip trimming. See drawPreviewArea() for details.
class AudioClipView final : public AbstractClipView {
public:
    [[nodiscard]] ClipType clipType() const override {
        return Audio;
    }

    explicit AudioClipView(int itemId, QGraphicsItem *parent = nullptr);
    ~AudioClipView() override = default;

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
    // Peak data resolution: High uses peakCache, Low uses peakCacheMipmap
    enum RenderResolution { High, Low };

    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, QColor color) override;
    [[nodiscard]] QString clipTypeName() const override;
    [[nodiscard]] QString iconPath() const override;

    AppGlobal::AudioLoadStatus m_status = AppGlobal::Init;
    AudioInfoModel m_audioInfo;
    QString m_errorMessage;
    double m_tempo = 60;
    RenderResolution m_resolution = High;
    QString m_path;
};



#endif // AUDIOCLIPGRAPHICSITEM_H
