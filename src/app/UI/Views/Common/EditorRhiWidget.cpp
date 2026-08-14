#include "EditorRhiWidget.h"

#include <QEvent>
#include <QFile>
#include <QMatrix4x4>
#include <QMetaObject>
#include <QPainterPath>
#include <QRegion>
#include <QVariant>
#include <rhi/qrhi.h>

#ifdef Q_OS_WIN
#  include "WindowsPlaybackIndicatorCompositor.h"
#  include <qt_windows.h>
#endif

#include <algorithm>
#include <cmath>

namespace {
#ifdef Q_OS_WIN
    constexpr auto windowInputSuppressedProperty = "_ds_editorRhiInputSuppressed";
    constexpr auto windowModalActiveProperty = "_ds_editorRhiModalActive";
    constexpr auto windowModalPanelRectProperty = "_ds_editorRhiModalPanelRect";
    constexpr auto windowModalPanelRadiusProperty = "_ds_editorRhiModalPanelRadius";
    constexpr auto windowModalBackdropColorProperty = "_ds_editorRhiModalBackdropColor";
#endif

    QShader loadShader(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader();
    }

    void configurePremultipliedBlend(QRhiGraphicsPipeline *pipeline) {
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::One;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        blend.srcAlpha = QRhiGraphicsPipeline::One;
        blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        pipeline->setTargetBlends({blend});
    }

    void configureTextBlend(QRhiGraphicsPipeline *pipeline) {
        QRhiGraphicsPipeline::TargetBlend blend;
        blend.colorWrite =
            QRhiGraphicsPipeline::R | QRhiGraphicsPipeline::G | QRhiGraphicsPipeline::B;
        blend.enable = true;
        blend.srcColor = QRhiGraphicsPipeline::ConstantColor;
        blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcColor;
        blend.srcAlpha = QRhiGraphicsPipeline::Zero;
        blend.dstAlpha = QRhiGraphicsPipeline::One;
        pipeline->setTargetBlends({blend});
    }
}

class EditorRhiWidget::Private {
public:
    struct TextureResources {
        quint64 generation = 0;
        QSize size;
        qsizetype vertexCapacity = 0;
        std::unique_ptr<QRhiTexture> texture;
        std::unique_ptr<QRhiBuffer> vertexBuffer;
        std::unique_ptr<QRhiShaderResourceBindings> bindings;
    };

    Private(EditorRhiWidget *q, QString diagnosticsTag)
        : q(q), diagnosticsTag(std::move(diagnosticsTag)) {
    }

    void initialize(QRhiCommandBuffer *commandBuffer) {
        release();
        rhi = q->rhi();
        colorTexture = q->colorTexture();
        if (!rhi || !colorTexture) {
            fail(QStringLiteral("QRhi or color texture is unavailable"));
            return;
        }

        const QRhiTextureRenderTargetDescription targetDescription{
            QRhiColorAttachment(colorTexture)};
        renderTarget.reset(rhi->newTextureRenderTarget(targetDescription));
        renderPassDescriptor.reset(renderTarget->newCompatibleRenderPassDescriptor());
        renderTarget->setRenderPassDescriptor(renderPassDescriptor.get());
        if (!renderTarget->create()) {
            fail(QStringLiteral("failed to create color-only render target"));
            return;
        }

        uniformBuffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 64));
        if (!uniformBuffer->create()) {
            fail(QStringLiteral("failed to create camera uniform buffer"));
            return;
        }

        solidBindings.reset(rhi->newShaderResourceBindings());
        solidBindings->setBindings({QRhiShaderResourceBinding::uniformBuffer(
            0, QRhiShaderResourceBinding::VertexStage, uniformBuffer.get())});
        if (!solidBindings->create()) {
            fail(QStringLiteral("failed to create solid shader bindings"));
            return;
        }

        fallbackTexture.reset(rhi->newTexture(QRhiTexture::RGBA8, QSize(1, 1)));
        sampler.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if (!fallbackTexture->create() || !sampler->create()) {
            fail(QStringLiteral("failed to create text sampler resources"));
            return;
        }
        fallbackTextBindings.reset(rhi->newShaderResourceBindings());
        fallbackTextBindings->setBindings({
            QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                     uniformBuffer.get()),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                      fallbackTexture.get(), sampler.get()),
        });
        if (!fallbackTextBindings->create()) {
            fail(QStringLiteral("failed to create text shader bindings"));
            return;
        }

        QImage fallbackImage(1, 1, QImage::Format_RGBA8888_Premultiplied);
        fallbackImage.fill(Qt::transparent);
        auto initialUpdates = rhi->nextResourceUpdateBatch();
        initialUpdates->uploadTexture(fallbackTexture.get(), fallbackImage);
        pendingUpdates.reset(initialUpdates);

        if (!createPipelines())
            return;

