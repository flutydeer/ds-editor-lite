#include "PlaybackIndicatorOverlay.h"

#include <QEvent>
#include <QPainter>

#ifdef Q_OS_WIN
#  include <qt_windows.h>
#endif

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <mutex>

#ifdef Q_OS_WIN
class NativePlaybackIndicatorWindow {
public:
    HWND handle = nullptr;
    QColor color = {200, 200, 200};
    HDC memoryDc = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ previousBitmap = nullptr;
    void *bits = nullptr;
    int bitmapWidth = 0;
    int bitmapHeight = 0;

    ~NativePlaybackIndicatorWindow() {
        if (handle && IsWindow(handle))
            DestroyWindow(handle);
        releaseBitmap();
        if (memoryDc)
            DeleteDC(memoryDc);
    }

    bool ensureBitmap(const int width, const int height) {
        if (bitmap && bitmapWidth == width && bitmapHeight == height)
            return true;
        releaseBitmap();
        if (!memoryDc)
            memoryDc = CreateCompatibleDC(nullptr);
        if (!memoryDc)
            return false;

        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(info.bmiHeader);
        info.bmiHeader.biWidth = width;
        info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(memoryDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap)
            return false;
        previousBitmap = SelectObject(memoryDc, bitmap);
        bitmapWidth = width;
        bitmapHeight = height;
        return true;
    }

private:
    void releaseBitmap() {
        if (bitmap) {
            if (memoryDc && previousBitmap)
                SelectObject(memoryDc, previousBitmap);
            DeleteObject(bitmap);
        }
        bitmap = nullptr;
        previousBitmap = nullptr;
        bits = nullptr;
        bitmapWidth = 0;
        bitmapHeight = 0;
    }
};

namespace {
    constexpr auto nativeIndicatorClassName = L"DsEditorLite.PlaybackIndicatorOverlay";

    LRESULT CALLBACK nativeIndicatorWindowProc(HWND window, const UINT message, const WPARAM wParam,
                                               const LPARAM lParam) {
        if (message == WM_NCHITTEST)
            return HTTRANSPARENT;
        if (message == WM_MOUSEACTIVATE)
            return MA_NOACTIVATE;
        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool ensureNativeIndicatorClass() {
        static std::once_flag once;
        static bool available = false;
        std::call_once(once, [] {
            WNDCLASSEXW windowClass{};
            windowClass.cbSize = sizeof(windowClass);
            windowClass.lpfnWndProc = nativeIndicatorWindowProc;
            windowClass.hInstance = GetModuleHandleW(nullptr);
            windowClass.lpszClassName = nativeIndicatorClassName;
            available =
                RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
        });
        return available;
    }
}
#endif

namespace {
    constexpr auto windowSuppressedProperty = "_ds_playbackIndicatorsSuppressed";
}

PlaybackIndicatorOverlay::PlaybackIndicatorOverlay(const Shape shape, QWidget *parent,
                                                   const Surface surface)
    : QWidget(parent), m_shape(shape), m_surface(surface) {
#ifndef Q_OS_WIN
    if (m_surface == Surface::NativeCompositor)
        m_surface = Surface::SharedWidget;
#endif
    setObjectName(QStringLiteral("playbackIndicatorOverlay"));
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(false);
    if (parent) {
        m_suppressed = parent->window()->property(windowSuppressedProperty).toBool();
        parent->installEventFilter(this);
        if (usesNativeCompositor() && parent->window() != parent)
            parent->window()->installEventFilter(this);
    }
    if (usesNativeCompositor()) {
#ifdef Q_OS_WIN
        ensureNativeWindow();
#endif
    } else {
        show();
        raise();
    }
    updateGeometry();
}

PlaybackIndicatorOverlay::~PlaybackIndicatorOverlay() {
#ifdef Q_OS_WIN
    m_nativeWindow.reset();
#endif
}

void PlaybackIndicatorOverlay::setWindowIndicatorsSuppressed(QWidget *window,
                                                             const bool suppressed) {
    if (!window)
        return;
    auto *root = window->window();
    root->setProperty(windowSuppressedProperty, suppressed);
    const auto overlays = root->findChildren<PlaybackIndicatorOverlay *>();
    for (auto *overlay : overlays)
        overlay->setSuppressed(suppressed);
}

void PlaybackIndicatorOverlay::setPosition(const qreal x) {
    if (m_position == x)
        return;
    if (m_suppressed) {
        m_position = x;
        return;
    }
    const auto oldPosition = m_position;
    const auto oldPositionVisible = isPositionVisible(oldPosition);
    const auto oldRect = indicatorRect(oldPosition);
    m_position = x;
    const auto positionVisible = isPositionVisible(m_position);
    const auto availableWidth =
        usesNativeCompositor() && parentWidget() ? parentWidget()->width() : width();
    const auto crossesViewport = oldPositionVisible && positionVisible && availableWidth > 0 &&
                                 std::abs(m_position - oldPosition) > availableWidth / 2.0;
    const auto needsDeferredRefresh = (!oldPositionVisible && positionVisible) || crossesViewport;

    if (usesNativeCompositor())
        updateGeometry();
    else
        update(oldRect.united(indicatorRect(m_position)));

    // Page turns can repaint the parent after moving the indicator back to the viewport.
    if (needsDeferredRefresh)
        scheduleRefresh();
}

void PlaybackIndicatorOverlay::setColor(const QColor &color) {
    if (m_color == color)
        return;
    m_color = color;
    if (usesNativeCompositor()) {
#ifdef Q_OS_WIN
        if (m_nativeWindow) {
            m_nativeWindow->color = color;
            updateGeometry();
        }
#endif
    } else {
        update(indicatorRect(m_position));
    }
}

void PlaybackIndicatorOverlay::setIndicatorVisible(const bool visible) {
    if (m_indicatorVisible == visible)
        return;
    const auto dirtyRect = indicatorRect(m_position);
    m_indicatorVisible = visible;
    if (usesNativeCompositor())
        updateGeometry();
    else
        update(dirtyRect);
}

bool PlaybackIndicatorOverlay::eventFilter(QObject *watched, QEvent *event) {
    const auto parent = parentWidget();
    const auto watchedParent = watched == parent;
    const auto watchedWindow = parent && watched == parent->window();
    if (usesNativeCompositor() && (watchedParent || watchedWindow)) {
        if (event->type() == QEvent::Hide) {
#ifdef Q_OS_WIN
            hideNativeWindow();
#endif
        } else if (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
                   event->type() == QEvent::Show || event->type() == QEvent::WindowStateChange ||
                   event->type() == QEvent::DevicePixelRatioChange) {
            updateGeometry();
            scheduleRefresh();
        }
    } else if (watchedParent && (event->type() == QEvent::Resize || event->type() == QEvent::Show ||
                                 event->type() == QEvent::DevicePixelRatioChange)) {
        raise();
        updateGeometry();
        update();
        scheduleRefresh();
    }
    return QWidget::eventFilter(watched, event);
}

void PlaybackIndicatorOverlay::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    if (usesNativeCompositor())
        return;
    if (m_suppressed || !m_indicatorVisible || !isPositionVisible(m_position))
        return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(m_color);

