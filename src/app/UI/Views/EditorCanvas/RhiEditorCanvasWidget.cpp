#include "RhiEditorCanvasWidget.h"

#include "Global/AppGlobal.h"
#include "Global/TracksEditorGlobal.h"
#include "ScreenSpaceStrokeTessellator.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"

#include <QFile>
#include <QContextMenuEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

    Q_LOGGING_CATEGORY(editorRhiLog, "ds.editor.rhi")

    constexpr int scrollBarExtent = 14;
    constexpr double pianoKeyHeight = 24.0;

    qsizetype nextBufferCapacity(const qsizetype required) {
        qsizetype capacity = 256;
        while (capacity < required)
            capacity *= 2;
        return capacity;
    }

} // namespace

RhiEditorCanvasWidget::RhiEditorCanvasWidget(const EditorCanvasKind kind, QWidget *parent)
    : QRhiWidget(parent), m_kind(kind) {
    m_clock.start();
    setApi(QRhiWidget::Api::Direct3D11);
    bool sampleCountOk = false;
    const auto requestedSampleCount =
        qEnvironmentVariableIntValue("DS_EDITOR_RHI_MSAA", &sampleCountOk);
    const auto sampleCount = sampleCountOk && requestedSampleCount == 4 ? 4 : 1;
    setSampleCount(sampleCount);
    qCInfo(editorRhiLog) << "Editor QRhi sample count" << sampleCount;
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    m_horizontalScrollBar = new QScrollBar(Qt::Horizontal, this);
    m_verticalScrollBar = new QScrollBar(Qt::Vertical, this);
    connect(m_horizontalScrollBar, &QScrollBar::valueChanged, this, [this] {
        m_cameraDirty = true;
        updateViewportSignals();
        scheduleUpdate();
    });
    connect(m_verticalScrollBar, &QScrollBar::valueChanged, this, [this] {
        m_cameraDirty = true;
        updateViewportSignals();
        scheduleUpdate();
    });
    connect(this, &QRhiWidget::renderFailed, this,
            [this] { reportFailure(tr("QRhiWidget reported a rendering failure.")); });
    connect(this, &QRhiWidget::frameSubmitted, this, [this] {
        const auto now = m_clock.nsecsElapsed();
        m_metrics.updateToFrameSubmittedNanoseconds =
            m_updateRequestedAt > 0 ? now - m_updateRequestedAt : 0;
        m_metrics.frameIntervalNanoseconds =
            m_lastFrameSubmittedAt > 0 ? now - m_lastFrameSubmittedAt : 0;
        m_lastFrameSubmittedAt = now;
        emit frameMetricsUpdated(m_metrics);
        if (m_metrics.frameNumber % 120 == 0) {
            qCInfo(editorRhiLog) << "Editor QRhi metrics"
                                 << "generation" << m_metrics.resourceGeneration << "revision"
                                 << m_metrics.snapshotRevision << "frameIntervalNs"
                                 << m_metrics.frameIntervalNanoseconds << "snapshotBuildNs"
                                 << m_metrics.snapshotBuildNanoseconds << "updateToFrameSubmittedNs"
                                 << m_metrics.updateToFrameSubmittedNanoseconds << "tessellationNs"
                                 << m_metrics.tessellationNanoseconds << "encodingNs"
                                 << m_metrics.commandEncodingNanoseconds << "drawCalls"
                                 << m_metrics.drawCalls << "vertices" << m_metrics.vertices
                                 << "uploadBytes" << m_metrics.uploadBytes << "atlasHits"
                                 << m_metrics.atlasHits << "atlasMisses" << m_metrics.atlasMisses
                                 << "dynamicFrames" << m_metrics.dynamicFrameCount << "cachedFrames"
                                 << m_metrics.cachedFrameCount;
        }
    });
}

RhiEditorCanvasWidget::~RhiEditorCanvasWidget() {
    resetGpuResources();
}

void RhiEditorCanvasWidget::setSnapshot(ImmutableEditorRenderSnapshot snapshot,
                                        const EditorDirtyDomains domains) {
    const auto hadSnapshot = static_cast<bool>(m_snapshot);
    const auto snapshotChanged = snapshot != m_snapshot;
    m_snapshot = std::move(snapshot);
    if (domains.testFlag(EditorDirtyDomain::Geometry) ||
        domains.testFlag(EditorDirtyDomain::Style) || domains.testFlag(EditorDirtyDomain::Text) ||
        domains.testFlag(EditorDirtyDomain::Waveform) ||
        domains.testFlag(EditorDirtyDomain::Selection) ||
        domains.testFlag(EditorDirtyDomain::Overlay)) {
        m_vertexDirty = true;
    }
    if (!hadSnapshot || domains.testFlag(EditorDirtyDomain::Text) ||
        domains.testFlag(EditorDirtyDomain::Geometry) ||
        domains.testFlag(EditorDirtyDomain::Style)) {
        rebuildTextQuads();
    }
    if (snapshotChanged && m_snapshot)
        m_hitIndex.rebuild(*m_snapshot);
    else if (!m_snapshot)
        m_hitIndex.clear();
    if (domains.testFlag(EditorDirtyDomain::Camera))
        m_cameraDirty = true;
    updateScrollRanges();
    scheduleUpdate();
}