#ifdef Q_OS_WIN
        QString compositorError;
        if (!playbackIndicatorCompositor.initialize(
                rhi, commandBuffer, static_cast<quintptr>(q->winId()),
                effectivePlaybackIndicatorColor(), &compositorError)) {
            fail(QStringLiteral("failed to initialize playback compositor: %1")
                     .arg(compositorError));
            return;
        }
        updatePlaybackIndicator();
#endif
        resourcesReady = true;
        qInfo().noquote() << QStringLiteral(
                                 "[%1] initialized backend=%2 sampleCount=1 depthStencil=none")
                                 .arg(diagnosticsTag, QString::fromUtf8(rhi->backendName()));
        q->onRhiReady();
    }

    bool createPipelines() {
        const auto solidVertex = loadShader(QStringLiteral(":/editor_rhi/solid.vert.qsb"));
        const auto solidFragment = loadShader(QStringLiteral(":/editor_rhi/solid.frag.qsb"));
        const auto textureVertex = loadShader(QStringLiteral(":/editor_rhi/texture.vert.qsb"));
        const auto textureFragment = loadShader(QStringLiteral(":/editor_rhi/texture.frag.qsb"));
        if (!solidVertex.isValid() || !solidFragment.isValid() || !textureVertex.isValid() ||
            !textureFragment.isValid()) {
            fail(QStringLiteral("failed to load editor RHI shaders"));
            return false;
        }

        solidPipeline.reset(rhi->newGraphicsPipeline());
        solidPipeline->setShaderStages({
            {QRhiShaderStage::Vertex,   solidVertex  },
            {QRhiShaderStage::Fragment, solidFragment}
        });
        QRhiVertexInputLayout solidLayout;
        solidLayout.setBindings({{sizeof(EditorRhiSolidVertex)}});
        solidLayout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, offsetof(EditorRhiSolidVertex, x)       },
            {0, 1, QRhiVertexInputAttribute::Float4, offsetof(EditorRhiSolidVertex, r)       },
            {0, 2, QRhiVertexInputAttribute::Float,  offsetof(EditorRhiSolidVertex, coverage)},
        });
        solidPipeline->setVertexInputLayout(solidLayout);
        solidPipeline->setShaderResourceBindings(solidBindings.get());
        solidPipeline->setRenderPassDescriptor(renderPassDescriptor.get());
        solidPipeline->setSampleCount(1);
        solidPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor);
        configurePremultipliedBlend(solidPipeline.get());
        if (!solidPipeline->create()) {
            fail(QStringLiteral("failed to create solid pipeline"));
            return false;
        }

        QRhiVertexInputLayout textLayout;
        textLayout.setBindings({{sizeof(EditorRhiTextVertex)}});
        textLayout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, offsetof(EditorRhiTextVertex, x)},
            {0, 1, QRhiVertexInputAttribute::Float2, offsetof(EditorRhiTextVertex, u)},
            {0, 2, QRhiVertexInputAttribute::Float4, offsetof(EditorRhiTextVertex, r)},
        });

        textPipeline.reset(rhi->newGraphicsPipeline());
        textPipeline->setShaderStages({
            {QRhiShaderStage::Vertex,   textureVertex  },
            {QRhiShaderStage::Fragment, textureFragment}
        });
        textPipeline->setVertexInputLayout(textLayout);
        textPipeline->setShaderResourceBindings(fallbackTextBindings.get());
        textPipeline->setRenderPassDescriptor(renderPassDescriptor.get());
        textPipeline->setSampleCount(1);
        configureTextBlend(textPipeline.get());
        textPipeline->setFlags(QRhiGraphicsPipeline::UsesScissor |
                               QRhiGraphicsPipeline::UsesBlendConstants);
        if (!textPipeline->create()) {
            fail(QStringLiteral("failed to create text pipeline"));
            return false;
        }
        return true;
    }

    EditorRhiFrameData acquireFrame() {
        return std::move(frame);
    }

    void submit(EditorRhiFrameData newFrame) {
        frame = std::move(newFrame);
        frameDirty = true;
        q->update();
    }

    QVector<EditorRhiSolidVertex> submitOverlay(QVector<EditorRhiSolidVertex> newVertices) {
        auto previousVertices = std::move(overlayVertices);
        overlayVertices = std::move(newVertices);
        overlayDirty = true;
        q->update();
        return previousVertices;
    }

    void render(QRhiCommandBuffer *cb) {
        if (!resourcesReady || !cb)
            return;
#ifdef Q_OS_WIN
        if (playbackIndicatorCompositor.hasPendingContentUpdate()) {
            QString compositorError;
            if (!playbackIndicatorCompositor.prepare(cb, &compositorError)) {
                fail(QStringLiteral("failed to update playback compositor: %1")
                         .arg(compositorError));
                return;
            }
        }
#endif
        if (colorTexture != q->colorTexture() || !renderTarget) {
            // Qt recreates the color buffer on resize / DPI change without
            // re-entering initialize(), so detect the swap here and rebuild the
            // render target lazily.
            colorTexture = q->colorTexture();
            rebuildRenderTarget();
            if (!renderTarget)
                return;
        }

        auto *updates = pendingUpdates.release();
        if (!updates)
            updates = rhi->nextResourceUpdateBatch();

        ensureBuffer(solidBuffer, solidBufferCapacity,
                     frame.solidVertices.size() *
                         static_cast<qsizetype>(sizeof(EditorRhiSolidVertex)),
                     frameDirty);
        if (frameDirty && solidBuffer && !frame.solidVertices.isEmpty()) {
            const auto bytes = frame.solidVertices.size() * sizeof(EditorRhiSolidVertex);
            updates->updateDynamicBuffer(solidBuffer.get(), 0, static_cast<quint32>(bytes),
                                         frame.solidVertices.constData());
        }

        ensureBuffer(overlayBuffer, overlayBufferCapacity,
                     overlayVertices.size() * static_cast<qsizetype>(sizeof(EditorRhiSolidVertex)),
                     overlayDirty);
        if (overlayDirty && overlayBuffer && !overlayVertices.isEmpty()) {
            const auto bytes = overlayVertices.size() * sizeof(EditorRhiSolidVertex);
            updates->updateDynamicBuffer(overlayBuffer.get(), 0, static_cast<quint32>(bytes),
                                         overlayVertices.constData());
        }

        for (const auto &batch : frame.textureBatches) {
            auto &resources = textureResources[batch.pageId];
            if (!resources)
                resources = std::make_shared<TextureResources>();
            ensureTextureResources(*resources, batch, updates);
        }

        const auto outputSize = renderTarget->pixelSize();
        QMatrix4x4 projection;
        projection.ortho(0.0f, static_cast<float>(outputSize.width()),
                         static_cast<float>(outputSize.height()), 0.0f, -1.0f, 1.0f);
        projection.translate(static_cast<float>(-frame.physicalCameraOffset.x()),
                             static_cast<float>(-frame.physicalCameraOffset.y()));
        const auto matrix = rhi->clipSpaceCorrMatrix() * projection;
        updates->updateDynamicBuffer(uniformBuffer.get(), 0, 64, matrix.constData());
        cb->beginPass(renderTarget.get(), frame.clearColor, {1.0f, 0}, updates);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
        cb->setScissor(QRhiScissor(0, 0, outputSize.width(), outputSize.height()));

        const auto drawSolid = [&](QRhiBuffer *buffer, const qsizetype vertexOffset,
                                   const qsizetype vertexCount) {
            if (!buffer || vertexCount <= 0)
                return;
            cb->setGraphicsPipeline(solidPipeline.get());
            cb->setShaderResources(solidBindings.get());
            const auto byteOffset =
                static_cast<quint32>(vertexOffset * sizeof(EditorRhiSolidVertex));
            const QRhiCommandBuffer::VertexInput binding(buffer, byteOffset);
            cb->setVertexInput(0, 1, &binding);
            cb->draw(static_cast<quint32>(vertexCount));
        };
        const auto drawTexture = [&](const int pageId, const qsizetype vertexOffset,
                                     const qsizetype vertexCount, const QColor &color) {
            if (vertexCount <= 0)
                return;
            const auto iterator = textureResources.constFind(pageId);
            if (iterator == textureResources.cend() || !iterator.value() ||
                !iterator.value()->vertexBuffer || !iterator.value()->bindings) {
                return;
            }
            const auto byteOffset =
                static_cast<quint32>(vertexOffset * sizeof(EditorRhiTextVertex));
            const QRhiCommandBuffer::VertexInput binding(iterator.value()->vertexBuffer.get(),
                                                         byteOffset);

            cb->setGraphicsPipeline(textPipeline.get());
            cb->setShaderResources(iterator.value()->bindings.get());
            cb->setBlendConstants(color);
            cb->setVertexInput(0, 1, &binding);
            cb->draw(static_cast<quint32>(vertexCount));
        };
        if (!frame.drawList.commands.isEmpty()) {
            for (const auto &command : frame.drawList.commands) {
                if (command.type == EditorRhiDrawCommand::Type::Solid)
                    drawSolid(solidBuffer.get(), command.vertexOffset, command.vertexCount);
                else
                    drawTexture(command.pageId, command.vertexOffset, command.vertexCount,
                                command.color);
            }
        } else {
            drawSolid(solidBuffer.get(), 0, frame.solidVertices.size());
        }
        drawSolid(overlayBuffer.get(), 0, overlayVertices.size());
        cb->endPass();
        frameDirty = false;
        overlayDirty = false;
    }

    void ensureBuffer(std::unique_ptr<QRhiBuffer> &buffer, qsizetype &capacity,
                      const qsizetype requiredBytes, bool &dirty) {
        if (requiredBytes <= 0 || requiredBytes <= capacity)
            return;
        qsizetype newCapacity = std::max<qsizetype>(4096, capacity);
        while (newCapacity < requiredBytes)
            newCapacity *= 2;
        buffer.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
                                    static_cast<quint32>(newCapacity)));
        if (!buffer->create()) {
            buffer.reset();
            fail(QStringLiteral("failed to grow vertex buffer"));
            return;
        }
        capacity = newCapacity;
        dirty = true;
    }

    void rebuildRenderTarget() {
        renderTarget.reset();
        renderPassDescriptor.reset();
        if (!rhi || !colorTexture)
            return;
        const QRhiTextureRenderTargetDescription targetDescription{
            QRhiColorAttachment(colorTexture)};
        renderTarget.reset(rhi->newTextureRenderTarget(targetDescription));
        if (renderTarget) {
            renderPassDescriptor.reset(renderTarget->newCompatibleRenderPassDescriptor());
            renderTarget->setRenderPassDescriptor(renderPassDescriptor.get());
            createPipelines(); // 旧 pipeline 引用旧 descriptor，必须重建
            if (!renderTarget->create()) {
                renderTarget.reset();
                renderPassDescriptor.reset();
                fail(QStringLiteral("failed to recreate render target after buffer swap"));
            }
        }
    }

    void invalidateTextures() {
        textureResources.clear();
        frameDirty = true;
    }

    void ensureTextureResources(TextureResources &resources, const EditorRhiTextureBatch &batch,
                                QRhiResourceUpdateBatch *updates) {
        if (batch.image.isNull())
            return;
        if (!resources.texture || resources.size != batch.image.size()) {
            resources.bindings.reset();
            resources.texture.reset(rhi->newTexture(QRhiTexture::RGBA8, batch.image.size()));
            if (!resources.texture->create()) {
                fail(QStringLiteral("failed to create glyph atlas texture"));
                return;
            }
            resources.size = batch.image.size();
            resources.generation = 0;
        }
        if (resources.generation != batch.generation) {
            updates->uploadTexture(resources.texture.get(), batch.image);
            resources.generation = batch.generation;
        }
        if (!resources.bindings) {
            resources.bindings.reset(rhi->newShaderResourceBindings());
            resources.bindings->setBindings({
                QRhiShaderResourceBinding::uniformBuffer(0, QRhiShaderResourceBinding::VertexStage,
                                                         uniformBuffer.get()),
                QRhiShaderResourceBinding::sampledTexture(1,
                                                          QRhiShaderResourceBinding::FragmentStage,
                                                          resources.texture.get(), sampler.get()),
            });
            if (!resources.bindings->create()) {
                fail(QStringLiteral("failed to create glyph atlas bindings"));
                return;
            }
        }
        const auto required =
            batch.vertices.size() * static_cast<qsizetype>(sizeof(EditorRhiTextVertex));
        ensureBuffer(resources.vertexBuffer, resources.vertexCapacity, required, frameDirty);
        if (frameDirty && resources.vertexBuffer && !batch.vertices.isEmpty()) {
            updates->updateDynamicBuffer(resources.vertexBuffer.get(), 0,
                                         static_cast<quint32>(required),
                                         batch.vertices.constData());
        }
    }

    void submitted() {
        q->onFrameSubmitted();
    }

    void fail(const QString &reason) {
        if (failureRequested)
            return;
        failureRequested = true;
        qCritical().noquote() << QStringLiteral("[%1] %2").arg(diagnosticsTag, reason);
        QMetaObject::invokeMethod(
            q, [this, reason] { emit q->backendFailed(reason); }, Qt::QueuedConnection);
    }

    void release() {
        resourcesReady = false;
#ifdef Q_OS_WIN
        playbackIndicatorCompositor.release();
#endif
        textureResources.clear();
        textPipeline.reset();
        solidPipeline.reset();
        fallbackTextBindings.reset();
        solidBindings.reset();
        sampler.reset();
        fallbackTexture.reset();
        overlayBuffer.reset();
        solidBuffer.reset();
        uniformBuffer.reset();
        pendingUpdates.reset();
        renderTarget.reset();
        renderPassDescriptor.reset();
        overlayBufferCapacity = 0;
        solidBufferCapacity = 0;
        colorTexture = nullptr;
        rhi = nullptr;
    }

