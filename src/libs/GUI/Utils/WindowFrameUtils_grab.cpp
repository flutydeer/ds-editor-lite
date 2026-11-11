#include <lite/GUI/Utils/WindowFrameUtils.h>

// Windows and X11 share the isolated-CI desktop capture path. macOS has its own
// target-window-only ScreenCaptureKit implementation and never enters this path.
#if !defined(Q_OS_MAC)

#  include <QWidget>
#  include <QApplication>
#  include <QScreen>
#  include <QWindow>
#  include <QPainter>
#  include <QPointer>
#  include <QEventLoop>
#  include <QTimer>
#  include <QThread>
#  include <QScopeGuard>
#  include <QImage>
#  include <QLibrary>

#  if defined(Q_OS_WIN)
#    include <Windows.h>
#    include <dwmapi.h>
#    include <cstring>
#  endif

#  if defined(Q_OS_LINUX) && __has_include(<X11/Xlib.h>)
#    include <X11/Xlib.h>
#    define LITE_HAS_X11_CAPTURE 1
#  endif

namespace {
#  if defined(Q_OS_WIN)
    QPixmap capturePrintedWindow(QWidget *widget, QString &reason) {
        if (!widget->isWindow() || !widget->isVisible() || widget->isMinimized()) {
            reason = QStringLiteral("PrintWindow requires a visible top-level window");
            return {};
        }
        const auto hwnd = reinterpret_cast<HWND>(widget->winId());
        DWORD processId = 0;
        const auto threadId = GetWindowThreadProcessId(hwnd, &processId);
        if (processId != GetCurrentProcessId() || threadId != GetCurrentThreadId()) {
            reason = QStringLiteral("PrintWindow target is not owned by this GUI thread");
            return {};
        }
        // GetWindowRect can be DPI-virtualized, whereas DWM frame bounds are
        // physical pixels. Do not combine them unless this thread is PM-aware.
        if (GetAwarenessFromDpiAwarenessContext(GetThreadDpiAwarenessContext()) !=
            DPI_AWARENESS_PER_MONITOR_AWARE) {
            reason = QStringLiteral("PrintWindow requires per-monitor DPI awareness");
            return {};
        }
        RECT frame{};
        RECT client{};
        POINT clientOrigin{};
        if (!GetWindowRect(hwnd, &frame) || !GetClientRect(hwnd, &client) ||
            !ClientToScreen(hwnd, &clientOrigin)) {
            reason = QStringLiteral("cannot query native PrintWindow geometry");
            return {};
        }
        const int width = frame.right - frame.left;
        const int height = frame.bottom - frame.top;
        if (width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
            static_cast<qint64>(width) * height > 64 * 1024 * 1024) {
            reason = QStringLiteral("PrintWindow dimensions are invalid or too large");
            return {};
        }
        // WM_PRINT may report success while returning stale or clipped Qt
        // backing-store content after a resize. Render the complete Qt client
        // independently, including any app-drawn title bar and close button.
        const QPointer<QWidget> target(widget);
        auto qtClient = widget->grab().toImage();
        const auto geometryMatches = [&] {
            RECT currentFrame{}, currentClient{};
            POINT currentOrigin{};
            return target && target->internalWinId() == reinterpret_cast<WId>(hwnd) &&
                   GetWindowRect(hwnd, &currentFrame) && GetClientRect(hwnd, &currentClient) &&
                   ClientToScreen(hwnd, &currentOrigin) && EqualRect(&frame, &currentFrame) &&
                   EqualRect(&client, &currentClient) && currentOrigin.x == clientOrigin.x &&
                   currentOrigin.y == clientOrigin.y;
        };
        if (qtClient.isNull() ||
            qtClient.size() != QSize(client.right - client.left, client.bottom - client.top) ||
            !geometryMatches()) {
            reason = QStringLiteral("PrintWindow/Qt client dimensions or native geometry changed");
            return {};
        }
        // An opaque native DIB cannot accurately preserve a translucent Qt
        // client. Refuse that case instead of inventing its desktop background.
        if (qtClient.hasAlphaChannel()) {
            for (int y = 0; y < qtClient.height(); ++y) {
                for (int x = 0; x < qtClient.width(); ++x) {
                    if (qAlpha(qtClient.pixel(x, y)) != 255) {
                        reason =
                            QStringLiteral("PrintWindow/Qt composition requires an opaque client");
                        return {};
                    }
                }
            }
        }
        qtClient = qtClient.convertToFormat(QImage::Format_RGB32);
        HDC windowDc = GetDC(hwnd);
        if (!windowDc) {
            reason = QStringLiteral("cannot acquire window drawing context");
            return {};
        }
        const auto releaseDc = qScopeGuard([&] { ReleaseDC(hwnd, windowDc); });
        HDC memoryDc = CreateCompatibleDC(windowDc);
        if (!memoryDc) {
            reason = QStringLiteral("cannot allocate PrintWindow drawing context");
            return {};
        }
        const auto deleteDc = qScopeGuard([&] { DeleteDC(memoryDc); });
        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = width;
        bitmapInfo.bmiHeader.biHeight = -height;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biBitCount = 32;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        void *bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(windowDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap || !bits) {
            if (bitmap)
                DeleteObject(bitmap);
            reason = QStringLiteral("cannot allocate PrintWindow bitmap");
            return {};
        }
        const auto deleteBitmap = qScopeGuard([&] { DeleteObject(bitmap); });
        HGDIOBJ previous = SelectObject(memoryDc, bitmap);
        if (!previous || previous == HGDI_ERROR) {
            reason = QStringLiteral("cannot select PrintWindow bitmap");
            return {};
        }
        const auto restoreBitmap = qScopeGuard([&] { SelectObject(memoryDc, previous); });
        QImage buffer(static_cast<uchar *>(bits), width, height, width * 4, QImage::Format_RGB32);
        constexpr QRgb unpainted = qRgb(255, 0, 255);
        buffer.fill(unpainted);
        // Same-process, same-GUI-thread only: WM_PRINT can dispatch directly.
        // PrintWindow is synchronous; do not move it to a worker and then block
        // the GUI thread waiting for that worker (which would deadlock WM_PRINT).
        constexpr UINT renderFullContent = 0x00000002;
        if (!PrintWindow(hwnd, memoryDc, renderFullContent)) {
            reason = QStringLiteral("PrintWindow failed");
            return {};
        }
        GdiFlush();
        if (!geometryMatches()) {
            reason = QStringLiteral("native geometry changed while PrintWindow was rendering");
            return {};
        }
        const QRect clientPixels(clientOrigin.x - frame.left, clientOrigin.y - frame.top,
                                 client.right - client.left, client.bottom - client.top);
        if (!buffer.rect().contains(clientPixels) || clientPixels.isEmpty()) {
            reason = QStringLiteral("PrintWindow did not contain the complete client rectangle");
            return {};
        }
        // Copy actual Qt pixels 1:1 only inside the Win32 client rectangle.
        // Outside it, keep exclusively the native WM_PRINT output. Do not scale
        // the client, redraw a system caption, or fabricate frame effects.
        for (int y = 0; y < qtClient.height(); ++y) {
            std::memcpy(buffer.scanLine(clientPixels.y() + y) + clientPixels.x() * 4,
                        qtClient.constScanLine(y), static_cast<size_t>(qtClient.width()) * 4);
        }
        int black = 0, missing = 0, count = 0;
        for (int y = clientPixels.top(); y <= clientPixels.bottom();
             y += qMax(1, clientPixels.height() / 64)) {
            for (int x = clientPixels.left(); x <= clientPixels.right();
                 x += qMax(1, clientPixels.width() / 64)) {
                const auto pixel = buffer.pixel(x, y);
                black += qRed(pixel) <= 2 && qGreen(pixel) <= 2 && qBlue(pixel) <= 2;
                missing += (pixel & 0x00ffffff) == (unpainted & 0x00ffffff);
                ++count;
            }
        }
        if (black * 100 >= count * 99 || missing) {
            reason = QStringLiteral("PrintWindow rejected: black or unpainted client pixels");
            return {};
        }
        QRect crop = buffer.rect();
        RECT visible{};
        if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &visible,
                                            sizeof(visible)))) {
            const QRect visiblePixels(visible.left - frame.left, visible.top - frame.top,
                                      visible.right - visible.left, visible.bottom - visible.top);
            if (!visiblePixels.isEmpty() && crop.contains(visiblePixels) &&
                visiblePixels.contains(clientPixels))
                crop = visiblePixels;
        }
        // A genuine native caption lies above the Win32 client. Its wide black
        // stripe cannot be repaired with a Qt grab. Reject that failure while
        // leaving ordinary one-pixel dark borders alone. App-drawn captions are
        // already inside clientPixels and were captured by QWidget above.
        const int captionTop = crop.top() + 2;
        const int captionBottom = clientPixels.top() - 3;
        if (captionBottom - captionTop + 1 >= 8) {
            int blackRun = 0;
            for (int x = crop.left() + 2; x <= crop.right() - 2; ++x) {
                int blackPixels = 0;
                for (int y = captionTop; y <= captionBottom; ++y) {
                    const auto pixel = buffer.pixel(x, y);
                    blackPixels += qRed(pixel) <= 2 && qGreen(pixel) <= 2 && qBlue(pixel) <= 2;
                }
                blackRun =
                    blackPixels * 100 >= (captionBottom - captionTop + 1) * 90 ? blackRun + 1 : 0;
                if (blackRun >= qMax(32, crop.width() / 8)) {
                    reason =
                        QStringLiteral("PrintWindow rejected: incomplete black native caption");
                    return {};
                }
            }
        }
        auto image = buffer.copy(crop);
        // The native frame must have painted too. Do not export the diagnostic
        // initialization color as a fabricated border when WM_PRINT omits it.
        for (int y = 0; y < image.height(); ++y) {
            const auto *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                if ((line[x] & 0x00ffffff) == (unpainted & 0x00ffffff)) {
                    reason = QStringLiteral(
                        "PrintWindow rejected: native frame contains unpainted pixels");
                    return {};
                }
            }
        }
        image.setDevicePixelRatio(widget->devicePixelRatioF());
        qInfo("PrintWindow + Qt capture: native frame %dx%d, client (%d,%d %dx%d), "
              "Qt client %dx%d; no DWM shadow/corner guarantee",
              width, height, clientPixels.x(), clientPixels.y(), clientPixels.width(),
              clientPixels.height(), qtClient.width(), qtClient.height());
        return QPixmap::fromImage(image);
    }
