#include <lite/GUI/Utils/WindowFrameUtils.h>

#include <lite/GUI/Theme/ThemeManager.h>

#ifdef Q_OS_MAC
#  include <AppKit/AppKit.h>

#  include <QWindow>
#  include <QWidget>
#  include <QStyle>
#  include <QGuiApplication>
#  include <QFile>

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

bool WindowFrameUtils::captureWindowChrome(QWidget *widget, const QString &filePath) {
    if (!widget)
        return false;
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return widget->grab().save(filePath);

    // Caching the theme frame instead of the Qt content is the whole point: the
    // traffic lights and the title bar are AppKit views that live outside the
    // QWidget backing store. It also avoids screencapture(1), whose screen
    // recording consent prompt cannot be answered from a headless CI job.
    auto view = reinterpret_cast<NSView *>(widget->winId());
    NSWindow *window = [view window];
    if (!window) {
        qWarning("Chrome capture has no NSWindow");
        return false;
    }
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

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(static_cast<const char *>(png.bytes), static_cast<qint64>(png.length)) ==
           static_cast<qint64>(png.length);
}

#endif