#ifdef Q_OS_WIN
    void updatePlaybackIndicator() {
        if (failureRequested || !playbackIndicatorCompositor.isReady())
            return;
        const auto dpr = std::max<qreal>(1.0, q->devicePixelRatioF());
        const auto physicalWidth = q->width() * dpr;
        const auto physicalCenter = playbackIndicatorPosition * dpr;
        const auto physicalX = physicalCenter - 0.5 * dpr;
        const auto inViewport = std::isfinite(playbackIndicatorPosition) &&
                                physicalCenter >= -2.0 * dpr &&
                                physicalCenter <= physicalWidth + 2.0 * dpr;
        QString compositorError;
        if (!playbackIndicatorCompositor.setGeometry(
                physicalX, dpr, q->height() * dpr,
                playbackIndicatorVisible && q->isVisible() && inViewport, &compositorError)) {
            fail(QStringLiteral("failed to move playback compositor: %1").arg(compositorError));
        }
    }

    void setPlaybackIndicatorPosition(const qreal position) {
        if (playbackIndicatorPosition == position)
            return;
        playbackIndicatorPosition = position;
        updatePlaybackIndicator();
    }

    [[nodiscard]] QColor effectivePlaybackIndicatorColor() const {
        if (!modalVisualActive || modalBackdropColor.alpha() <= 0)
            return playbackIndicatorColor;
        const auto backdropAlpha = modalBackdropColor.alphaF();
        const auto blendChannel = [&](const int foreground, const int backdrop) {
            return qRound(foreground * (1.0 - backdropAlpha) + backdrop * backdropAlpha);
        };
        return QColor(blendChannel(playbackIndicatorColor.red(), modalBackdropColor.red()),
                      blendChannel(playbackIndicatorColor.green(), modalBackdropColor.green()),
                      blendChannel(playbackIndicatorColor.blue(), modalBackdropColor.blue()),
                      playbackIndicatorColor.alpha());
    }

    void updatePlaybackIndicatorColor() {
        if (playbackIndicatorCompositor.setColor(effectivePlaybackIndicatorColor()) &&
            playbackIndicatorCompositor.isReady()) {
            q->update();
        }
    }

    void setPlaybackIndicatorColor(const QColor &color) {
        if (playbackIndicatorColor == color)
            return;
        playbackIndicatorColor = color;
        updatePlaybackIndicatorColor();
    }

    void setPlaybackIndicatorVisible(const bool visible) {
        if (playbackIndicatorVisible == visible)
            return;
        playbackIndicatorVisible = visible;
        updatePlaybackIndicator();
    }

    void restoreOriginalMask() {
        if (!modalMaskOwned)
            return;
        if (hadOriginalMask)
            q->setMask(originalMask);
        else
            q->clearMask();
        originalMask = {};
        hadOriginalMask = false;
        modalMaskOwned = false;
    }

    void updateModalMask() {
        if (!modalVisualActive || !modalMaskOwned)
            return;
        auto *topLevel = q->window();
        if (!topLevel)
            return;

        const QRect localPanelRect(q->mapFrom(topLevel, modalPanelRect.topLeft()),
                                   modalPanelRect.size());
        if (!localPanelRect.intersects(q->rect())) {
            if (hadOriginalMask)
                q->setMask(originalMask);
            else
                q->clearMask();
            return;
        }

        QPainterPath panelPath;
        panelPath.addRoundedRect(QRectF(localPanelRect), modalPanelCornerRadius,
                                 modalPanelCornerRadius);
        const QRegion panelRegion(panelPath.toFillPolygon().toPolygon(), Qt::WindingFill);
        const QRegion baseRegion = hadOriginalMask ? originalMask : QRegion(q->rect());
        q->setMask(baseRegion.subtracted(panelRegion));
    }

    void setModalVisualState(const bool active, const QRect &panelRect,
                             const qreal panelCornerRadius, const QColor &backdropColor) {
        modalPanelRect = panelRect;
        this->modalPanelCornerRadius = std::max<qreal>(0.0, panelCornerRadius);
        modalBackdropColor = backdropColor;

        if (active && !modalMaskOwned) {
            originalMask = q->mask();
            hadOriginalMask = !originalMask.isEmpty();
            modalMaskOwned = true;
        }
        modalVisualActive = active;
        if (modalVisualActive)
            updateModalMask();
        else
            restoreOriginalMask();
        updatePlaybackIndicatorColor();
    }
