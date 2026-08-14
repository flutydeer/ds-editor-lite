#include "WindowsPlaybackIndicatorCompositor.h"

#include <QRect>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cmath>

using Microsoft::WRL::ComPtr;

namespace {
    bool checkResult(const HRESULT result, const QString &operation, QString *error) {
        if (SUCCEEDED(result))
            return true;
        if (error) {
            *error = QStringLiteral("%1 failed with HRESULT 0x%2")
                         .arg(operation)
                         .arg(static_cast<quint32>(result), 8, 16, QLatin1Char('0'));
        }
        return false;
    }

    bool changed(const float lhs, const float rhs) {
        return std::abs(lhs - rhs) > 0.001f;
    }
}

class WindowsPlaybackIndicatorCompositor::Private {
public:
    bool initialize(QRhi *rhi, QRhiCommandBuffer *commandBuffer, const quintptr windowId,
                    const QColor &initialColor, QString *error) {
        release();
        if (!rhi || rhi->backend() != QRhi::D3D11) {
            if (error)
                *error = QStringLiteral("DirectComposition requires the D3D11 RHI backend");
            return false;
        }

        const auto *nativeHandles =
            static_cast<const QRhiD3D11NativeHandles *>(rhi->nativeHandles());
        if (!nativeHandles || !nativeHandles->dev || !nativeHandles->context || !windowId) {
            if (error)
                *error = QStringLiteral("D3D11 native handles are unavailable");
            return false;
        }

        device = static_cast<ID3D11Device *>(nativeHandles->dev);
        context = static_cast<ID3D11DeviceContext *>(nativeHandles->context);
        ComPtr<IDXGIDevice> dxgiDevice;
        if (!checkResult(device.As(&dxgiDevice), QStringLiteral("QueryInterface(IDXGIDevice)"),
                         error)) {
            release();
            return false;
        }
        if (!checkResult(DCompositionCreateDevice(
                             dxgiDevice.Get(), __uuidof(IDCompositionDevice),
                             reinterpret_cast<void **>(compositionDevice.GetAddressOf())),
                         QStringLiteral("DCompositionCreateDevice"), error) ||
            !checkResult(compositionDevice->CreateTargetForHwnd(reinterpret_cast<HWND>(windowId),
                                                                TRUE, target.GetAddressOf()),
                         QStringLiteral("CreateTargetForHwnd"), error) ||
            !checkResult(compositionDevice->CreateVisual(rootVisual.GetAddressOf()),
                         QStringLiteral("CreateVisual(root)"), error) ||
            !checkResult(compositionDevice->CreateVisual(lineVisual.GetAddressOf()),
                         QStringLiteral("CreateVisual(playback indicator)"), error) ||
            !checkResult(compositionDevice->CreateScaleTransform(lineScale.GetAddressOf()),
                         QStringLiteral("CreateScaleTransform"), error) ||
            !checkResult(compositionDevice->CreateSurface(1, 1, DXGI_FORMAT_R8G8B8A8_UNORM,
                                                          DXGI_ALPHA_MODE_PREMULTIPLIED,
                                                          lineSurface.GetAddressOf()),
                         QStringLiteral("CreateSurface(playback indicator)"), error) ||
            !checkResult(lineVisual->SetContent(lineSurface.Get()), QStringLiteral("SetContent"),
                         error) ||
            !checkResult(lineVisual->SetTransform(lineScale.Get()), QStringLiteral("SetTransform"),
                         error) ||
            !checkResult(target->SetRoot(rootVisual.Get()), QStringLiteral("SetRoot"), error)) {
            release();
            return false;
        }

        color = initialColor;
        lineContentDirty = true;
        ready = true;
        if (!prepare(commandBuffer, error)) {
            release();
            return false;
        }
        return true;
    }

    void release() {
        if (target && compositionDevice) {
            target->SetRoot(nullptr);
            compositionDevice->Commit();
        }
        lineAttached = false;
        ready = false;
        lineContentDirty = false;
        physicalX = 0.0f;
        physicalWidth = 0.0f;
        physicalHeight = 0.0f;
        lineSurface.Reset();
        lineScale.Reset();
        lineVisual.Reset();
        rootVisual.Reset();
        target.Reset();
        compositionDevice.Reset();
        context.Reset();
        device.Reset();
    }

    bool prepare(QRhiCommandBuffer *commandBuffer, QString *error) {
        if (!ready || !lineContentDirty)
            return true;
        if (!commandBuffer) {
            if (error)
                *error = QStringLiteral("a QRhi command buffer is required to update D3D content");
            return false;
        }

        if (lineContentDirty && !drawSurface(commandBuffer, lineSurface.Get(), color,
                                             QStringLiteral("playback indicator"), error)) {
            return false;
        }
        lineContentDirty = false;
        return checkResult(compositionDevice->Commit(), QStringLiteral("Commit"), error);
    }

