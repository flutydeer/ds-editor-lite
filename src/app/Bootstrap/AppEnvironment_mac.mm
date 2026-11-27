#include "AppEnvironment.h"
#include "UI/Window/DocumentWindowManager.h"

#import <AppKit/AppKit.h>

@interface DSEApplicationReopenDelegate : NSObject <NSApplicationDelegate>
@end

@implementation DSEApplicationReopenDelegate
- (BOOL)applicationShouldHandleReopen:(NSApplication *)application hasVisibleWindows:(BOOL)visible {
    auto *windows = DocumentWindowManager::instance();
    if (visible || !windows || !windows->keepsRunningWithoutWindows() || windows->active())
        return YES;

    // Check for a modal panel while handling the Dock click. It may close before Qt next runs.
    windows->activateApplication();
    return NO;
}
@end

void AppEnvironment::installMacReopenDelegate() {
    const auto platform = qgetenv("QT_QPA_PLATFORM");
    if (!platform.isEmpty() && !platform.startsWith("cocoa"))
        return;
    @autoreleasepool {
        NSApplication *application = [NSApplication sharedApplication];
        if (!application.delegate) {
            static auto *delegate = [DSEApplicationReopenDelegate new];
            application.delegate = delegate;
        }
    }
}
