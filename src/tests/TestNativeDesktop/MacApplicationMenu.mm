#include "MacApplicationMenu.h"

#include <QMenuBar>
#import <AppKit/AppKit.h>
#import <Carbon/Carbon.h>

namespace MacApplicationMenu {
    bool isCurrent(QMenuBar &bar) {
        return bar.isNativeMenuBar() && NSApp.mainMenu == bar.toNSMenu();
    }

    bool triggerCommand(char key) {
        unsigned short keyCode;
        switch (key) {
            case 'n': keyCode = kVK_ANSI_N; break;
            case 'o': keyCode = kVK_ANSI_O; break;
            case 'w': keyCode = kVK_ANSI_W; break;
            case 'q': keyCode = kVK_ANSI_Q; break;
            default: return false;
        }
        const unichar character = key;
        NSString *text = [NSString stringWithCharacters:&character length:1];
        NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint
                                    modifierFlags:NSEventModifierFlagCommand
                                        timestamp:NSProcessInfo.processInfo.systemUptime
                                     windowNumber:NSApp.keyWindow.windowNumber context:nil
                                       characters:text charactersIgnoringModifiers:text
                                        isARepeat:NO keyCode:keyCode];
        return [NSApp.mainMenu performKeyEquivalent:event];
    }

    void reopen(bool hasVisibleWindows) {
        [NSApp.delegate applicationShouldHandleReopen:NSApp hasVisibleWindows:hasVisibleWindows];
    }
}