ImmutableEditorRenderSnapshot RhiEditorCanvasWidget::snapshot() const {
    return m_snapshot;
}

EditorViewportState RhiEditorCanvasWidget::viewportState() const {
    EditorViewportState state;
    state.centerTick = (startTick() + endTick()) * 0.5;
    const auto secondaryCenter = centerSecondaryValue();
    state.centerTrack = secondaryCenter;
    state.centerKey = secondaryCenter;
    state.horizontalScale = m_scaleX;
    state.verticalScale = m_scaleY;
    state.startTick = startTick();
    state.endTick = endTick();
    state.topValue = topSecondaryValue();
    state.bottomValue = bottomSecondaryValue();
    state.viewportSize = contentPixelSize();
    state.devicePixelRatio = devicePixelRatioF();
    state.playbackPosition = m_playbackPosition;
    state.lastPlaybackPosition = m_lastPlaybackPosition;
    return state;
}

void RhiEditorCanvasWidget::restoreViewportState(const EditorViewportState &state) {
    if (!setViewportScale(state.horizontalScale, state.verticalScale))
        return;
    setViewportCenter(state.centerTick, m_kind == EditorCanvasKind::TrackEditor ? state.centerTrack
                                                                                : state.centerKey);
    setPlaybackPosition(state.playbackPosition);
    setLastPlaybackPosition(state.lastPlaybackPosition);
}

bool RhiEditorCanvasWidget::setViewportScale(const double horizontalScale,
                                             const double verticalScale) {
    if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale) ||
        horizontalScale <= 0.0 || verticalScale <= 0.0)
        return false;
    const auto previousCenterTick = (startTick() + endTick()) * 0.5;
    const auto previousSecondary = centerSecondaryValue();
    m_scaleX = std::clamp(horizontalScale, 0.1, 12.0);
    m_scaleY = std::clamp(verticalScale, 0.25, 8.0);
    updateScrollRanges();
    setViewportCenter(previousCenterTick, previousSecondary);
    m_vertexDirty = true;
    m_overlayDirty = true;
    m_cameraDirty = true;
    emit scaleChanged(m_scaleX, m_scaleY);
    updateViewportSignals();
    scheduleUpdate();
    return true;
}

void RhiEditorCanvasWidget::setViewportCenter(const double tick, const double secondaryValue) {
    if (!std::isfinite(tick) || !std::isfinite(secondaryValue))
        return;
    const auto contentSize = contentPixelSize();
    const auto centerX = tick * pixelsPerQuarterNote() / AppGlobal::ticksPerQuarterNote;
    const auto centerY = yForSecondaryValue(secondaryValue);
    m_horizontalScrollBar->setValue(qRound(centerX * m_scaleX - contentSize.width() * 0.5));
    m_verticalScrollBar->setValue(qRound(centerY * m_scaleY - contentSize.height() * 0.5));
}

void RhiEditorCanvasWidget::setSceneLength(const int tick) {
    m_sceneLength = qMax(0, tick);
    updateScrollRanges();
}

void RhiEditorCanvasWidget::setPlaybackPosition(const double tick) {
    if (qFuzzyCompare(m_playbackPosition, tick))
        return;
    m_playbackPosition = tick;
    m_overlayDirty = true;
    scheduleUpdate();
}

void RhiEditorCanvasWidget::setLastPlaybackPosition(const double tick) {
    if (qFuzzyCompare(m_lastPlaybackPosition, tick))
        return;
    m_lastPlaybackPosition = tick;
    m_overlayDirty = true;
    scheduleUpdate();
}

double RhiEditorCanvasWidget::startTick() const {
    return cameraPosition().x() * AppGlobal::ticksPerQuarterNote / pixelsPerQuarterNote();
}

double RhiEditorCanvasWidget::endTick() const {
    const auto width = contentPixelSize().width() / m_scaleX;
    return (cameraPosition().x() + width) * AppGlobal::ticksPerQuarterNote / pixelsPerQuarterNote();
}

double RhiEditorCanvasWidget::topSecondaryValue() const {
    return secondaryValueForY(cameraPosition().y());
}

double RhiEditorCanvasWidget::bottomSecondaryValue() const {
    return secondaryValueForY(cameraPosition().y() + contentPixelSize().height() / m_scaleY);
}

double RhiEditorCanvasWidget::centerSecondaryValue() const {
    return secondaryValueForY(cameraPosition().y() + contentPixelSize().height() / m_scaleY * 0.5);
}

double RhiEditorCanvasWidget::scaleX() const {
    return m_scaleX;
}

double RhiEditorCanvasWidget::scaleY() const {
    return m_scaleY;
}

QScrollBar *RhiEditorCanvasWidget::horizontalScrollBar() const {
    return m_horizontalScrollBar;
}

QScrollBar *RhiEditorCanvasWidget::verticalScrollBar() const {
    return m_verticalScrollBar;
}