#endif

    EditorRhiWidget *q;
    QString diagnosticsTag;
    EditorRhiFrameData frame;
    QVector<EditorRhiSolidVertex> overlayVertices;
    bool frameDirty = true;
    bool overlayDirty = true;
    bool resourcesReady = false;
    bool failureRequested = false;
    QRhi *rhi = nullptr;
    QRhiTexture *colorTexture = nullptr;
    qsizetype overlayBufferCapacity = 0;
    qsizetype solidBufferCapacity = 0;
    std::unique_ptr<QRhiTextureRenderTarget> renderTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> renderPassDescriptor;
    std::unique_ptr<QRhiBuffer> uniformBuffer;
    std::unique_ptr<QRhiBuffer> overlayBuffer;
    std::unique_ptr<QRhiBuffer> solidBuffer;
    std::unique_ptr<QRhiShaderResourceBindings> solidBindings;
    std::unique_ptr<QRhiGraphicsPipeline> solidPipeline;
    std::unique_ptr<QRhiTexture> fallbackTexture;
    std::unique_ptr<QRhiSampler> sampler;
    std::unique_ptr<QRhiShaderResourceBindings> fallbackTextBindings;
    std::unique_ptr<QRhiGraphicsPipeline> textPipeline;
    std::unique_ptr<QRhiResourceUpdateBatch> pendingUpdates;
    QHash<int, std::shared_ptr<TextureResources>> textureResources;
    bool inputSuppressed = false;