#  endif

#  if defined(LITE_HAS_X11_CAPTURE)
    QPixmap captureX11WithoutCompositor(QWidget *widget, QRect captureRect, QString &reason) {
        // This deliberately captures the real Openbox frame without shadows.
        // Do not read redirected root contents or retain an overlay fallback.
        if (QGuiApplication::screens().size() != 1 ||
            widget->screen()->geometry().topLeft() != QPoint(0, 0)) {
            reason = QStringLiteral("X11 capture requires one screen at origin (0,0)");
            return {};
        }
        QLibrary xlib(QStringLiteral("X11"), 6);
        const auto openDisplay =
            reinterpret_cast<decltype(&XOpenDisplay)>(xlib.resolve("XOpenDisplay"));
        const auto closeDisplay =
            reinterpret_cast<decltype(&XCloseDisplay)>(xlib.resolve("XCloseDisplay"));
        const auto attributes =
            reinterpret_cast<decltype(&XGetWindowAttributes)>(xlib.resolve("XGetWindowAttributes"));
        const auto screenNumber = reinterpret_cast<decltype(&XScreenNumberOfScreen)>(
            xlib.resolve("XScreenNumberOfScreen"));
        const auto internAtom =
            reinterpret_cast<decltype(&XInternAtom)>(xlib.resolve("XInternAtom"));
        const auto selectionOwner =
            reinterpret_cast<decltype(&XGetSelectionOwner)>(xlib.resolve("XGetSelectionOwner"));
        const auto getProperty =
            reinterpret_cast<decltype(&XGetWindowProperty)>(xlib.resolve("XGetWindowProperty"));
        const auto translateCoordinates = reinterpret_cast<decltype(&XTranslateCoordinates)>(
            xlib.resolve("XTranslateCoordinates"));
        const auto freeProperty = reinterpret_cast<decltype(&XFree)>(xlib.resolve("XFree"));
        if (!openDisplay || !closeDisplay || !attributes || !screenNumber || !internAtom ||
            !selectionOwner || !getProperty || !translateCoordinates || !freeProperty) {
            reason = QStringLiteral("cannot query X11 compositor ownership and frame geometry");
            return {};
        }
        Display *display = openDisplay(nullptr);
        if (!display) {
            reason = QStringLiteral("cannot open the CI X11 display");
            return {};
        }
        const auto close = qScopeGuard([&] { closeDisplay(display); });
        XWindowAttributes info{};
        if (!attributes(display, static_cast<Window>(widget->winId()), &info)) {
            reason = QStringLiteral("cannot query the X11 target window");
            return {};
        }
        const auto selection =
            QByteArray("_NET_WM_CM_S") + QByteArray::number(screenNumber(info.screen));
        const auto atom = internAtom(display, selection.constData(), True);
        if (atom && selectionOwner(display, atom)) {
            reason = QStringLiteral("X11 frame capture requires a desktop without a compositor");
            return {};
        }
        QPointer<QWidget> target(widget);
        const auto nativeId = widget->internalWinId();
        const auto rootId = info.root;
        QRect previousClient(widget->mapToGlobal(QPoint()), widget->size());
        widget->raise();
        int stableObservations = 0;
        // Openbox may finalize decorations or reposition a newly raised dialog.
        // Track its real geometry instead of rejecting that first adjustment.
        // Require two stable intervals, with a total wait bounded to 750 ms.
        for (int attempt = 0; attempt < 3 && stableObservations < 2; ++attempt) {
            QEventLoop settle;
            QTimer::singleShot(250, &settle, &QEventLoop::quit);
            settle.exec(QEventLoop::ExcludeUserInputEvents);
            if (!target || !target->isVisible() || target->isMinimized() ||
                target->internalWinId() != nativeId) {
                reason = QStringLiteral("X11 capture target disappeared or changed native window");
                return {};
            }
            const auto currentFrame = target->frameGeometry();
            const QRect currentClient(target->mapToGlobal(QPoint()), target->size());
            if (currentFrame == captureRect && currentClient == previousClient)
                ++stableObservations;
            else
                stableObservations = 0;
            captureRect = currentFrame;
            previousClient = currentClient;
        }
        if (stableObservations < 2) {
            reason = QStringLiteral("X11 capture target geometry did not stabilize within 750 ms");
            return {};
        }
        auto *screen = target->screen();
        if (!screen || QGuiApplication::screens().size() != 1 ||
            screen->geometry().topLeft() != QPoint(0, 0) || captureRect.isEmpty() ||
            !screen->geometry().contains(captureRect)) {
            reason = QStringLiteral("stable X11 frame is outside the supported single screen");
            return {};
        }
        // Re-query after stabilization: the original native dimensions may have
        // changed while the WM applied its final frame and client geometry.
        const auto scale = screen->devicePixelRatio();
        XWindowAttributes root{};
        if (!attributes(display, static_cast<Window>(nativeId), &info) || info.root != rootId ||
            !attributes(display, rootId, &root) ||
            root.width != qRound(screen->geometry().width() * scale) ||
            root.height != qRound(screen->geometry().height() * scale) ||
            info.width != qRound(target->width() * scale) ||
            info.height != qRound(target->height() * scale)) {
            reason = QStringLiteral("stable X11 target/screen dimensions do not match Qt scaling");
            return {};
        }
        // QWidget::frameGeometry can briefly expose only the client even after
        // Openbox has created decorations. Read the WM's physical frame extents
        // and the actual X11 client origin instead of trusting that cached rect.
        const auto nativeFrame = [&]() -> QRect {
            XWindowAttributes nativeInfo{};
            if (!attributes(display, static_cast<Window>(nativeId), &nativeInfo) ||
                nativeInfo.root != rootId || nativeInfo.width != qRound(target->width() * scale) ||
                nativeInfo.height != qRound(target->height() * scale))
                return {};
            const auto property = internAtom(display, "_NET_FRAME_EXTENTS", True);
            const auto cardinal = internAtom(display, "CARDINAL", True);
            if (!property || !cardinal)
                return {};
            Atom actualType{};
            int format = 0;
            unsigned long length = 0, remaining = 0;
            unsigned char *bytes = nullptr;
            const auto release = qScopeGuard([&] {
                if (bytes)
                    freeProperty(bytes);
            });
            if (getProperty(display, static_cast<Window>(nativeId), property, 0, 4, False, cardinal,
                            &actualType, &format, &length, &remaining, &bytes) != Success ||
                actualType != cardinal || format != 32 || length != 4 || remaining || !bytes)
                return {};
            const auto *extents = reinterpret_cast<const unsigned long *>(bytes);
            for (int i = 0; i < 4; ++i) {
                if (extents[i] > 256)
                    return {};
            }
            int x = 0, y = 0;
            Window child{};
            if (!translateCoordinates(display, static_cast<Window>(nativeId), rootId, 0, 0, &x, &y,
                                      &child))
                return {};
            const QRect actualClient(qRound(x / scale), qRound(y / scale), target->width(),
                                     target->height());
            if (actualClient != QRect(target->mapToGlobal(QPoint()), target->size()))
                return {};
            return actualClient.adjusted(-qRound(extents[0] / scale), -qRound(extents[2] / scale),
                                         qRound(extents[1] / scale), qRound(extents[3] / scale));
        };
        captureRect = nativeFrame();
        if (captureRect.isEmpty() || !screen->geometry().contains(captureRect)) {
            reason =
                QStringLiteral("X11 native frame extents are unavailable or outside the screen");
            return {};
        }
        // Probe the target client, not WM-owned decorations, for occlusion.
        // In the isolated CI display this also detects raised fixture windows.
        const QRect clientRect(target->mapToGlobal(QPoint()), target->size());
        const QPoint center = clientRect.center();
        const QPoint probes[] = {
            clientRect.topLeft(),
            clientRect.topRight(),
            clientRect.bottomLeft(),
            clientRect.bottomRight(),
            center,
            {center.x(),         clientRect.top()   },
            {center.x(),         clientRect.bottom()},
            {clientRect.left(),  center.y()         },
            {clientRect.right(), center.y()         }
        };
        for (const auto &point : probes) {
            if (QGuiApplication::topLevelAt(point) != target->windowHandle()) {
                reason = QStringLiteral("X11 target client is obscured at (%1,%2)")
                             .arg(point.x())
                             .arg(point.y());
                return {};
            }
        }
        const auto currentAtom = internAtom(display, selection.constData(), True);
        if (currentAtom && selectionOwner(display, currentAtom)) {
            reason = QStringLiteral("X11 compositor started during capture setup");
            return {};
        }
        const QPixmap capture = target->screen()->grabWindow(
            0, captureRect.x(), captureRect.y(), captureRect.width(), captureRect.height());
        if (!target || nativeFrame() != captureRect) {
            reason = QStringLiteral("X11 native frame geometry changed during capture");
            return {};
        }
        const auto image = capture.toImage();
        const auto offset = clientRect.topLeft() - captureRect.topLeft();
        const QRect client(qRound(offset.x() * scale), qRound(offset.y() * scale),
                           qRound(target->width() * scale), qRound(target->height() * scale));
        if (image.isNull() || !image.rect().contains(client) || client.isEmpty()) {
            reason = QStringLiteral("X11 frame capture returned no complete client image");
            return {};
        }
        // Retain the regression guard for the observed black-window failure,
        // excluding all window-manager decorations from the sampled rectangle.
        int black = 0, count = 0;
        for (int row = client.top(); row <= client.bottom(); row += qMax(1, client.height() / 64)) {
            for (int column = client.left(); column <= client.right();
                 column += qMax(1, client.width() / 64)) {
                const auto pixel = image.pixel(column, row);
                black += qRed(pixel) <= 2 && qGreen(pixel) <= 2 && qBlue(pixel) <= 2;
                ++count;
            }
        }
        if (black * 100 >= count * 99) {
            reason = QStringLiteral("X11 frame capture rejected: at least 99% of sampled client "
                                    "pixels are black (RGB <= 2)");
            return {};
        }
        qInfo("X11 frame capture: native frame %dx%d, client %dx%d; no compositor or synthetic "
              "shadows/corners",
              captureRect.width(), captureRect.height(), target->width(), target->height());
        return capture;
    }
