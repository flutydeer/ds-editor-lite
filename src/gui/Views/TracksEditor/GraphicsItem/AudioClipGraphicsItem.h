//
// Created by fluty on 2023/11/16.
//

#ifndef AUDIOCLIPGRAPHICSITEM_H
#define AUDIOCLIPGRAPHICSITEM_H

#include "AbstractClipGraphicsItem.h"
#include "AudioClipBackgroundWorker.h"

class AudioClipGraphicsItem final : public AbstractClipGraphicsItem {
public:
    enum Status { Init, Loading, Loaded, Error };
    explicit AudioClipGraphicsItem(int itemId, QGraphicsItem *parent = nullptr);
    ~AudioClipGraphicsItem() override = default;

    QString path() const;
    void setPath(const QString &path);
    double tempo() const;
    void setTempo(double tempo);

public slots:
    void onLoadComplete(bool success, QString errorMessage);
    void onTempoChange(double tempo);

private:
    enum RenderResolution { High, Low };

    void drawPreviewArea(QPainter *painter, const QRectF &previewRect, int opacity) override;
    QString clipTypeName() override {
        return "[Audio] ";
    }
    void updateLength();
    void addMenuActions(QMenu *menu) override;

    Status m_status = Init;
    AudioClipBackgroundWorker *m_worker = nullptr;
    QVector<std::tuple<short, short>> m_peakCache;
    QVector<std::tuple<short, short>> m_peakCacheMipmap;
    double m_renderStart = 0;
    double m_renderEnd = 0;
    QPoint m_mouseLastPos;
    int m_rectLastWidth = -1;
    double m_chunksPerTick;
    int m_chunkSize;
    int m_mipmapScale;
    int m_sampleRate;
    int m_channels;
    long long m_frames;
    double m_tempo = 60;
    RenderResolution m_resolution = High;
    QString m_path;
};



#endif // AUDIOCLIPGRAPHICSITEM_H
