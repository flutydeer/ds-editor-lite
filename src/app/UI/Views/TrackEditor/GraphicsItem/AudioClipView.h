//
// Created by fluty on 2023/11/16.
//

#ifndef AUDIOCLIPGRAPHICSITEM_H
#define AUDIOCLIPGRAPHICSITEM_H

#include "AbstractClipView.h"
#include "Model/AppModel/AudioInfoModel.h"
#include "Global/AppGlobal.h"

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

public slots:
    void onTempoChange(double tempo);

private:
    enum RenderResolution { High, Low };

    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, int opacity) override;
    QString clipTypeName() override;

    AppGlobal::AudioLoadStatus m_status = AppGlobal::Init;
    AudioInfoModel m_audioInfo;
    QString m_errorMessage;
    double m_renderStart = 0;
    double m_renderEnd = 0;
    QPoint m_mouseLastPos;
    int m_rectLastWidth = -1;
    double m_tempo = 60;
    RenderResolution m_resolution = High;
    QString m_path;
};



#endif // AUDIOCLIPGRAPHICSITEM_H
