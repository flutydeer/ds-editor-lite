//
// Created by fluty on 2023/11/16.
//

#ifndef AUDIOCLIPGRAPHICSITEM_H
#define AUDIOCLIPGRAPHICSITEM_H

#include "AbstractClipGraphicsItem.h"
#include "Model/AudioInfoModel.h"
#include "Global/AppGlobal.h"

class AudioClipBackgroundWorker;

class AudioClipGraphicsItem final : public AbstractClipGraphicsItem {
public:
    explicit AudioClipGraphicsItem(int itemId, QGraphicsItem *parent = nullptr);
    ~AudioClipGraphicsItem() override = default;

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
    QString clipTypeName() override {
        return "[Audio] ";
    }
    void updateLength();
    void addMenuActions(Menu *menu) override;

    AppGlobal::AudioLoadStatus m_status;
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