#ifdef Q_OS_WIN
    WindowsPlaybackIndicatorCompositor playbackIndicatorCompositor;
    qreal playbackIndicatorPosition = 0.0;
    QColor playbackIndicatorColor{200, 200, 200};
    bool playbackIndicatorVisible = false;
    QRect modalPanelRect;
    qreal modalPanelCornerRadius = 0.0;
    QColor modalBackdropColor{0, 0, 0, 96};
    QRegion originalMask;
    bool modalVisualActive = false;
    bool modalMaskOwned = false;
    bool hadOriginalMask = false;
#endif
};

EditorRhiWidget::EditorRhiWidget(QString diagnosticsTag, QWidget *parent)
    : QRhiWidget(parent), d(std::make_unique<Private>(this, std::move(diagnosticsTag))) {
#ifdef Q_OS_WIN
    // Isolate the D3D surface from unrelated backing-store flushes in the top-level widget.
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    setAttribute(Qt::WA_NativeWindow);
    setApi(QRhiWidget::Api::Direct3D11);
    if (parent) {
        auto *root = parent->window();
        d->inputSuppressed = root->property(windowInputSuppressedProperty).toBool();
        const auto backdropValue = root->property(windowModalBackdropColorProperty);
        d->setModalVisualState(root->property(windowModalActiveProperty).toBool(),
                               root->property(windowModalPanelRectProperty).toRect(),
                               root->property(windowModalPanelRadiusProperty).toReal(),
                               backdropValue.canConvert<QColor>() ? backdropValue.value<QColor>()
                                                                  : d->modalBackdropColor);
    }
#endif
    setSampleCount(1);
    setColorBufferFormat(QRhiWidget::TextureFormat::RGBA8);
    setAutoRenderTarget(false);
    setFocusPolicy(Qt::StrongFocus);
    connect(this, &QRhiWidget::renderFailed, this, [this] {
        requestBackendFailure(QStringLiteral("QRhiWidget reported render failure"));
    });
    connect(this, &QRhiWidget::frameSubmitted, this, [this] { d->submitted(); });
}