QPointF RhiEditorCanvasWidget::logicalPosition(const QPointF &viewportPosition) const {
    return {
        cameraPosition().x() + viewportPosition.x() / m_scaleX,
        cameraPosition().y() + viewportPosition.y() / m_scaleY,
    };
}

EditorHitResult RhiEditorCanvasWidget::hitTest(const QPointF &logicalPosition) const {
    return m_hitIndex.query(logicalPosition, m_scaleX);
}

const EditorRenderMetrics &RhiEditorCanvasWidget::metrics() const {
    return m_metrics;
}

void RhiEditorCanvasWidget::setSnapshotBuildDuration(const qint64 nanoseconds) {
    m_metrics.snapshotBuildNanoseconds = qMax<qint64>(0, nanoseconds);
}

void RhiEditorCanvasWidget::onWheelHorScale(QWheelEvent *event) {
    const auto factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    setViewportScale(m_scaleX * factor, m_scaleY);
    event->accept();
}

void RhiEditorCanvasWidget::onWheelVerScale(QWheelEvent *event) {
    const auto factor = event->angleDelta().y() > 0 ? 1.15 : 1.0 / 1.15;
    setViewportScale(m_scaleX, m_scaleY * factor);
    event->accept();
}

void RhiEditorCanvasWidget::onWheelHorScroll(QWheelEvent *event) {
    m_horizontalScrollBar->setValue(m_horizontalScrollBar->value() - event->angleDelta().y());
    event->accept();
}

void RhiEditorCanvasWidget::onWheelVerScroll(QWheelEvent *event) {
    m_verticalScrollBar->setValue(m_verticalScrollBar->value() - event->angleDelta().y());
    event->accept();
}

bool RhiEditorCanvasWidget::event(QEvent *event) {
    const auto result = QRhiWidget::event(event);
    if (event->type() == QEvent::DevicePixelRatioChange ||
        event->type() == QEvent::ScreenChangeInternal) {
        rebuildTextQuads();
        m_vertexDirty = true;
        m_overlayDirty = true;
        m_cameraDirty = true;
        scheduleUpdate();
    }
    return result;
}

void RhiEditorCanvasWidget::initialize(QRhiCommandBuffer *commandBuffer) {
    if (qEnvironmentVariable("DS_EDITOR_RHI_FAIL") == QStringLiteral("initialize")) {
        reportFailure(tr("Injected QRhi initialization failure."));
        return;
    }

    if (m_rhi != rhi()) {
        resetGpuResources();
        m_rhi = rhi();
    } else {
        m_pipeline.reset();
        m_textPipeline.reset();
    }

    if (!m_rhi || !renderTarget()) {
        reportFailure(tr("QRhi did not provide a render device or render target."));
        return;
    }

    createPipeline(commandBuffer);
}

