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

#  if defined(Q_OS_LINUX) && __has_include(<X11/Xlib.h>)
#    include <X11/Xlib.h>
#    include <bit>
#    define LITE_HAS_X11_CAPTURE 1
#  endif

namespace {
#  if defined(LITE_HAS_X11_CAPTURE)
    QPixmap captureX11Output(QWidget *target, const QRect &captureRect, QString &reason) {
        // Read the compositor's final output, not Qt XCB's CopyArea from root
        // with IncludeInferiors. Redirected, different-depth child windows can
        // otherwise replace their composed contents with black rectangles.
        if (QGuiApplication::screens().size() != 1 ||
            target->screen()->geometry().topLeft() != QPoint(0, 0)) {
            reason = QStringLiteral("X11 compositor capture requires one screen at origin (0,0)");
            return {};
        }
        QLibrary xlib(QStringLiteral("X11"), 6);
        QLibrary composite(QStringLiteral("Xcomposite"), 1);
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
        const auto translate = reinterpret_cast<decltype(&XTranslateCoordinates)>(
            xlib.resolve("XTranslateCoordinates"));
        const auto getImage = reinterpret_cast<decltype(&XGetImage)>(xlib.resolve("XGetImage"));
        const auto getOverlay = reinterpret_cast<Window (*)(Display *, Window)>(
            composite.resolve("XCompositeGetOverlayWindow"));
        const auto releaseOverlay = reinterpret_cast<void (*)(Display *, Window)>(
            composite.resolve("XCompositeReleaseOverlayWindow"));
        if (!openDisplay || !closeDisplay || !attributes || !screenNumber || !internAtom ||
            !selectionOwner || !translate || !getImage || !getOverlay || !releaseOverlay) {
            reason = QStringLiteral("X11 compositor capture libraries are unavailable");
            return {};
        }
        Display *display = openDisplay(nullptr);
        if (!display) {
            reason = QStringLiteral("cannot open the CI X11 display");
            return {};
        }
        const auto close = qScopeGuard([&] { closeDisplay(display); });
        XWindowAttributes info{};
        const auto window = static_cast<Window>(target->winId());
        if (!attributes(display, window, &info)) {
            reason = QStringLiteral("cannot query the X11 target window");
            return {};
        }
        const auto selection =
            QByteArray("_NET_WM_CM_S") + QByteArray::number(screenNumber(info.screen));
        const auto atom = internAtom(display, selection.constData(), True);
        if (!atom || !selectionOwner(display, atom)) {
            reason = QStringLiteral("X11 screen has no active compositor");
            return {};
        }
        // Reference the already-running compositor's overlay. Never redirect
        // windows or change the overlay's input/visual shape or stacking.
        const Window overlay = getOverlay(display, info.root);
        if (!overlay) {
            reason = QStringLiteral("X11 compositor overlay is unavailable");
            return {};
        }
        const auto release = qScopeGuard([&] { releaseOverlay(display, info.root); });
        int clientX = 0, clientY = 0;
        Window child = 0;
        if (!translate(display, window, overlay, 0, 0, &clientX, &clientY, &child)) {
            reason = QStringLiteral("cannot locate target in the compositor output");
            return {};
        }
        const auto scale = target->screen()->devicePixelRatio();
        const auto offset = target->mapToGlobal(QPoint()) - captureRect.topLeft();
        const int x = clientX - qRound(offset.x() * scale);
        const int y = clientY - qRound(offset.y() * scale);
        const int width = qRound(captureRect.width() * scale);
        const int height = qRound(captureRect.height() * scale);
        XWindowAttributes outputInfo{};
        if (!attributes(display, overlay, &outputInfo) || x < 0 || y < 0 ||
            x + width > outputInfo.width || y + height > outputInfo.height ||
            outputInfo.width != qRound(target->screen()->geometry().width() * scale) ||
            outputInfo.height != qRound(target->screen()->geometry().height() * scale) ||
            info.width != qRound(target->width() * scale) ||
            info.height != qRound(target->height() * scale) ||
            clientX != qRound(target->mapToGlobal(QPoint()).x() * scale) ||
            clientY != qRound(target->mapToGlobal(QPoint()).y() * scale)) {
            reason = QStringLiteral(
                "X11 target/screen coordinates or scale do not match compositor output");
            return {};
        }
        XImage *pixels = getImage(display, overlay, x, y, width, height, AllPlanes, ZPixmap);
        if (!pixels) {
            reason = QStringLiteral("XGetImage returned no compositor output");
            return {};
        }
        const auto destroy = qScopeGuard([&] { pixels->f.destroy_image(pixels); });
        const auto channel = [](unsigned long pixel, unsigned long mask) -> int {
            if (!mask)
                return 0;
            const auto shift = std::countr_zero(mask);
            return static_cast<int>(((pixel & mask) >> shift) * 255 / (mask >> shift));
        };
        QImage image(width, height, QImage::Format_RGB32);
        if (image.isNull()) {
            reason = QStringLiteral("cannot allocate compositor capture image");
            return {};
        }
        for (int row = 0; row < height; ++row) {
            auto *line = reinterpret_cast<QRgb *>(image.scanLine(row));
            for (int column = 0; column < width; ++column) {
                const auto pixel = pixels->f.get_pixel(pixels, column, row);
                line[column] =
                    qRgb(channel(pixel, pixels->red_mask), channel(pixel, pixels->green_mask),
                         channel(pixel, pixels->blue_mask));
            }
        }
        image.setDevicePixelRatio(scale);
        // Reject the observed redirected-window failure using only client
        // pixels, excluding the shadow, backdrop and window-manager frame.
        const QRect client(qRound(offset.x() * scale), qRound(offset.y() * scale),
                           qRound(target->width() * scale), qRound(target->height() * scale));
        if (!image.rect().contains(client) || client.isEmpty()) {
            reason = QStringLiteral("client pixels are outside compositor capture");
            return {};
        }
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
            reason = QStringLiteral("compositor capture rejected: at least 99% of sampled client "
                                    "pixels are black (RGB <= 2)");
            return {};
        }
        return QPixmap::fromImage(image);
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
        constexpr int shadowMargin = 40;
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
        CaptureBackdrop backdrop;
        QPointer<QWindow> nativeTarget = widget->windowHandle();
        if (!nativeTarget) {
            reason = QStringLiteral("target has no native window handle");
            return {};
        }
        const QPointer<QWindow> originalTransientParent = nativeTarget->transientParent();
        const bool insertBackdrop = platform == QStringLiteral("xcb");
        // Openbox raises a dialog's owner chain with the dialog. An unrelated
        // backdrop then ends up below the host, exposing the host around it.
        // Insert the backdrop between owner and dialog, never above the target.
        if (insertBackdrop) {
            backdrop.winId();
            backdrop.windowHandle()->setTransientParent(originalTransientParent);
            nativeTarget->setTransientParent(backdrop.windowHandle());
        }
        // Restore before destroying the backdrop, including every early return.
        const auto restoreTransientParent = qScopeGuard([&] {
            if (insertBackdrop && nativeTarget)
                nativeTarget->setTransientParent(originalTransientParent);
        });
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

        if (platform == QStringLiteral("xcb")) {
#  if defined(LITE_HAS_X11_CAPTURE)
            return captureX11Output(target, captureRect, reason);
#  else
            reason = QStringLiteral("built without X11 capture headers");
            return {};
#  endif
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
                                : QStringLiteral("x11-desktop-window");
        if (saved && captureMethod)
            *captureMethod = method;
        qInfo("Window capture: %s (actual desktop pixels, available system effects only)",
              saved ? qPrintable(method) : "failed to write PNG");
        return saved;
    }
    qWarning("Window capture fallback: %s", qPrintable(reason));
    const bool saved = target && target->grab().save(filePath);
    if (captureMethod)
        *captureMethod = saved ? QStringLiteral("qt-client-fallback") : QStringLiteral("failed");
    qInfo("Window capture: %s (Qt client area only)", saved ? "qt-client-fallback" : "failed");
    return saved;
}

#endif
