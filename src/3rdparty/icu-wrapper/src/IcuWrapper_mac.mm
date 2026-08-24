#include "IcuWrapper_p.h"

#import <Foundation/Foundation.h>

namespace IcuWrapperPrivate {

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
        if (matches.count == 0)
            return {};

        NSString *requestedLanguage =
            [[NSLocale localeWithLocaleIdentifier:preference]
                objectForKey:NSLocaleLanguageCode];
        NSString *matchedLanguage =
            [[NSLocale localeWithLocaleIdentifier:matches.firstObject]
                objectForKey:NSLocaleLanguageCode];
        if (requestedLanguage.length == 0 || matchedLanguage.length == 0 ||
            ![requestedLanguage isEqualToString:matchedLanguage])
            return {};

        const NSUInteger index = [supported indexOfObject:matches.firstObject];
        if (index == NSNotFound)
            return {};
        return available.at(indexes[index].longLongValue);
    }
}

} // namespace IcuWrapperPrivate
