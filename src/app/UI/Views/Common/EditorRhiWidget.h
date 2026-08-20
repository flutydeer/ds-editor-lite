#ifndef EDITORRHIWIDGET_H
#define EDITORRHIWIDGET_H

#include "EditorRhiGeometry.h"

#include <QColor>
#include <QImage>
#include <QRhiWidget>
#include <QVector>

#include <memory>

class QPaintEvent;
class QResizeEvent;

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

struct EditorRhiTextureDrawSpan {
    int pageId = -1;
    qsizetype vertexOffset = 0;
    qsizetype vertexCount = 0;
    QColor color;

    [[nodiscard]] bool isValid() const {
        return pageId >= 0 && vertexOffset >= 0 && vertexCount > 0;
    }
};

struct EditorRhiDrawCommand {
    enum class Type { Solid, Texture };

    Type type = Type::Solid;
    int pageId = -1;
    qsizetype vertexOffset = 0;
    qsizetype vertexCount = 0;
    QColor color;
};

struct EditorRhiDrawList {
    void clear() {
        commands.clear();
        committedSolidVertexCount = 0;
    }

    void appendTexture(const EditorRhiTextureDrawSpan &span, const qsizetype solidVertexCount) {
        if (!span.isValid())
            return;
        appendPendingSolid(solidVertexCount);
        if (!commands.isEmpty()) {
            auto &last = commands.last();
            if (last.type == EditorRhiDrawCommand::Type::Texture && last.pageId == span.pageId &&
                last.color == span.color &&
                last.vertexOffset + last.vertexCount == span.vertexOffset) {
                last.vertexCount += span.vertexCount;
                return;
            }
        }
        commands.append({EditorRhiDrawCommand::Type::Texture, span.pageId, span.vertexOffset,
                         span.vertexCount, span.color});
    }

    void finish(const qsizetype solidVertexCount) {
        appendPendingSolid(solidVertexCount);
    }

    QVector<EditorRhiDrawCommand> commands;

private:
    void appendPendingSolid(const qsizetype solidVertexCount) {
        const auto count = solidVertexCount - committedSolidVertexCount;
        if (count <= 0)
            return;
        commands.append({EditorRhiDrawCommand::Type::Solid, -1, committedSolidVertexCount, count});
        committedSolidVertexCount = solidVertexCount;
    }

    qsizetype committedSolidVertexCount = 0;
};

struct EditorRhiFrameData {
    QColor clearColor = Qt::black;
    QPointF physicalCameraOffset;
    QVector<EditorRhiSolidVertex> solidVertices;
    QVector<EditorRhiTextureBatch> textureBatches;
    EditorRhiDrawList drawList;
};

class EditorRhiWidget : public QRhiWidget {
    Q_OBJECT

public:
    explicit EditorRhiWidget(QString diagnosticsTag, QWidget *parent = nullptr);
    ~EditorRhiWidget() override;

signals:
    void backendFailed(const QString &reason);

protected:
    // Reclaiming the submitted snapshot lets producers rebuild it in place before update().
    [[nodiscard]] EditorRhiFrameData acquireFrame();
    void submitFrame(EditorRhiFrameData frame);
    [[nodiscard]] QVector<EditorRhiOverlayRect> submitOverlay(QVector<EditorRhiOverlayRect> rects);
    [[nodiscard]] QPointF physicalWindowOffset() const;
    [[nodiscard]] bool isResizeActive() const;
    void requestBackendFailure(const QString &reason);

    void initialize(QRhiCommandBuffer *cb) final;
    void render(QRhiCommandBuffer *cb) final;
    void releaseResources() final;
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    virtual void onRhiReady();
    virtual void onDevicePixelRatioChanged();
    virtual void onResizeSettled();

private:
    class Private;
    std::unique_ptr<Private> d;
};

#endif // EDITORRHIWIDGET_H
