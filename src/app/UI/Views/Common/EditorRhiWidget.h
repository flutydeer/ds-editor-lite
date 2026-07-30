#ifndef EDITORRHIWIDGET_H
#define EDITORRHIWIDGET_H

#include "EditorRhiGeometry.h"

#include <QColor>
#include <QImage>
#include <QRhiWidget>
#include <QVector>

#include <memory>

struct EditorRhiTextVertex {
    float x = 0.0f;
    float y = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;
};

struct EditorRhiTextureBatch {
    int pageId = 0;
    quint64 generation = 0;
    QImage image;
    QVector<EditorRhiTextVertex> vertices;
};

struct EditorRhiFrameData {
    QColor clearColor = Qt::black;
    QPointF physicalCameraOffset;
    QVector<EditorRhiSolidVertex> solidVertices;
    QVector<EditorRhiTextureBatch> textureBatches;
};

class EditorRhiWidget : public QRhiWidget {
    Q_OBJECT

public:
    explicit EditorRhiWidget(QString diagnosticsTag, QWidget *parent = nullptr);
    ~EditorRhiWidget() override;

signals:
    void backendFailed(const QString &reason);

protected:
    void submitFrame(EditorRhiFrameData frame);
    [[nodiscard]] const EditorRhiFrameData &frameData() const;
    void requestBackendFailure(const QString &reason);

    void initialize(QRhiCommandBuffer *cb) final;
    void render(QRhiCommandBuffer *cb) final;
    void releaseResources() final;
    bool event(QEvent *event) override;

    virtual void onRhiReady();
    virtual void onFrameSubmitted();
    virtual void onDevicePixelRatioChanged();

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // EDITORRHIWIDGET_H
