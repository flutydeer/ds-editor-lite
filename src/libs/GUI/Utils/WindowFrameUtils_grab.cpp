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

namespace {
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
            if (QGuiApplication::topLevelAt(point) != backdrop.windowHandle()) {
                reason = QStringLiteral("capture backdrop is obscured or incorrectly stacked");
                return {};
            }
        }
        if (QGuiApplication::topLevelAt(center) != target->windowHandle()) {
            reason = QStringLiteral("target window is obscured");
            return {};
        }

        // QScreen root coordinates are screen-relative logical pixels on both
        // Windows and XCB. Qt supplies the matching device-pixel-ratio image.
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