void RhiEditorCanvasWidget::render(QRhiCommandBuffer *commandBuffer) {
    if (!m_rhi || !m_pipeline || !m_textPipeline || !m_uniformBuffer ||
        !m_textShaderResourceBindings) {
        reportFailure(tr("QRhi canvas resources are not initialized."));
        return;
    }
    if (qEnvironmentVariable("DS_EDITOR_RHI_FAIL") == QStringLiteral("render")) {
        reportFailure(tr("Injected QRhi render failure."));
        return;
    }

    const auto geometryWasDirty = m_vertexDirty;
    const auto overlayWasDirty = m_overlayDirty;
    QElapsedTimer tessellationTimer;
    tessellationTimer.start();
    if (geometryWasDirty)
        rebuildVertices();
    if (overlayWasDirty)
        rebuildOverlayVertices();
    m_metrics.tessellationNanoseconds =
        geometryWasDirty || overlayWasDirty ? tessellationTimer.nsecsElapsed() : 0;

    auto *updates = m_rhi->nextResourceUpdateBatch();
    auto uploadBytes = quint64{0};
    if (geometryWasDirty && m_vertices.size() > m_vertexBufferCapacity) {
        m_vertexBufferCapacity = nextBufferCapacity(m_vertices.size());
        m_vertexBuffer.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                             m_vertexBufferCapacity * static_cast<qsizetype>(sizeof(Vertex))));
        if (!m_vertexBuffer->create()) {
            reportFailure(tr("Could not allocate the QRhi vertex buffer."));
            delete updates;
            return;
        }
    }
    if (geometryWasDirty && !m_vertices.isEmpty()) {
        updates->updateDynamicBuffer(m_vertexBuffer.get(), 0, m_vertices.size() * sizeof(Vertex),
                                     m_vertices.constData());
        uploadBytes += static_cast<quint64>(m_vertices.size() * sizeof(Vertex));
    }
    if (overlayWasDirty && m_overlayVertices.size() > m_overlayVertexBufferCapacity) {
        m_overlayVertexBufferCapacity = nextBufferCapacity(m_overlayVertices.size());
        m_overlayVertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                     m_overlayVertexBufferCapacity *
                                                         static_cast<qsizetype>(sizeof(Vertex))));
        if (!m_overlayVertexBuffer->create()) {
            reportFailure(tr("Could not allocate the QRhi overlay vertex buffer."));
            delete updates;
            return;
        }
    }
    if (overlayWasDirty && !m_overlayVertices.isEmpty()) {
        updates->updateDynamicBuffer(m_overlayVertexBuffer.get(), 0,
                                     m_overlayVertices.size() * sizeof(Vertex),
                                     m_overlayVertices.constData());
        uploadBytes += static_cast<quint64>(m_overlayVertices.size() * sizeof(Vertex));
    }
    if (geometryWasDirty && m_textVertices.size() > m_textVertexBufferCapacity) {
        m_textVertexBufferCapacity = nextBufferCapacity(m_textVertices.size());
        m_textVertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                  m_textVertexBufferCapacity *
                                                      static_cast<qsizetype>(sizeof(TextVertex))));
        if (!m_textVertexBuffer->create()) {
            reportFailure(tr("Could not allocate the QRhi text vertex buffer."));
            delete updates;
            return;
        }
    }
    if (geometryWasDirty && !m_textVertices.isEmpty()) {
        updates->updateDynamicBuffer(m_textVertexBuffer.get(), 0,
                                     m_textVertices.size() * sizeof(TextVertex),
                                     m_textVertices.constData());
        uploadBytes += static_cast<quint64>(m_textVertices.size() * sizeof(TextVertex));
    }
    if (m_textTexture && m_uploadedAtlasRevision != m_textAtlas.revision()) {
        updates->uploadTexture(m_textTexture.get(), m_textAtlas.image());
        uploadBytes += static_cast<quint64>(m_textAtlas.image().sizeInBytes());
        m_uploadedAtlasRevision = m_textAtlas.revision();
    }

    const auto camera = cameraPosition();
    const auto outputSize = renderTarget()->pixelSize();
    const auto dpr = devicePixelRatioF();
    const auto logicalOutputWidth = outputSize.width() / dpr;
    const auto logicalOutputHeight = outputSize.height() / dpr;
    QMatrix4x4 matrix = m_rhi->clipSpaceCorrMatrix();
    matrix.ortho(static_cast<float>(camera.x()),
                 static_cast<float>(camera.x() + logicalOutputWidth / m_scaleX),
                 static_cast<float>(camera.y() + logicalOutputHeight / m_scaleY),
                 static_cast<float>(camera.y()), -1.0F, 1.0F);
    updates->updateDynamicBuffer(m_uniformBuffer.get(), 0, 64, matrix.constData());

    const auto clearColor =
        m_kind == EditorCanvasKind::TrackEditor ? QColor(20, 22, 27) : QColor(18, 20, 24);
    QElapsedTimer encodingTimer;
    encodingTimer.start();
    commandBuffer->beginPass(renderTarget(), clearColor, {1.0F, 0}, updates);
    commandBuffer->setGraphicsPipeline(m_pipeline.get());
    commandBuffer->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
    commandBuffer->setShaderResources();
    if (!m_vertices.isEmpty()) {
        const QRhiCommandBuffer::VertexInput vertexBinding(m_vertexBuffer.get(), 0);
        commandBuffer->setVertexInput(0, 1, &vertexBinding);
        commandBuffer->draw(static_cast<quint32>(m_vertices.size()));
    }
    if (!m_textVertices.isEmpty()) {
        commandBuffer->setGraphicsPipeline(m_textPipeline.get());
        commandBuffer->setShaderResources(m_textShaderResourceBindings.get());
        const QRhiCommandBuffer::VertexInput textVertexBinding(m_textVertexBuffer.get(), 0);
        commandBuffer->setVertexInput(0, 1, &textVertexBinding);
        commandBuffer->draw(static_cast<quint32>(m_textVertices.size()));
    }
    if (!m_overlayVertices.isEmpty()) {
        commandBuffer->setGraphicsPipeline(m_pipeline.get());
        commandBuffer->setShaderResources();
        const QRhiCommandBuffer::VertexInput overlayVertexBinding(m_overlayVertexBuffer.get(), 0);
        commandBuffer->setVertexInput(0, 1, &overlayVertexBinding);
        commandBuffer->draw(static_cast<quint32>(m_overlayVertices.size()));
    }
    commandBuffer->endPass();

    ++m_metrics.frameNumber;
    if (geometryWasDirty || overlayWasDirty)
        ++m_metrics.dynamicFrameCount;
    else
        ++m_metrics.cachedFrameCount;
    m_metrics.resourceGeneration = m_resourceGeneration;
    m_metrics.snapshotRevision = m_snapshot ? m_snapshot->revision : 0;
    m_metrics.drawCalls = (!m_vertices.isEmpty() ? 1 : 0) + (!m_overlayVertices.isEmpty() ? 1 : 0) +
                          (!m_textVertices.isEmpty() ? 1 : 0);
    m_metrics.vertices =
        static_cast<quint64>(m_vertices.size() + m_overlayVertices.size() + m_textVertices.size());
    m_metrics.uploadBytes = uploadBytes;
    m_metrics.atlasHits = m_textAtlas.hitCount();
    m_metrics.atlasMisses = m_textAtlas.missCount();
    m_metrics.commandEncodingNanoseconds = encodingTimer.nsecsElapsed();
    m_vertexDirty = false;
    m_overlayDirty = false;
    m_cameraDirty = false;
}

