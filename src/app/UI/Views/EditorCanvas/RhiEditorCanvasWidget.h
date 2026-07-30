#ifndef RHIEDITORCANVASWIDGET_H
#define RHIEDITORCANVASWIDGET_H

#include "EditorCanvasTypes.h"
#include "EditorHitIndex.h"
#include "GpuTextAtlas.h"

#include <QRhiWidget>
#include <rhi/qrhi.h>

#include <QMatrix4x4>
#include <QElapsedTimer>

#include <memory>

class QResizeEvent;
class QScrollBar;
class QWheelEvent;
class QMouseEvent;
class QContextMenuEvent;
class QKeyEvent;
class QFocusEvent;
class QEvent;

class RhiEditorCanvasWidget : public QRhiWidget {
    Q_OBJECT

public:
    // Backend-specific canvas adapters forward their common signals through this widget.
    explicit RhiEditorCanvasWidget(EditorCanvasKind kind, QWidget *parent = nullptr);
    ~RhiEditorCanvasWidget() override;

    void setSnapshot(ImmutableEditorRenderSnapshot snapshot, EditorDirtyDomains domains);
    [[nodiscard]] ImmutableEditorRenderSnapshot snapshot() const;

    [[nodiscard]] EditorViewportState viewportState() const;
    void restoreViewportState(const EditorViewportState &state);
    bool setViewportScale(double horizontalScale, double verticalScale);
    void setViewportCenter(double tick, double secondaryValue);
    void setSceneLength(int tick);
    void setPlaybackPosition(double tick);
    void setLastPlaybackPosition(double tick);

    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double topSecondaryValue() const;
    [[nodiscard]] double bottomSecondaryValue() const;
    [[nodiscard]] double centerSecondaryValue() const;
    [[nodiscard]] double scaleX() const;
    [[nodiscard]] double scaleY() const;

    [[nodiscard]] QScrollBar *horizontalScrollBar() const;
    [[nodiscard]] QScrollBar *verticalScrollBar() const;
    [[nodiscard]] QPointF logicalPosition(const QPointF &viewportPosition) const;
    [[nodiscard]] EditorHitResult hitTest(const QPointF &logicalPosition) const;
    [[nodiscard]] const EditorRenderMetrics &metrics() const;
    void setSnapshotBuildDuration(qint64 nanoseconds);

public slots:
    void onWheelHorScale(QWheelEvent *event);
    void onWheelVerScale(QWheelEvent *event);
    void onWheelHorScroll(QWheelEvent *event);
    void onWheelVerScroll(QWheelEvent *event);

signals:
    void scaleChanged(double horizontalScale, double verticalScale);
    void timeRangeChanged(double startTick, double endTick);
    void secondaryRangeChanged(double top, double bottom);
    void visibleRectChanged(const QRectF &rect);
    void canvasSizeChanged(QSize size);
    void backendFailed(const QString &reason);
    void pointerPressed(const QPointF &logicalPosition, const EditorHitResult &hit,
                        Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void pointerMoved(const QPointF &logicalPosition, const EditorHitResult &hit,
                      Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    void pointerLeft();
    void pointerReleased(const QPointF &logicalPosition, const EditorHitResult &hit,
                         Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void pointerDoubleClicked(const QPointF &logicalPosition, const EditorHitResult &hit,
                              Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void contextMenuRequested(const QPointF &logicalPosition, const EditorHitResult &hit,
                              const QPoint &globalPosition);
    void keyPressed(int key, Qt::KeyboardModifiers modifiers);
    void interactionCanceled();
    void frameMetricsUpdated(const EditorRenderMetrics &metrics);

protected:
    bool event(QEvent *event) override;
    void initialize(QRhiCommandBuffer *commandBuffer) override;
    void render(QRhiCommandBuffer *commandBuffer) override;
    void releaseResources() override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    struct Vertex {
        float x;
        float y;
        float red;
        float green;
        float blue;
        float alpha;
    };

    struct TextVertex {
        float x;
        float y;
        float red;
        float green;
        float blue;
        float alpha;
        float u;
        float v;
    };

    static QShader loadShader(const QString &path);
    static void appendRectangle(QVector<Vertex> &vertices, const QRectF &rect, const QColor &color);
    void appendLine(QVector<Vertex> &vertices, const EditorRenderLine &line) const;
    void appendPath(QVector<Vertex> &vertices, const EditorRenderPath &path) const;
    static Vertex vertex(const QPointF &point, const QColor &color);
    static TextVertex textVertex(const QPointF &point, const QColor &color,
                                 const QPointF &textureCoordinate);
    static void appendTextQuad(QVector<TextVertex> &vertices, const GpuTextQuad &quad);

    void createPipeline(QRhiCommandBuffer *commandBuffer);
    void rebuildTextQuads();
    void rebuildVertices();
    void rebuildOverlayVertices();
    void updateScrollRanges();
    void updateViewportSignals();
    void scheduleUpdate();
    void reportFailure(const QString &reason);
    void resetGpuResources();
    [[nodiscard]] QSize contentPixelSize() const;
    [[nodiscard]] QPointF cameraPosition() const;
    [[nodiscard]] double pixelsPerQuarterNote() const;
    [[nodiscard]] double secondaryUnitHeight() const;
    [[nodiscard]] double secondaryValueForY(double y) const;
    [[nodiscard]] double yForSecondaryValue(double value) const;

    EditorCanvasKind m_kind;
    ImmutableEditorRenderSnapshot m_snapshot;
    QVector<Vertex> m_vertices;
    QVector<Vertex> m_overlayVertices;
    QVector<TextVertex> m_textVertices;
    QVector<GpuTextQuad> m_textQuads;
    GpuTextAtlas m_textAtlas;
    EditorHitIndex m_hitIndex;
    QScrollBar *m_horizontalScrollBar = nullptr;
    QScrollBar *m_verticalScrollBar = nullptr;

    QRhi *m_rhi = nullptr;
    std::unique_ptr<QRhiBuffer> m_vertexBuffer;
    std::unique_ptr<QRhiBuffer> m_overlayVertexBuffer;
    std::unique_ptr<QRhiBuffer> m_textVertexBuffer;
    std::unique_ptr<QRhiBuffer> m_uniformBuffer;
    std::unique_ptr<QRhiShaderResourceBindings> m_shaderResourceBindings;
    std::unique_ptr<QRhiShaderResourceBindings> m_textShaderResourceBindings;
    std::unique_ptr<QRhiGraphicsPipeline> m_pipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_textPipeline;
    std::unique_ptr<QRhiTexture> m_textTexture;
    std::unique_ptr<QRhiSampler> m_textSampler;
    qsizetype m_vertexBufferCapacity = 0;
    qsizetype m_overlayVertexBufferCapacity = 0;
    qsizetype m_textVertexBufferCapacity = 0;
    quint64 m_resourceGeneration = 0;
    quint64 m_uploadedAtlasRevision = 0;

    double m_scaleX = 1.0;
    double m_scaleY = 1.0;
    int m_sceneLength = 0;
    double m_playbackPosition = 0.0;
    double m_lastPlaybackPosition = 0.0;
    bool m_vertexDirty = true;
    bool m_overlayDirty = true;
    bool m_cameraDirty = true;
    bool m_failureReported = false;
    QElapsedTimer m_clock;
    qint64 m_updateRequestedAt = 0;
    qint64 m_lastFrameSubmittedAt = 0;
    EditorRenderMetrics m_metrics;
};

#endif // RHIEDITORCANVASWIDGET_H