EditorRhiWidget::~EditorRhiWidget() = default;

void EditorRhiWidget::setWindowInputSuppressed(QWidget *window, const bool suppressed) {
#ifdef Q_OS_WIN
    if (!window)
        return;
    auto *root = window->window();
    root->setProperty(windowInputSuppressedProperty, suppressed);
    const auto surfaces = root->findChildren<EditorRhiWidget *>();
    for (auto *surface : surfaces)
        surface->setInputSuppressed(suppressed);
#else
    Q_UNUSED(window)
    Q_UNUSED(suppressed)
#endif
}

void EditorRhiWidget::setWindowModalVisualState(QWidget *window, const bool active,
                                                const QRect &panelRect,
                                                const qreal panelCornerRadius,
                                                const QColor &backdropColor) {
#ifdef Q_OS_WIN
    if (!window)
        return;
    auto *root = window->window();
    root->setProperty(windowModalActiveProperty, active);
    root->setProperty(windowModalPanelRectProperty, panelRect);
    root->setProperty(windowModalPanelRadiusProperty, panelCornerRadius);
    root->setProperty(windowModalBackdropColorProperty, backdropColor);
    const auto surfaces = root->findChildren<EditorRhiWidget *>();
    for (auto *surface : surfaces)
        surface->setModalVisualState(active, panelRect, panelCornerRadius, backdropColor);
#else
    Q_UNUSED(window)
    Q_UNUSED(active)
    Q_UNUSED(panelRect)
    Q_UNUSED(panelCornerRadius)
    Q_UNUSED(backdropColor)
#endif
}