void RhiEditorCanvasWidget::releaseResources() {
    resetGpuResources();
}

void RhiEditorCanvasWidget::resizeEvent(QResizeEvent *event) {
    QRhiWidget::resizeEvent(event);
    const auto widgetSize = event->size();
    m_horizontalScrollBar->setGeometry(0, qMax(0, widgetSize.height() - scrollBarExtent),
                                       qMax(0, widgetSize.width() - scrollBarExtent),
                                       scrollBarExtent);
    m_verticalScrollBar->setGeometry(qMax(0, widgetSize.width() - scrollBarExtent), 0,
                                     scrollBarExtent,
                                     qMax(0, widgetSize.height() - scrollBarExtent));
    updateScrollRanges();
    m_vertexDirty = true;
    m_overlayDirty = true;
    m_cameraDirty = true;
    emit canvasSizeChanged(contentPixelSize());
    updateViewportSignals();
}

void RhiEditorCanvasWidget::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() == Qt::ControlModifier) {
        onWheelHorScale(event);
    } else if (event->modifiers() == Qt::AltModifier) {
        onWheelVerScale(event);
    } else if (event->modifiers() == Qt::ShiftModifier) {
        onWheelHorScroll(event);
    } else {
        onWheelVerScroll(event);
    }
}

void RhiEditorCanvasWidget::mousePressEvent(QMouseEvent *event) {
    const auto logical = logicalPosition(event->position());
    emit pointerPressed(logical, hitTest(logical), event->button(), event->modifiers());
    event->accept();
}

void RhiEditorCanvasWidget::mouseMoveEvent(QMouseEvent *event) {
    const auto logical = logicalPosition(event->position());
    emit pointerMoved(logical, hitTest(logical), event->buttons(), event->modifiers());
    event->accept();
}

void RhiEditorCanvasWidget::mouseReleaseEvent(QMouseEvent *event) {
    const auto logical = logicalPosition(event->position());
    emit pointerReleased(logical, hitTest(logical), event->button(), event->modifiers());
    event->accept();
}

void RhiEditorCanvasWidget::mouseDoubleClickEvent(QMouseEvent *event) {
    const auto logical = logicalPosition(event->position());
    emit pointerDoubleClicked(logical, hitTest(logical), event->button(), event->modifiers());
    event->accept();
}

void RhiEditorCanvasWidget::contextMenuEvent(QContextMenuEvent *event) {
    const auto logical = logicalPosition(event->pos());
    emit contextMenuRequested(logical, hitTest(logical), event->globalPos());
    event->accept();
}

void RhiEditorCanvasWidget::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape)
        emit interactionCanceled();
    emit keyPressed(event->key(), event->modifiers());
    event->accept();
}

void RhiEditorCanvasWidget::focusOutEvent(QFocusEvent *event) {
    emit interactionCanceled();
    QRhiWidget::focusOutEvent(event);
}

void RhiEditorCanvasWidget::leaveEvent(QEvent *event) {
    emit pointerLeft();
    QRhiWidget::leaveEvent(event);
}

QShader RhiEditorCanvasWidget::loadShader(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QShader::fromSerialized(file.readAll());
}

void RhiEditorCanvasWidget::appendRectangle(QVector<Vertex> &vertices, const QRectF &rect,
                                            const QColor &color) {
    if (!rect.isValid() || color.alpha() == 0)
        return;
    const auto topLeft = vertex(rect.topLeft(), color);
    const auto topRight = vertex(rect.topRight(), color);
    const auto bottomLeft = vertex(rect.bottomLeft(), color);
    const auto bottomRight = vertex(rect.bottomRight(), color);
    vertices.append({topLeft, bottomLeft, topRight, topRight, bottomLeft, bottomRight});
}

void RhiEditorCanvasWidget::appendLine(QVector<Vertex> &vertices,
                                       const EditorRenderLine &line) const {
    const auto mesh = ScreenSpaceStrokeTessellator::tessellate(
        {line.start, line.end}, line.color, line.width, m_scaleX, m_scaleY, devicePixelRatioF(),
        EditorStrokeJoin::Bevel, EditorStrokeCap::Butt);
    vertices.reserve(vertices.size() + mesh.size());
    for (const auto &meshVertex : mesh)
        vertices.append(vertex(meshVertex.position, meshVertex.color));
}

void RhiEditorCanvasWidget::appendPath(QVector<Vertex> &vertices,
                                       const EditorRenderPath &path) const {
    const auto mesh = ScreenSpaceStrokeTessellator::tessellate(
        path.points, path.color, path.width, m_scaleX, m_scaleY, devicePixelRatioF(), path.join,
        path.cap);
    vertices.reserve(vertices.size() + mesh.size());
    for (const auto &meshVertex : mesh)
        vertices.append(vertex(meshVertex.position, meshVertex.color));
}