#  endif

    class CaptureBackdrop final : public QWidget {
    public:
        CaptureBackdrop()
            : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus |
                                   Qt::NoDropShadowWindowHint),
              m_color(qEnvironmentVariable("LITE_CAPTURE_THEME") == QStringLiteral("light")
                          ? QColor(244, 244, 244)
                          : QColor(30, 30, 30)) {
            setAttribute(Qt::WA_ShowWithoutActivating);
            setAttribute(Qt::WA_OpaquePaintEvent);
            setFocusPolicy(Qt::NoFocus);
        }

    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.fillRect(rect(), m_color);
        }

    private:
        QColor m_color;
    };

    QPixmap captureDesktopWindow(QWidget *widget, QString &reason) {
        // Root capture can contain overlapping windows. It is deliberately
        // opt-in for the isolated screenshot CI process, never a local default.
        if (qEnvironmentVariable("LITE_SCREENSHOT_DESKTOP_CAPTURE") != QStringLiteral("1")) {
            reason = QStringLiteral("isolated desktop capture is not enabled");
            return {};
        }
        const auto platform = QGuiApplication::platformName();
        if (platform != QStringLiteral("windows") && platform != QStringLiteral("xcb")) {
            reason =
                QStringLiteral("desktop capture requires Windows or X11, not %1").arg(platform);
            return {};
        }
        if (!widget->isWindow() || !widget->isVisible() || widget->isMinimized()) {
            reason = QStringLiteral("target is not a visible top-level window");
            return {};
        }
        auto *screen = widget->screen();
        const int shadowMargin = platform == QStringLiteral("xcb") ? 0 : 40;
        if (!screen) {
            reason = QStringLiteral("target has no screen");
            return {};
        }
        QPointer<QWidget> target(widget);
        const QPoint originalPosition = widget->pos();
        bool moved = false;
        const auto restorePosition = qScopeGuard([&] {
            if (moved && target)
                target->move(originalPosition);
        });
        QRect captureRect = widget->frameGeometry().adjusted(-shadowMargin, -shadowMargin,
                                                             shadowMargin, shadowMargin);
        const auto available = screen->availableGeometry();
        if (captureRect.width() > available.width() || captureRect.height() > available.height()) {
            reason = QStringLiteral("window with shadow margin (%1x%2) exceeds available screen "
                                    "(%3x%4)")
                         .arg(captureRect.width())
                         .arg(captureRect.height())
                         .arg(available.width())
                         .arg(available.height());
            return {};
        }
        if (!available.contains(captureRect)) {
            const auto delta = available.center() - captureRect.center();
            widget->move(originalPosition + delta);
            moved = true;
            captureRect = widget->frameGeometry().adjusted(-shadowMargin, -shadowMargin,
                                                           shadowMargin, shadowMargin);
        }
        if (!screen->geometry().contains(captureRect)) {
            reason = QStringLiteral("window manager did not place target within the screen");
            return {};
        }
        if (platform == QStringLiteral("xcb")) {
#  if defined(LITE_HAS_X11_CAPTURE)
            return captureX11WithoutCompositor(target, captureRect, reason);
#  else
            reason = QStringLiteral("built without X11 capture headers");
            return {};
#  endif
        }
        CaptureBackdrop backdrop;
        // Cover the complete capture rectangle so neither the host nor other
        // fixtures appear through the target's actual corner mask and shadow.
        // Avoid a screen-sized window: some WMs give fullscreen windows a
        // separate stacking layer above the dialog we want to capture.
        backdrop.setGeometry(captureRect);
        backdrop.show();
        backdrop.raise();
        widget->raise();
        // Do not activate either window: native dialog focus remains unchanged.
        // Allow the real window manager/compositor to finish painting, bounded
        // independently of screenshot service availability.
        QEventLoop settle;
        QTimer::singleShot(250, &settle, &QEventLoop::quit);
        settle.exec(QEventLoop::ExcludeUserInputEvents);
        if (!target || !target->isVisible() ||
            target->frameGeometry().adjusted(-shadowMargin, -shadowMargin, shadowMargin,
                                             shadowMargin) != captureRect) {
            reason = QStringLiteral("target disappeared or moved during capture setup");
            return {};
        }

        // Refuse an obviously obscured or badly stacked scene before reading
        // screen pixels. These checks supplement, not replace, CI isolation.
        const QPoint center = captureRect.center();
        const QPoint probes[] = {
            captureRect.topLeft(),
            captureRect.topRight(),
            captureRect.bottomLeft(),
            captureRect.bottomRight(),
            {center.x(),          captureRect.top()   },
            {center.x(),          captureRect.bottom()},
            {captureRect.left(),  center.y()          },
            {captureRect.right(), center.y()          }
        };
        for (const auto &point : probes) {
            auto *hit = QGuiApplication::topLevelAt(point);
            if (hit != backdrop.windowHandle()) {
                reason = QStringLiteral("capture backdrop is obscured or incorrectly stacked at "
                                        "(%1,%2): hit %3, backdrop geometry (%4,%5 %6x%7)")
                             .arg(point.x())
                             .arg(point.y())
                             .arg(hit ? hit->title() : QStringLiteral("non-Qt window"))
                             .arg(backdrop.x())
                             .arg(backdrop.y())
                             .arg(backdrop.width())
                             .arg(backdrop.height());
                return {};
            }
        }
        if (QGuiApplication::topLevelAt(center) != target->windowHandle()) {
            reason = QStringLiteral("target window is obscured");
            return {};
        }

        // Windows QScreen root coordinates are screen-relative logical pixels.
        // Qt supplies the matching device-pixel-ratio image.
        const QPoint origin = captureRect.topLeft() - screen->geometry().topLeft();
        const auto image = screen->grabWindow(0, origin.x(), origin.y(), captureRect.width(),
                                              captureRect.height());
        if (image.isNull())
            reason = QStringLiteral("desktop pixel capture returned no image");
        return image;
    }
}

