//
// macOS recent documents integration
//

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <QString>

void addToMacRecentDocument(const QString &filePath) {
    @autoreleasepool {
        if (filePath.isEmpty())
            return;

        const QByteArray utf8Path = filePath.toUtf8();
        NSString *nsPath = [NSString stringWithUTF8String:utf8Path.constData()];
        if (!nsPath)
            return;

        NSURL *url = [NSURL fileURLWithPath:nsPath];
        if (url) {
            [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:url];
        }
    }
}