RhiEditorCanvasWidget::Vertex RhiEditorCanvasWidget::vertex(const QPointF &point,
                                                            const QColor &color) {
    const auto alpha = color.alphaF();
    return {
        static_cast<float>(point.x()),
        static_cast<float>(point.y()),
        static_cast<float>(color.redF() * alpha),
        static_cast<float>(color.greenF() * alpha),
        static_cast<float>(color.blueF() * alpha),
        static_cast<float>(alpha),
    };
}

RhiEditorCanvasWidget::TextVertex
    RhiEditorCanvasWidget::textVertex(const QPointF &point, const QColor &color,
                                      const QPointF &textureCoordinate) {
    const auto alpha = color.alphaF();
    return {
        static_cast<float>(point.x()),
        static_cast<float>(point.y()),
        static_cast<float>(color.redF() * alpha),
        static_cast<float>(color.greenF() * alpha),
        static_cast<float>(color.blueF() * alpha),
        static_cast<float>(alpha),
        static_cast<float>(textureCoordinate.x()),
        static_cast<float>(textureCoordinate.y()),
    };
}

void RhiEditorCanvasWidget::appendTextQuad(QVector<TextVertex> &vertices, const GpuTextQuad &quad) {
    const auto &bounds = quad.bounds;
    const auto &uv = quad.textureCoordinates;
    const auto topLeft = textVertex(bounds.topLeft(), quad.color, uv.topLeft());
    const auto topRight = textVertex(bounds.topRight(), quad.color, uv.topRight());
    const auto bottomLeft = textVertex(bounds.bottomLeft(), quad.color, uv.bottomLeft());
    const auto bottomRight = textVertex(bounds.bottomRight(), quad.color, uv.bottomRight());
    vertices.append({topLeft, bottomLeft, topRight, topRight, bottomLeft, bottomRight});
}

void RhiEditorCanvasWidget::createPipeline(QRhiCommandBuffer *commandBuffer) {
    const auto vertexShader = loadShader(QStringLiteral(":/shaders/editor/editor_canvas.vert.qsb"));
    const auto fragmentShader =
        loadShader(QStringLiteral(":/shaders/editor/editor_canvas.frag.qsb"));
    if (!vertexShader.isValid() || !fragmentShader.isValid()) {
        reportFailure(tr("The editor QRhi shaders could not be loaded."));
        return;
    }

    if (!m_uniformBuffer) {
        m_uniformBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        if (!m_uniformBuffer->create()) {
            reportFailure(tr("Could not allocate the QRhi camera buffer."));
            return;
        }
    }
    if (!m_vertexBuffer) {
        m_vertexBufferCapacity = 256;
        m_vertexBuffer.reset(
            m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                             m_vertexBufferCapacity * static_cast<qsizetype>(sizeof(Vertex))));
        if (!m_vertexBuffer->create()) {
            reportFailure(tr("Could not allocate the initial QRhi vertex buffer."));
            return;
        }
    }
    if (!m_textVertexBuffer) {
        m_textVertexBufferCapacity = 256;
        m_textVertexBuffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                                  m_textVertexBufferCapacity *
                                                      static_cast<qsizetype>(sizeof(TextVertex))));
        if (!m_textVertexBuffer->create()) {
            reportFailure(tr("Could not allocate the initial QRhi text vertex buffer."));
            return;
        }
    }
    if (!m_textTexture) {
        m_textTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, m_textAtlas.image().size()));
        if (!m_textTexture->create()) {
            reportFailure(tr("Could not allocate the QRhi glyph atlas texture."));
            return;
        }
        m_uploadedAtlasRevision = 0;
    }
    if (!m_textSampler) {
        m_textSampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                              QRhiSampler::None, QRhiSampler::ClampToEdge,
                                              QRhiSampler::ClampToEdge));
        if (!m_textSampler->create()) {
            reportFailure(tr("Could not create the QRhi glyph atlas sampler."));
            return;
        }
    }
    if (!m_shaderResourceBindings) {
        m_shaderResourceBindings.reset(m_rhi->newShaderResourceBindings());
        m_shaderResourceBindings->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                     m_uniformBuffer.get()),
        });
        if (!m_shaderResourceBindings->create()) {
            reportFailure(tr("Could not create the QRhi shader bindings."));
            return;
        }
    }
    if (!m_textShaderResourceBindings) {
        m_textShaderResourceBindings.reset(m_rhi->newShaderResourceBindings());
        m_textShaderResourceBindings->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                     m_uniformBuffer.get()),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                      m_textTexture.get(), m_textSampler.get()),
        });
        if (!m_textShaderResourceBindings->create()) {
            reportFailure(tr("Could not create the QRhi text shader bindings."));
            return;
        }
    }

    m_pipeline.reset(m_rhi->newGraphicsPipeline());
    m_pipeline->setShaderStages({
        {QRhiShaderStage::Vertex,   vertexShader  },
        {QRhiShaderStage::Fragment, fragmentShader},
    });
    QRhiVertexInputLayout layout;
    layout.setBindings({{sizeof(Vertex)}});
    layout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, offsetof(Vertex, x)  },
        {0, 1, QRhiVertexInputAttribute::Float4, offsetof(Vertex, red)},
    });
    m_pipeline->setVertexInputLayout(layout);
    m_pipeline->setShaderResourceBindings(m_shaderResourceBindings.get());
    m_pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable = true;
    blend.srcColor = QRhiGraphicsPipeline::One;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    m_pipeline->setTargetBlends({blend});
    if (!m_pipeline->create()) {
        reportFailure(tr("Could not create the QRhi graphics pipeline."));
        return;
    }

    const auto textVertexShader =
        loadShader(QStringLiteral(":/shaders/editor/editor_text.vert.qsb"));
    const auto textFragmentShader =
        loadShader(QStringLiteral(":/shaders/editor/editor_text.frag.qsb"));
    if (!textVertexShader.isValid() || !textFragmentShader.isValid()) {
        reportFailure(tr("The editor QRhi text shaders could not be loaded."));
        return;
    }
    m_textPipeline.reset(m_rhi->newGraphicsPipeline());
    m_textPipeline->setShaderStages({
        {QRhiShaderStage::Vertex,   textVertexShader  },
        {QRhiShaderStage::Fragment, textFragmentShader},
    });
    QRhiVertexInputLayout textLayout;
    textLayout.setBindings({{sizeof(TextVertex)}});
    textLayout.setAttributes({
        {0, 0, QRhiVertexInputAttribute::Float2, offsetof(TextVertex, x)  },
        {0, 1, QRhiVertexInputAttribute::Float4, offsetof(TextVertex, red)},
        {0, 2, QRhiVertexInputAttribute::Float2, offsetof(TextVertex, u)  },
    });
    m_textPipeline->setVertexInputLayout(textLayout);
    m_textPipeline->setShaderResourceBindings(m_textShaderResourceBindings.get());
    m_textPipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    m_textPipeline->setTargetBlends({blend});
    if (!m_textPipeline->create()) {
        reportFailure(tr("Could not create the QRhi text pipeline."));
        return;
    }

    ++m_resourceGeneration;
    m_vertexDirty = true;
    m_cameraDirty = true;
    const auto driver = m_rhi->driverInfo();
    qCInfo(editorRhiLog) << "Initialized editor QRhi canvas"
                         << "generation" << m_resourceGeneration << "Qt" << qVersion() << "backend"
                         << m_rhi->backendName() << "adapter" << driver.deviceName << "vendor"
                         << driver.vendorId << "device" << driver.deviceId;
    Q_UNUSED(commandBuffer);
}

