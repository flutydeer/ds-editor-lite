#ifndef EDITORRHIWIDGET_H
#define EDITORRHIWIDGET_H

#include "EditorRhiGeometry.h"

#include <QColor>
#include <QImage>
#include <QRect>
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

    static void setWindowInputSuppressed(QWidget *window, bool suppressed);
    static void setWindowModalVisualState(QWidget *window, bool active, const QRect &panelRect,
                                          qreal panelCornerRadius, const QColor &backdropColor);

signals:
    void backendFailed(const QString &reason);

protected:
    // Reclaiming the submitted snapshot lets producers rebuild it in place before update().
    [[nodiscard]] EditorRhiFrameData acquireFrame();
    void submitFrame(EditorRhiFrameData frame);
    [[nodiscard]] QVector<EditorRhiSolidVertex>
        submitOverlay(QVector<EditorRhiSolidVertex> vertices);
    [[nodiscard]] QPointF physicalWindowOffset() const;
    void requestBackendFailure(const QString &reason);
#ifdef Q_OS_WIN
    void setPlaybackIndicatorPosition(qreal position);
    void setPlaybackIndicatorColor(const QColor &color);
    void setPlaybackIndicatorVisible(bool visible);
#endif

    void initialize(QRhiCommandBuffer *cb) final;
    void render(QRhiCommandBuffer *cb) final;
    void releaseResources() final;
    bool event(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

    virtual void onRhiReady();
    virtual void onFrameSubmitted();
    virtual void onDevicePixelRatioChanged();

private:
    void setInputSuppressed(bool suppressed);
    void setModalVisualState(bool active, const QRect &panelRect, qreal panelCornerRadius,
                             const QColor &backdropColor);

    class Private;
    std::unique_ptr<Private> d;
};

#endif // EDITORRHIWIDGET_H
