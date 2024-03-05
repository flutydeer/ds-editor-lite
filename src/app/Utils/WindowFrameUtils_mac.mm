#include "WindowFrameUtils.h"

#include <AppKit/AppKit.h>

#include <QWindow>
#include <QWidget>
#include <QStyle>

void WindowFrameUtils::applyFrameEffects(QWidget *widget) {
    auto frame = NSMakeRect(widget->x(), widget->y(), widget->width(), widget->height());
    auto view = reinterpret_cast<NSView *>(widget->winId());

    auto visualEffectView = [[NSVisualEffectView alloc] init];
    visualEffectView.autoresizingMask = NSViewWidthSizable|NSViewHeightSizable;
    visualEffectView.wantsLayer = YES;
    visualEffectView.frame = frame;
    visualEffectView.state = NSVisualEffectStateActive;
    visualEffectView.material = NSVisualEffectMaterialUltraDark;
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
}