void RhiEditorCanvasWidget::rebuildTextQuads() {
    m_textQuads.clear();
    if (!m_snapshot)
        return;
    auto texts = m_snapshot->texts;
    std::ranges::stable_sort(texts, {}, &EditorRenderText::layer);
    m_textQuads = m_textAtlas.layout(texts, devicePixelRatioF());
}

void RhiEditorCanvasWidget::rebuildVertices() {
    m_vertices.clear();
    m_textVertices.clear();
    if (m_snapshot) {
        QVector<int> layers;
        const auto appendLayer = [&layers](const int layer) {
            if (!layers.contains(layer))
                layers.append(layer);
        };
        for (const auto &rect : m_snapshot->rectangles)
            appendLayer(rect.layer);
        for (const auto &line : m_snapshot->lines)
            appendLayer(line.layer);
        for (const auto &path : m_snapshot->paths)
            appendLayer(path.layer);
        std::ranges::sort(layers);

        for (const auto layer : layers) {
            for (const auto &rect : m_snapshot->rectangles) {
                if (rect.layer != layer)
                    continue;
                appendRectangle(m_vertices, rect.bounds, rect.fill);
                if (rect.border.alpha() > 0) {
                    const auto bounds = rect.bounds;
                    appendLine(m_vertices, {
                                               {bounds.left(),  bounds.top()},
                                               {bounds.right(), bounds.top()},
                                               rect.border,
                                               1.0F
                    });
                    appendLine(m_vertices, {
                                               {bounds.right(), bounds.top()   },
                                               {bounds.right(), bounds.bottom()},
                                               rect.border,
                                               1.0F
                    });
                    appendLine(m_vertices, {
                                               {bounds.right(), bounds.bottom()},
                                               {bounds.left(),  bounds.bottom()},
                                               rect.border,
                                               1.0F
                    });
                    appendLine(m_vertices, {
                                               {bounds.left(), bounds.bottom()},
                                               {bounds.left(), bounds.top()   },
                                               rect.border,
                                               1.0F
                    });
                }
            }
            for (const auto &line : m_snapshot->lines)
                if (line.layer == layer)
                    appendLine(m_vertices, line);
            for (const auto &path : m_snapshot->paths)
                if (path.layer == layer)
                    appendPath(m_vertices, path);
        }
        for (const auto &quad : std::as_const(m_textQuads))
            appendTextQuad(m_textVertices, quad);
    }
}

