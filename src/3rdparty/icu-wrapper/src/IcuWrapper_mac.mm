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

        NSString *preference = [NSString stringWithUTF8String:
            requested.toUtf8().constData()];
        NSArray<NSString *> *matches =
            [NSBundle preferredLocalizationsFromArray:supported
                                       forPreferences:@[ preference ]];

        // Foundation picks candidate ordering with a distance-based fallback
        // that may cross script/region siblings (zh-TW -> zh-Hant), so every
        // candidate must additionally be a member of the requested tag's
        // explicit parent chain — the contract shared with the ICU backends
        // (see docs/design/localization-display-name-design.md §4).
        NSArray<NSString *> *acceptChain =
            acceptChainForNormalizedTag(requested);
        for (NSString *match in matches) {
            if (![acceptChain containsObject:[match lowercaseString]])
                continue;
            const NSUInteger index = [supported indexOfObject:match];
            if (index == NSNotFound)
                continue;
            return available.at(indexes[index].longLongValue);
        }
        return {};
    }
}

} // namespace IcuWrapperPrivate