void EditorRhiWidget::setInputSuppressed(const bool suppressed) {
    d->inputSuppressed = suppressed;
}

void EditorRhiWidget::setModalVisualState(const bool active, const QRect &panelRect,
                                          const qreal panelCornerRadius,
                                          const QColor &backdropColor) {
#ifdef Q_OS_WIN
    d->setModalVisualState(active, panelRect, panelCornerRadius, backdropColor);
#else
    Q_UNUSED(active)
    Q_UNUSED(panelRect)
    Q_UNUSED(panelCornerRadius)
    Q_UNUSED(backdropColor)
#endif
}

EditorRhiFrameData EditorRhiWidget::acquireFrame() {
    return d->acquireFrame();
}

void EditorRhiWidget::submitFrame(EditorRhiFrameData frame) {
    d->submit(std::move(frame));
}

QVector<EditorRhiSolidVertex>
    EditorRhiWidget::submitOverlay(QVector<EditorRhiSolidVertex> vertices) {
    return d->submitOverlay(std::move(vertices));
}

QPointF EditorRhiWidget::physicalWindowOffset() const {
    const auto *topLevel = window();
    return topLevel ? mapTo(topLevel, QPointF()) * devicePixelRatioF() : QPointF();
}

void EditorRhiWidget::requestBackendFailure(const QString &reason) {
    d->fail(reason);
}