    if (m_shape == Shape::Line) {
        pen.setWidthF(1.0);
        painter.setPen(pen);
        painter.drawLine(QLineF(m_position, 0, m_position, height()));
        return;
    }

    constexpr qreal penWidth = 2.0;
    constexpr qreal triangleWidth = 12.0;
    constexpr qreal triangleHeight = 1.73205 * triangleWidth / 2.0;
    pen.setWidthF(penWidth);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(m_color);
    const auto top = height() - triangleHeight - penWidth;
    const QPointF points[] = {
        {m_position - triangleWidth / 2.0, top                 },
        {m_position + triangleWidth / 2.0, top                 },
        {m_position,                       top + triangleHeight}
    };
    painter.drawPolygon(points, 3);
}

QRect PlaybackIndicatorOverlay::indicatorRect(const qreal position) const {
    if (!isPositionVisible(position))
        return {};
    if (m_shape == Shape::Line)
        return QRectF(position - 2.0, 0, 4.0, height()).toAlignedRect();

    constexpr qreal triangleWidth = 12.0;
    constexpr qreal triangleHeight = 1.73205 * triangleWidth / 2.0;
    return QRectF(position - triangleWidth / 2.0 - 2.0, height() - triangleHeight - 4.0,
                  triangleWidth + 4.0, triangleHeight + 4.0)
        .toAlignedRect();
}

bool PlaybackIndicatorOverlay::isPositionVisible(const qreal position) const {
    if (!std::isfinite(position))
        return false;
    const qreal margin = m_shape == Shape::Line ? 1.0 : 8.0;
    const auto availableWidth =
        usesNativeCompositor() && parentWidget() ? parentWidget()->width() : width();
    return position >= -margin && position < availableWidth + margin;
}

bool PlaybackIndicatorOverlay::usesNativeCompositor() const {
    return m_surface == Surface::NativeCompositor;
}

void PlaybackIndicatorOverlay::setSuppressed(const bool suppressed) {
    if (m_suppressed == suppressed)
        return;
    const auto dirtyRect = indicatorRect(m_position);
    m_suppressed = suppressed;
    if (usesNativeCompositor())
        updateGeometry();
    else
        update(dirtyRect);
}