void RhiEditorCanvasWidget::rebuildOverlayVertices() {
    m_overlayVertices.clear();
    const auto playheadX =
        m_playbackPosition * pixelsPerQuarterNote() / AppGlobal::ticksPerQuarterNote;
    const auto lastPlayheadX =
        m_lastPlaybackPosition * pixelsPerQuarterNote() / AppGlobal::ticksPerQuarterNote;
    const auto height = m_snapshot ? qMax(m_snapshot->logicalExtent.height(),
                                          contentPixelSize().height() / m_scaleY)
                                   : contentPixelSize().height() / m_scaleY;
    appendLine(
        m_overlayVertices,
        {
            {lastPlayheadX, 0.0   },
            {lastPlayheadX, height},
            QColor(150, 155, 165, 180), 1.0F
    });
    appendLine(m_overlayVertices,
               {
                   {playheadX, 0.0   },
                   {playheadX, height},
                   QColor(245, 245, 245), 1.0F
    });
}

void RhiEditorCanvasWidget::updateScrollRanges() {
    const auto contentSize = contentPixelSize();
    auto logicalExtent = m_snapshot ? m_snapshot->logicalExtent : QSizeF();
    const auto sceneLengthWidth =
        m_sceneLength * pixelsPerQuarterNote() / AppGlobal::ticksPerQuarterNote;
    logicalExtent.setWidth(qMax(logicalExtent.width(), sceneLengthWidth));
    if (m_kind == EditorCanvasKind::TrackEditor)
        logicalExtent.setHeight(qMax(logicalExtent.height(), TracksEditorGlobal::trackHeight));
    else
        logicalExtent.setHeight(qMax(logicalExtent.height(), 128.0 * pianoKeyHeight));

    m_horizontalScrollBar->setPageStep(contentSize.width());
    m_horizontalScrollBar->setRange(
        0, qMax(0, qRound(logicalExtent.width() * m_scaleX) - contentSize.width()));
    m_verticalScrollBar->setPageStep(contentSize.height());
    m_verticalScrollBar->setRange(
        0, qMax(0, qRound(logicalExtent.height() * m_scaleY) - contentSize.height()));
    updateViewportSignals();
}

void RhiEditorCanvasWidget::updateViewportSignals() {
    const auto camera = cameraPosition();
    const auto contentSize = contentPixelSize();
    const QRectF visible(camera.x(), camera.y(), contentSize.width() / m_scaleX,
                         contentSize.height() / m_scaleY);
    emit visibleRectChanged(visible);
    emit timeRangeChanged(startTick(), endTick());
    emit secondaryRangeChanged(topSecondaryValue(), bottomSecondaryValue());
}

void RhiEditorCanvasWidget::scheduleUpdate() {
    m_updateRequestedAt = m_clock.nsecsElapsed();
    update();
}

void RhiEditorCanvasWidget::reportFailure(const QString &reason) {
    if (m_failureReported)
        return;
    m_failureReported = true;
    qCCritical(editorRhiLog) << reason;
    QTimer::singleShot(0, this, [this, reason] { emit backendFailed(reason); });
}

void RhiEditorCanvasWidget::resetGpuResources() {
    m_pipeline.reset();
    m_textPipeline.reset();
    m_shaderResourceBindings.reset();
    m_textShaderResourceBindings.reset();
    m_uniformBuffer.reset();
    m_vertexBuffer.reset();
    m_overlayVertexBuffer.reset();
    m_textVertexBuffer.reset();
    m_textTexture.reset();
    m_textSampler.reset();
    m_vertexBufferCapacity = 0;
    m_overlayVertexBufferCapacity = 0;
    m_textVertexBufferCapacity = 0;
    m_uploadedAtlasRevision = 0;
    m_rhi = nullptr;
    m_vertexDirty = true;
    m_overlayDirty = true;
    m_cameraDirty = true;
}

QSize RhiEditorCanvasWidget::contentPixelSize() const {
    return {qMax(1, width() - scrollBarExtent), qMax(1, height() - scrollBarExtent)};
}

QPointF RhiEditorCanvasWidget::cameraPosition() const {
    return {m_horizontalScrollBar->value() / m_scaleX, m_verticalScrollBar->value() / m_scaleY};
}

double RhiEditorCanvasWidget::pixelsPerQuarterNote() const {
    return m_kind == EditorCanvasKind::TrackEditor ? TracksEditorGlobal::pixelsPerQuarterNote
                                                   : ClipEditorGlobal::pixelsPerQuarterNote;
}

double RhiEditorCanvasWidget::secondaryUnitHeight() const {
    return m_kind == EditorCanvasKind::TrackEditor ? TracksEditorGlobal::trackHeight
                                                   : pianoKeyHeight;
}

double RhiEditorCanvasWidget::secondaryValueForY(const double y) const {
    if (m_kind == EditorCanvasKind::TrackEditor)
        return y / secondaryUnitHeight() - 0.5;
    return 127.0 - y / secondaryUnitHeight();
}

double RhiEditorCanvasWidget::yForSecondaryValue(const double value) const {
    if (m_kind == EditorCanvasKind::TrackEditor)
        return (value + 0.5) * secondaryUnitHeight();
    return (127.0 - value) * secondaryUnitHeight();
}