    bool drawSurface(QRhiCommandBuffer *commandBuffer, IDCompositionSurface *compositionSurface,
                     const QColor &surfaceColor, const QString &description, QString *error) {
        ComPtr<IDXGISurface> surface;
        POINT updateOffset{};
        const auto beginResult = compositionSurface->BeginDraw(
            nullptr, __uuidof(IDXGISurface), reinterpret_cast<void **>(surface.GetAddressOf()),
            &updateOffset);
        if (!checkResult(beginResult, QStringLiteral("BeginDraw(%1)").arg(description), error))
            return false;

        HRESULT drawResult = S_OK;
        ComPtr<ID3D11Resource> resource;
        ComPtr<ID3D11RenderTargetView> renderTarget;
        commandBuffer->beginExternal();
        drawResult = surface.As(&resource);
        if (SUCCEEDED(drawResult))
            drawResult = device->CreateRenderTargetView(resource.Get(), nullptr,
                                                        renderTarget.GetAddressOf());
        if (SUCCEEDED(drawResult)) {
            const auto alpha = static_cast<float>(surfaceColor.alphaF());
            const float clearColor[] = {
                static_cast<float>(surfaceColor.redF()) * alpha,
                static_cast<float>(surfaceColor.greenF()) * alpha,
                static_cast<float>(surfaceColor.blueF()) * alpha,
                alpha,
            };
            context->ClearRenderTargetView(renderTarget.Get(), clearColor);
            context->Flush();
        }
        commandBuffer->endExternal();
        const auto endResult = compositionSurface->EndDraw();
        if (!checkResult(drawResult, QStringLiteral("draw %1 surface").arg(description), error) ||
            !checkResult(endResult, QStringLiteral("EndDraw(%1)").arg(description), error)) {
            return false;
        }
        return true;
    }

    bool setGeometry(const qreal x, const qreal width, const qreal height, const bool visible,
                     QString *error) {
        if (!ready)
            return true;
        const auto nextX = static_cast<float>(x);
        const auto nextWidth = static_cast<float>(std::max<qreal>(0.0, width));
        const auto nextHeight = static_cast<float>(std::max<qreal>(0.0, height));
        const auto nextVisible = visible && nextWidth > 0.0f && nextHeight > 0.0f;
        const auto wasAttached = lineAttached;
        bool dirty = false;

        if (changed(physicalX, nextX)) {
            if (!checkResult(lineVisual->SetOffsetX(nextX), QStringLiteral("SetOffsetX"), error))
                return false;
            physicalX = nextX;
            dirty = true;
        }
        if (changed(physicalWidth, nextWidth)) {
            if (!checkResult(lineScale->SetScaleX(nextWidth), QStringLiteral("SetScaleX"), error))
                return false;
            physicalWidth = nextWidth;
            dirty = true;
        }
        if (changed(physicalHeight, nextHeight)) {
            if (!checkResult(lineScale->SetScaleY(nextHeight), QStringLiteral("SetScaleY"), error))
                return false;
            physicalHeight = nextHeight;
            dirty = true;
        }
        if (nextVisible != lineAttached) {
            const auto result = nextVisible ? rootVisual->AddVisual(lineVisual.Get(), TRUE, nullptr)
                                            : rootVisual->RemoveVisual(lineVisual.Get());
            if (!checkResult(result,
                             nextVisible ? QStringLiteral("AddVisual")
                                         : QStringLiteral("RemoveVisual"),
                             error)) {
                return false;
            }
            lineAttached = nextVisible;
            dirty = true;
        }
        return !dirty || (!wasAttached && !lineAttached) ||
               checkResult(compositionDevice->Commit(), QStringLiteral("Commit"), error);
    }

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDCompositionDevice> compositionDevice;
    ComPtr<IDCompositionTarget> target;
    ComPtr<IDCompositionVisual> rootVisual;
    ComPtr<IDCompositionVisual> lineVisual;
    ComPtr<IDCompositionScaleTransform> lineScale;
    ComPtr<IDCompositionSurface> lineSurface;
    QColor color;
    float physicalX = 0.0f;
    float physicalWidth = 0.0f;
    float physicalHeight = 0.0f;
    bool ready = false;
    bool lineContentDirty = false;
    bool lineAttached = false;
};

WindowsPlaybackIndicatorCompositor::WindowsPlaybackIndicatorCompositor()
    : d(std::make_unique<Private>()) {
}

WindowsPlaybackIndicatorCompositor::~WindowsPlaybackIndicatorCompositor() = default;

bool WindowsPlaybackIndicatorCompositor::initialize(QRhi *rhi, QRhiCommandBuffer *commandBuffer,
                                                    const quintptr windowId, const QColor &color,
                                                    QString *error) {
    return d->initialize(rhi, commandBuffer, windowId, color, error);
}

void WindowsPlaybackIndicatorCompositor::release() {
    d->release();
}

bool WindowsPlaybackIndicatorCompositor::isReady() const {
    return d->ready;
}

bool WindowsPlaybackIndicatorCompositor::hasPendingContentUpdate() const {
    return d->lineContentDirty;
}

bool WindowsPlaybackIndicatorCompositor::setColor(const QColor &color) {
    if (d->color == color)
        return false;
    d->color = color;
    d->lineContentDirty = true;
    return true;
}

bool WindowsPlaybackIndicatorCompositor::prepare(QRhiCommandBuffer *commandBuffer, QString *error) {
    return d->prepare(commandBuffer, error);
}

bool WindowsPlaybackIndicatorCompositor::setGeometry(const qreal physicalX,
                                                     const qreal physicalWidth,
                                                     const qreal physicalHeight, const bool visible,
                                                     QString *error) {
    return d->setGeometry(physicalX, physicalWidth, physicalHeight, visible, error);
}