void PlaybackIndicatorOverlay::scheduleRefresh() {
    if (m_refreshPending)
        return;
    m_refreshPending = true;
    QMetaObject::invokeMethod(
        this,
        [this] {
            m_refreshPending = false;
            updateGeometry();
            if (!usesNativeCompositor()) {
                raise();
                update();
            }
        },
        Qt::QueuedConnection);
}

void PlaybackIndicatorOverlay::updateGeometry() {
    const auto parent = parentWidget();
    if (!parent)
        return;
    if (!usesNativeCompositor()) {
        setGeometry(parent->rect());
        return;
    }

#ifdef Q_OS_WIN
    ensureNativeWindow();
    if (!m_nativeWindow || !m_nativeWindow->handle || m_suppressed || !m_indicatorVisible ||
        !isPositionVisible(m_position) || !parent->isVisible()) {
        hideNativeWindow();
        return;
    }

    const auto parentWindow = reinterpret_cast<HWND>(parent->winId());
    const auto rootWindow = GetAncestor(parentWindow, GA_ROOT);
    RECT parentRect;
    if (!IsWindowVisible(parentWindow) || IsIconic(rootWindow) ||
        !GetWindowRect(parentWindow, &parentRect)) {
        hideNativeWindow();
        return;
    }

    const auto dpr = std::max<qreal>(1.0, parent->devicePixelRatioF());
    const auto lineStart = parentRect.left + m_position * dpr;
    const auto lineEnd = lineStart + dpr;
    const auto surfaceWidth = std::max<LONG>(2, static_cast<LONG>(std::ceil(dpr)) + 1);
    const auto left = std::max(parentRect.left, static_cast<LONG>(std::floor(lineStart)));
    const auto right = std::min(parentRect.right, left + surfaceWidth);
    if (right <= left || parentRect.bottom <= parentRect.top) {
        hideNativeWindow();
        return;
    }

    const auto width = right - left;
    const auto height = parentRect.bottom - parentRect.top;
    if (!m_nativeWindow->ensureBitmap(width, height)) {
        hideNativeWindow();
        return;
    }

    auto *pixels = static_cast<std::uint32_t *>(m_nativeWindow->bits);
    const auto &color = m_nativeWindow->color;
    for (int column = 0; column < width; ++column) {
        const auto pixelStart = static_cast<qreal>(left + column);
        const auto coverage = std::clamp(
            std::min(lineEnd, pixelStart + 1.0) - std::max(lineStart, pixelStart), 0.0, 1.0);
        const auto alpha = qRound(color.alpha() * coverage);
        const auto red = color.red() * alpha / 255;
        const auto green = color.green() * alpha / 255;
        const auto blue = color.blue() * alpha / 255;
        const auto pixel =
            static_cast<std::uint32_t>((alpha << 24) | (red << 16) | (green << 8) | blue);
        for (int row = 0; row < height; ++row)
            pixels[row * width + column] = pixel;
    }

    POINT destination = {left, parentRect.top};
    POINT source = {0, 0};
    SIZE size = {width, height};
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    // The layered popup supplies DWM with a complete alpha surface for every subpixel step.
    if (!UpdateLayeredWindow(m_nativeWindow->handle, nullptr, &destination, &size,
                             m_nativeWindow->memoryDc, &source, 0, &blend, ULW_ALPHA)) {
        hideNativeWindow();
        return;
    }
    if (!IsWindowVisible(m_nativeWindow->handle)) {
        SetWindowPos(m_nativeWindow->handle, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
    }
#endif
}

#ifdef Q_OS_WIN
void PlaybackIndicatorOverlay::ensureNativeWindow() {
    if (m_nativeWindow && m_nativeWindow->handle)
        return;
    const auto parent = parentWidget();
    if (!parent || !ensureNativeIndicatorClass())
        return;

    auto state = std::make_unique<NativePlaybackIndicatorWindow>();
    state->color = m_color;
    const auto parentWindow = reinterpret_cast<HWND>(parent->winId());
    const auto ownerWindow = GetAncestor(parentWindow, GA_ROOT);
    state->handle =
        CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
                        nativeIndicatorClassName, L"", WS_POPUP, 0, 0, 1, 1, ownerWindow, nullptr,
                        GetModuleHandleW(nullptr), nullptr);
    if (state->handle)
        m_nativeWindow = std::move(state);
}

void PlaybackIndicatorOverlay::hideNativeWindow() const {
    if (m_nativeWindow && m_nativeWindow->handle)
        ShowWindow(m_nativeWindow->handle, SW_HIDE);
}
#endif
