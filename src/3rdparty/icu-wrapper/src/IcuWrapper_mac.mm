#include "IcuWrapper_p.h"

#import <Foundation/Foundation.h>

namespace IcuWrapperPrivate {
namespace {

// Explicit fallback chain of a normalized BCP 47 tag, built by plain
// last-subtag truncation (zh-Hans-CN -> [zh-hans-cn, zh-hans, zh],
// lowercased for case-insensitive comparison, which BCP 47 mandates).
// This mirrors the ICU backend's uloc_getParent chain semantics
// (structural only, no likely-subtag maximization), so script/region
// siblings (zh-TW vs zh-Hant, pt-BR vs pt-PT) never cross-match.
NSArray<NSString *> *acceptChainForNormalizedTag(const QString &tag) {
    NSMutableArray<NSString *> *chain = [NSMutableArray array];
    NSString *current = [NSString
        stringWithUTF8String:tag.toLower().toUtf8().constData()];
    while (current.length > 0) {
        [chain addObject:current];
        const NSRange dash =
            [current rangeOfString:@"-" options:NSBackwardsSearch];
        if (dash.location == NSNotFound)
            break;
        current = [current substringToIndex:dash.location];
    }
    return chain;
}

} // namespace

QString platformBestMatch(const QString &requested,
                          const QStringList &available)
{
    @autoreleasepool {
        NSMutableArray<NSString *> *supported =
            [NSMutableArray arrayWithCapacity:available.size()];
        NSMutableArray<NSNumber *> *indexes =
            [NSMutableArray arrayWithCapacity:available.size()];

        for (qsizetype i = 0; i < available.size(); ++i) {
            const QString normalized = normalizeLanguageTag(available.at(i));
            if (normalized.isEmpty())
                continue;
            [supported addObject:[NSString stringWithUTF8String:
                normalized.toUtf8().constData()]];
            [indexes addObject:@(i)];
        }
        if (supported.count == 0)
            return {};

        // Walk the explicit parent chain from most to least specific.
        // Exact case-insensitive matches keep the order in available.
        // Foundation's distance-based selector can return off-chain siblings
        // (zh-Hant for zh-TW) and omit the valid parent (zh).
        NSArray<NSString *> *acceptChain =
            acceptChainForNormalizedTag(requested);
        for (NSString *chainItem in acceptChain) {
            for (NSUInteger i = 0; i < supported.count; ++i) {
                if ([[supported[i] lowercaseString] isEqualToString:chainItem])
                    return available.at(indexes[i].longLongValue);
            }
        }
        return {};
    }
}

} // namespace IcuWrapperPrivate
