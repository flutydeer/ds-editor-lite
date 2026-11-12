#include <lite/GUI/Utils/WindowFrameUtils.h>

#include <lite/GUI/Theme/ThemeManager.h>

#ifdef Q_OS_MAC
#  include <AppKit/AppKit.h>
#  if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140400
#    include <ScreenCaptureKit/ScreenCaptureKit.h>
#  endif

#  include <QWindow>
#  include <QWidget>
#  include <QStyle>
#  include <QGuiApplication>
#  include <QFile>
#  include <QElapsedTimer>

#  include <atomic>
#  include <cmath>
#  include <memory>

namespace {
    bool writePng(const QByteArray &png, const QString &filePath) {
        QFile file(filePath);
        return !png.isEmpty() && file.open(QIODevice::WriteOnly) && file.write(png) == png.size();
    }

#  if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140400
    struct WindowCaptureState {
        std::atomic_bool finished{false};
        std::atomic_bool abandoned{false};
        QByteArray png;
        QString error;
        bool serviceFailure = false;
    };

    QByteArray captureCompositedWindow(NSWindow *window, QString &reason) {
        // A failed service must not add a timeout for every gallery item.
        static QString unavailableReason;
        if (!unavailableReason.isEmpty()) {
            reason = unavailableReason;
            return {};
        }
        if (@available(macOS 14.4, *)) {
            const auto state = std::make_shared<WindowCaptureState>();
            const auto windowId = static_cast<CGWindowID>(window.windowNumber);
            // Unlike global shareable-content enumeration, this API is limited
            // to our process and does not ask for Screen Recording permission.
            [SCShareableContent getCurrentProcessShareableContentWithCompletionHandler:^(
                                    SCShareableContent *content, NSError *error) {
                @autoreleasepool {
                    if (state->abandoned.load())
                        return;
                    SCWindow *target = nil;
                    for (SCWindow *candidate in content.windows) {
                        if (candidate.windowID == windowId) {
                            target = candidate;
                            break;
                        }
                    }
                    if (error || !target) {
                        // A missing transient window is local to that capture, not
                        // evidence that the service is unavailable for later windows.
                        state->serviceFailure = error != nil;
                        state->error =
                            error ? QString::fromUtf8(error.localizedDescription.UTF8String)
                                  : QStringLiteral("own window not available to ScreenCaptureKit");
                        state->finished.store(true, std::memory_order_release);
                        return;
                    }
                    auto filter = [[[SCContentFilter alloc] initWithDesktopIndependentWindow:target]
                        autorelease];
                    auto config = [[[SCStreamConfiguration alloc] init] autorelease];
                    // Leave space for the compositor's real shadow. This is
                    // capture padding, not a synthesized border or shadow.
                    const auto scale = filter.pointPixelScale;
                    constexpr double shadowPadding = 64.0;
                    config.width = static_cast<size_t>(
                        std::ceil((filter.contentRect.size.width + 2 * shadowPadding) * scale));
                    config.height = static_cast<size_t>(
                        std::ceil((filter.contentRect.size.height + 2 * shadowPadding) * scale));
                    config.captureResolution = SCCaptureResolutionBest;
                    config.scalesToFit = NO;
                    config.preservesAspectRatio = YES;
                    config.showsCursor = NO;
                    config.capturesAudio = NO;
                    config.includeChildWindows = NO;
                    config.ignoreShadowsSingleWindow = NO;
                    config.ignoreGlobalClipSingleWindow = YES;
                    config.shouldBeOpaque = NO;
                    config.backgroundColor = CGColorGetConstantColor(kCGColorClear);
                    [SCScreenshotManager
                        captureImageWithFilter:filter
                                 configuration:config
                             completionHandler:^(CGImageRef image, NSError *captureError) {
                                 @autoreleasepool {
                                     if (state->abandoned.load())
                                         return;
                                     if (image && !captureError) {
                                         auto rep = [[[NSBitmapImageRep alloc]
                                             initWithCGImage:image] autorelease];
                                         NSData *png =
                                             [rep representationUsingType:NSBitmapImageFileTypePNG
                                                               properties:@{}];
                                         if (png)
                                             state->png =
                                                 QByteArray(static_cast<const char *>(png.bytes),
                                                            static_cast<qsizetype>(png.length));
                                     }
                                     if (state->png.isEmpty())
                                         state->error =
                                             captureError
                                                 ? QString::fromUtf8(
                                                       captureError.localizedDescription.UTF8String)
                                                 : QStringLiteral(
                                                       "ScreenCaptureKit returned no image");
                                     state->finished.store(true, std::memory_order_release);
                                 }
                             }];
                }
            }];

            QElapsedTimer timer;
            timer.start();
            // Do not block the main dispatch queue: native callbacks and drawing
            // may need it. No QWidget or caller stack is captured by callbacks,
            // and a late callback never writes a screenshot after the timeout.
            while (!state->finished.load(std::memory_order_acquire) && timer.elapsed() < 3000) {
                @autoreleasepool {
                    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                                             beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
                }
            }
            if (!state->finished.load(std::memory_order_acquire)) {
                state->abandoned.store(true);
                reason = QStringLiteral("ScreenCaptureKit timed out after 3000 ms");
                unavailableReason = reason;
            } else {
                reason = state->error;
                if (!state->png.isEmpty())
                    return state->png;
                if (state->serviceFailure)
                    unavailableReason = reason;
            }
        } else {
            reason = QStringLiteral("own-process capture requires macOS 14.4 or newer");
        }
        return {};
    }
#  endif
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void WindowFrameUtils::applyFrameEffects(QWidget *widget) {
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;

    auto frame = NSMakeRect(widget->x(), widget->y(), widget->width(), widget->height());
    auto view = reinterpret_cast<NSView *>(widget->winId());

    auto visualEffectView = [[NSVisualEffectView alloc] init];
    visualEffectView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    visualEffectView.wantsLayer = YES;
    visualEffectView.frame = frame;
    visualEffectView.state = NSVisualEffectStateActive;
    const bool dark = ThemeManager::instance()->colorType() != ThemeManager::ThemeColorType::Light;
    visualEffectView.material =
        dark ? NSVisualEffectMaterialUltraDark : NSVisualEffectMaterialWindowBackground;
    visualEffectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    visualEffectView.wantsLayer = YES;

    auto containerWindow = QWindow::fromWinId(0);
    auto containerWidget = QWidget::createWindowContainer(containerWindow, widget);
    containerWidget->setAttribute(Qt::WA_NativeWindow);
    containerWidget->lower();

    auto window = [view window];
    auto content = [window contentView];
    [content addSubview:visualEffectView positioned:NSWindowBelow relativeTo:nullptr];

    window.titlebarAppearsTransparent = YES;
    window.appearance =
        [NSAppearance appearanceNamed:(dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua)];
}

void WindowFrameUtils::mergeTitleBarIntoContent(QWidget *widget) {
    if (!widget || QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return;

    auto view = reinterpret_cast<NSView *>(widget->winId());
    auto window = [view window];
    if (!window) {
        qInfo("Title bar merge deferred: the NSView has no NSWindow yet");
        return;
    }

    // QWindowKit also maintains FullSizeContentView when the NSView attaches
    // to its NSWindow. This is an idempotent native-state safeguard, not a
    // replacement for the QWidget safe-area policy.
    const auto before = window.styleMask;
    const auto contentBefore = window.contentLayoutRect;
    window.styleMask |= NSWindowStyleMaskFullSizeContentView;
    window.titlebarAppearsTransparent = YES;
    window.titleVisibility = NSWindowTitleHidden;
    qInfo(
        "Title bar merge applied: styleMask 0x%lx -> 0x%lx, content layout %.0fx%.0f -> %.0fx%.0f",
        static_cast<unsigned long>(before), static_cast<unsigned long>(window.styleMask),
        contentBefore.size.width, contentBefore.size.height, window.contentLayoutRect.size.width,
        window.contentLayoutRect.size.height);
}

bool WindowFrameUtils::captureWindowChrome(QWidget *widget, const QString &filePath,
                                           QString *captureMethod) {
    if (captureMethod)
        *captureMethod = QStringLiteral("failed");
    if (!widget)
        return false;
    if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
        const bool saved = widget->grab().save(filePath);
        if (saved && captureMethod)
            *captureMethod = QStringLiteral("qt-client-fallback");
        qInfo("Window capture: %s (Qt client area only)", saved ? "qt-client-fallback" : "failed");
        return saved;
    }
    if (![NSThread isMainThread]) {
        qWarning("Window capture requires the GUI thread");
        return false;
    }

    auto view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = [view window];
    if (!window) {
        qWarning("Chrome capture has no NSWindow");
        return false;
    }
    QString fallbackReason;
#  if __MAC_OS_X_VERSION_MAX_ALLOWED >= 140400
    const auto compositedPng = captureCompositedWindow(window, fallbackReason);
    if (!compositedPng.isEmpty()) {
        const bool saved = writePng(compositedPng, filePath);
        if (saved && captureMethod)
            *captureMethod = QStringLiteral("screencapturekit-window");
        qInfo("Window capture: %s", saved ? "screencapturekit-window" : "failed to write PNG");
        return saved;
    }
#  else
    fallbackReason = QStringLiteral("built without the macOS 14.4 capture API");
#  endif
    qWarning(
        "Window capture fallback: %s; AppKit frame cache has no compositor shadow/corner guarantee",
        qPrintable(fallbackReason));
    // AppKit includes native title-bar views, but cannot reproduce the window
    // server's shadows and corner mask. Never label this as a system capture.
    NSView *frameView = [[window contentView] superview];
    if (!frameView) {
        qWarning("Chrome capture has no AppKit frame view");
        return false;
    }

    const NSRect bounds = frameView.bounds;
    const NSRect clientRect = [view convertRect:view.bounds toView:frameView];
    const auto topInset = frameView.isFlipped ? NSMinY(clientRect) - NSMinY(bounds)
                                              : NSMaxY(bounds) - NSMaxY(clientRect);
    qInfo("Chrome native geometry: frame %.0fx%.0f, Qt view %.0fx%.0f, top inset %.0f, "
          "view safe-area top %.0f, screen safe-area top %.0f, styleMask 0x%lx",
          bounds.size.width, bounds.size.height, clientRect.size.width, clientRect.size.height,
          topInset, view.safeAreaInsets.top, window.screen.safeAreaInsets.top,
          static_cast<unsigned long>(window.styleMask));
    NSBitmapImageRep *rep = [frameView bitmapImageRepForCachingDisplayInRect:bounds];
    if (!rep) {
        qWarning("Chrome capture could not allocate an AppKit bitmap");
        return false;
    }
    [frameView cacheDisplayInRect:bounds toBitmapImageRep:rep];
    NSData *png = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if (!png)
        return false;

    const bool saved = writePng(
        QByteArray(static_cast<const char *>(png.bytes), static_cast<qsizetype>(png.length)),
        filePath);
    if (saved && captureMethod)
        *captureMethod = QStringLiteral("appkit-frame-fallback");
    qInfo("Window capture: %s", saved ? "appkit-frame-fallback" : "failed to write PNG");
    return saved;
}

#endif
