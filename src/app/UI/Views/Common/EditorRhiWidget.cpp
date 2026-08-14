#include "EditorRhiWidget.h"

#include "Model/AppOptions/AppOptions.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QMatrix4x4>
#include <QMetaObject>
#include <QTimer>
#include <rhi/qrhi.h>

#include <algorithm>

namespace {
    constexpr int kStatsWindow = 120;

    QShader loadShader(const QString &path) {
        QFile file(path);
        return file.open(QIODevice::ReadOnly) ? QShader::fromSerialized(file.readAll()) : QShader();
    }

    double percentile(const QVector<double> &values, const double ratio) {
        if (values.isEmpty())
            return 0.0;
        auto sorted = values;
        std::sort(sorted.begin(), sorted.end());
        const auto index = static_cast<qsizetype>(std::clamp(ratio, 0.0, 1.0) *
                                                  static_cast<double>(sorted.size() - 1));
        return sorted.at(index);
    }

    QString statsTriple(const QVector<double> &values) {
        return QStringLiteral("%1/%2/%3")
            .arg(percentile(values, 0.50), 0, 'f', 3)
            .arg(percentile(values, 0.95), 0, 'f', 3)
            .arg(percentile(values, 0.99), 0, 'f', 3);
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
        clock.start();
    }

    void initialize() {
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
        updateRequestedNs = clock.nsecsElapsed();
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
        if (colorTexture != q->colorTexture() || !renderTarget) {
            // Qt recreates the color buffer on resize / DPI change without
            // re-entering initialize(), so detect the swap here and rebuild the
            // render target lazily.
            colorTexture = q->colorTexture();
            rebuildRenderTarget();
            if (!renderTarget)
                return;
        }

        QElapsedTimer uploadTimer;
        uploadTimer.start();
        auto *updates = pendingUpdates.release();
        if (!updates)
            updates = rhi->nextResourceUpdateBatch();

        ensureBuffer(solidBuffer, solidBufferCapacity,
                     frame.solidVertices.size() *
                         static_cast<qsizetype>(sizeof(EditorRhiSolidVertex)),
                     frameDirty);
        double uploadedBytes = 0.0;
        if (frameDirty && solidBuffer && !frame.solidVertices.isEmpty()) {
            const auto bytes = frame.solidVertices.size() * sizeof(EditorRhiSolidVertex);
            updates->updateDynamicBuffer(solidBuffer.get(), 0, static_cast<quint32>(bytes),
                                         frame.solidVertices.constData());
            uploadedBytes += bytes;
        }

        ensureBuffer(overlayBuffer, overlayBufferCapacity,
                     overlayVertices.size() *
                         static_cast<qsizetype>(sizeof(EditorRhiSolidVertex)),
                     overlayDirty);
        if (overlayDirty && overlayBuffer && !overlayVertices.isEmpty()) {
            const auto bytes = overlayVertices.size() * sizeof(EditorRhiSolidVertex);
            updates->updateDynamicBuffer(overlayBuffer.get(), 0, static_cast<quint32>(bytes),
                                         overlayVertices.constData());
            uploadedBytes += bytes;
        }

        for (const auto &batch : frame.textureBatches) {
            auto &resources = textureResources[batch.pageId];
            if (!resources)
                resources = std::make_shared<TextureResources>();
            ensureTextureResources(*resources, batch, updates, uploadedBytes);
        }

        const auto outputSize = renderTarget->pixelSize();
        QMatrix4x4 projection;
        projection.ortho(0.0f, static_cast<float>(outputSize.width()),
                         static_cast<float>(outputSize.height()), 0.0f, -1.0f, 1.0f);
        projection.translate(static_cast<float>(-frame.physicalCameraOffset.x()),
                             static_cast<float>(-frame.physicalCameraOffset.y()));
        const auto matrix = rhi->clipSpaceCorrMatrix() * projection;
        updates->updateDynamicBuffer(uniformBuffer.get(), 0, 64, matrix.constData());
        const auto uploadMs = uploadTimer.nsecsElapsed() / 1000000.0;

        QElapsedTimer encodeTimer;
        encodeTimer.start();
        cb->beginPass(renderTarget.get(), frame.clearColor, {1.0f, 0}, updates);
        cb->setViewport(QRhiViewport(0, 0, outputSize.width(), outputSize.height()));
        cb->setScissor(QRhiScissor(0, 0, outputSize.width(), outputSize.height()));

        int drawCalls = 0;
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
            ++drawCalls;
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
            ++drawCalls;
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
        const auto encodeMs = encodeTimer.nsecsElapsed() / 1000000.0;
        frameDirty = false;
        overlayDirty = false;

        if (appOptions->developer()->enableDiagnostics) {
            uploadSamples.append(uploadMs);
            encodeSamples.append(encodeMs);
            cpuSamples.append(uploadMs + encodeMs);
            vertexSamples.append(frame.solidVertices.size());
            uploadByteSamples.append(uploadedBytes);
            drawCallSamples.append(drawCalls);
        }
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
                                QRhiResourceUpdateBatch *updates, double &uploadedBytes) {
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
            uploadedBytes += batch.image.sizeInBytes();
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
            uploadedBytes += required;
        }
    }

    void submitted() {
        const auto now = clock.nsecsElapsed();
        if (lastSubmittedNs != 0) {
            const auto interval = (now - lastSubmittedNs) / 1000000.0;
            if (interval <= 500.0 && appOptions->developer()->enableDiagnostics)
                intervalSamples.append(interval);
        }
        lastSubmittedNs = now;
        if (updateRequestedNs != 0 && appOptions->developer()->enableDiagnostics) {
            submitSamples.append((now - updateRequestedNs) / 1000000.0);
            updateRequestedNs = 0;
        }
        q->onFrameSubmitted();

        if (!appOptions->developer()->enableDiagnostics || cpuSamples.size() < kStatsWindow)
            return;
        qInfo().noquote() << QStringLiteral(
                                 "[%1Stats] frames=%2 cpuMs=%3 uploadMs=%4 encodeMs=%5 "
                                 "submitMs=%6 intervalMs=%7 vertices=%8 uploadBytes=%9 draws=%10")
                                 .arg(diagnosticsTag)
                                 .arg(cpuSamples.size())
                                 .arg(statsTriple(cpuSamples), statsTriple(uploadSamples),
                                      statsTriple(encodeSamples), statsTriple(submitSamples),
                                      statsTriple(intervalSamples), statsTriple(vertexSamples),
                                      statsTriple(uploadByteSamples), statsTriple(drawCallSamples));
        cpuSamples.clear();
        uploadSamples.clear();
        encodeSamples.clear();
        submitSamples.clear();
        intervalSamples.clear();
        vertexSamples.clear();
        uploadByteSamples.clear();
        drawCallSamples.clear();
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

    QElapsedTimer clock;
    qint64 updateRequestedNs = 0;
    qint64 lastSubmittedNs = 0;
    QVector<double> cpuSamples;
    QVector<double> uploadSamples;
    QVector<double> encodeSamples;
    QVector<double> submitSamples;
    QVector<double> intervalSamples;
    QVector<double> vertexSamples;
    QVector<double> uploadByteSamples;
    QVector<double> drawCallSamples;
};

EditorRhiWidget::EditorRhiWidget(QString diagnosticsTag, QWidget *parent)
    : QRhiWidget(parent), d(std::make_unique<Private>(this, std::move(diagnosticsTag))) {
#ifdef Q_OS_WIN
    setApi(QRhiWidget::Api::Direct3D11);
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

void EditorRhiWidget::initialize(QRhiCommandBuffer *) {
    d->initialize();
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
    return result;
}

void EditorRhiWidget::onRhiReady() {
}

void EditorRhiWidget::onFrameSubmitted() {
}

void EditorRhiWidget::onDevicePixelRatioChanged() {
    d->invalidateTextures();
}