bool WindowFrameUtils::captureWindowChrome(QWidget *widget, const QString &filePath,
                                           QString *captureMethod) {
    if (captureMethod)
        *captureMethod = QStringLiteral("failed");
    if (!widget || !qApp || QThread::currentThread() != qApp->thread()) {
        qWarning("Window capture requires a widget and the GUI thread");
        return false;
    }
    QString reason;
    const QPointer<QWidget> target(widget);
    const auto desktopImage = captureDesktopWindow(widget, reason);
    if (!desktopImage.isNull()) {
        const bool saved = desktopImage.save(filePath);
        const auto method = QGuiApplication::platformName() == QStringLiteral("windows")
                                ? QStringLiteral("windows-desktop-window")
                                : QStringLiteral("x11-window-without-compositor");
        if (saved && captureMethod)
            *captureMethod = method;
        qInfo("Window capture: %s (actual desktop pixels, available system effects only)",
              saved ? qPrintable(method) : "failed to write PNG");
        return saved;
    }
    qWarning("Window capture fallback: %s", qPrintable(reason));
#  if defined(Q_OS_WIN)
    if (target && QGuiApplication::platformName() == QStringLiteral("windows") &&
        qEnvironmentVariable("LITE_SCREENSHOT_DESKTOP_CAPTURE") == QStringLiteral("1")) {
        const auto printedImage = capturePrintedWindow(target, reason);
        if (!printedImage.isNull()) {
            const bool saved = printedImage.save(filePath);
            if (saved && captureMethod)
                *captureMethod = QStringLiteral("windows-printwindow-frame-qt-client");
            qInfo("Window capture: %s",
                  saved ? "windows-printwindow-frame-qt-client" : "failed to write PNG");
            return saved;
        }
        qWarning("PrintWindow capture fallback: %s", qPrintable(reason));
    }
#  endif
    const bool saved = target && target->grab().save(filePath);
    if (captureMethod)
        *captureMethod = saved ? QStringLiteral("qt-client-fallback") : QStringLiteral("failed");
    qInfo("Window capture: %s (Qt client area only)", saved ? "qt-client-fallback" : "failed");
    return saved;
}

#endif