void EditorRhiWidget::initialize(QRhiCommandBuffer *commandBuffer) {
    d->initialize(commandBuffer);
}

void EditorRhiWidget::render(QRhiCommandBuffer *cb) {
    d->render(cb);
}

void EditorRhiWidget::releaseResources() {
    d->release();
}

bool EditorRhiWidget::event(QEvent *event) {
    const auto result = QRhiWidget::event(event);
    if (event->type() == QEvent::DevicePixelRatioChange)
        onDevicePixelRatioChanged();
#ifdef Q_OS_WIN
    const auto eventType = event->type();
    if (eventType == QEvent::Move || eventType == QEvent::Resize ||
        eventType == QEvent::DevicePixelRatioChange) {
        d->updateModalMask();
    }
    if (eventType == QEvent::DevicePixelRatioChange || eventType == QEvent::Resize ||
        eventType == QEvent::Show || eventType == QEvent::Hide) {
        d->updatePlaybackIndicator();
    }
#endif
    return result;
}

#ifdef Q_OS_WIN
void EditorRhiWidget::setPlaybackIndicatorPosition(const qreal position) {
    d->setPlaybackIndicatorPosition(position);
}

void EditorRhiWidget::setPlaybackIndicatorColor(const QColor &color) {
    d->setPlaybackIndicatorColor(color);
}

void EditorRhiWidget::setPlaybackIndicatorVisible(const bool visible) {
    d->setPlaybackIndicatorVisible(visible);
}
#endif

bool EditorRhiWidget::nativeEvent(const QByteArray &eventType, void *message, qintptr *result) {
#ifdef Q_OS_WIN
    if (d->inputSuppressed) {
        const auto *nativeMessage = static_cast<MSG *>(message);
        if (nativeMessage && nativeMessage->message == WM_NCHITTEST && result) {
            *result = HTTRANSPARENT;
            return true;
        }
    }
#endif
    return QRhiWidget::nativeEvent(eventType, message, result);
}

void EditorRhiWidget::onRhiReady() {
}

void EditorRhiWidget::onFrameSubmitted() {
}

void EditorRhiWidget::onDevicePixelRatioChanged() {
    d->invalidateTextures();
}
