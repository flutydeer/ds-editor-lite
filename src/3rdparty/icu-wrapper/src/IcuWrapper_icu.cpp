#include "IcuWrapper_p.h"

#ifdef Q_OS_WIN
#  include <icu.h>
#else
#  include <unicode/uloc.h>
#  include <unicode/utypes.h>
#endif

#include <array>
#include <vector>

namespace IcuWrapperPrivate {
namespace {

QByteArray toIcuLocaleId(const QString &tag)
{
    const QByteArray utf8 = normalizeLanguageTag(tag).toUtf8();
    if (utf8.isEmpty())
        return {};

    std::array<char, ULOC_FULLNAME_CAPACITY> localeId{};
    int32_t parsedLength = 0;
    UErrorCode status = U_ZERO_ERROR;
    uloc_forLanguageTag(utf8.constData(), localeId.data(), localeId.size(),
                        &parsedLength, &status);
    if (U_FAILURE(status) || parsedLength != utf8.size())
        return {};
    return QByteArray(localeId.data());
}

} // namespace

QString platformBestMatch(const QString &requested,
                          const QStringList &available)
{
    const QByteArray requestedId = toIcuLocaleId(requested);
    if (requestedId.isEmpty())
        return {};

    std::vector<QByteArray> localeIds;
    std::vector<qsizetype> originalIndexes;
    localeIds.reserve(available.size());
    originalIndexes.reserve(available.size());
    for (qsizetype i = 0; i < available.size(); ++i) {
        const QByteArray id = toIcuLocaleId(available.at(i));
        if (!id.isEmpty()) {
            localeIds.push_back(id);
            originalIndexes.push_back(i);
        }
    }
    if (localeIds.empty())
        return {};

    // Build the explicit fallback chain of the requested locale
    // (zh_Hans_CN -> zh_Hans -> zh) and match by exact canonical-ID
    // comparison against this chain. uloc_acceptLanguage is NOT usable as
    // the matcher: since ICU 67 it delegates to the distance-based
    // LocaleMatcher, whose likely-subtag maximization makes script/region
    // siblings coincide (zh_TW and zh_Hant both maximize to zh_Hant_TW),
    // so they cross-match on Linux (and it never fell back across
    // script+region combos on the Windows SDK umbrella ICU 64.2 either).
    // A manual chain + exact comparison keeps behavior deterministic and
    // uniform across platforms and ICU versions.
    std::vector<QByteArray> acceptChain;
    acceptChain.push_back(requestedId);
    {
        QByteArray current = requestedId;
        std::array<char, ULOC_FULLNAME_CAPACITY> parent{};
        for (;;) {
            UErrorCode parentStatus = U_ZERO_ERROR;
            const int32_t parentLength =
                uloc_getParent(current.constData(), parent.data(), parent.size(), &parentStatus);
            if (U_FAILURE(parentStatus) || parentLength <= 0 ||
                parentLength >= static_cast<int32_t>(parent.size()))
                break;
            parent[parentLength] = '\0';
            const QByteArray next(parent.data(), parentLength);
            // Defensive: stop if an ICU build ever returns the ID unchanged
            // (a non-shortening parent would otherwise spin forever).
            if (next == current)
                break;
            current = next;
            acceptChain.push_back(current);
        }
    }

    // Pick the most specific chain entry that exactly matches an available
    // canonical ID; ties keep the original available order.
    for (const QByteArray &chainItem : acceptChain) {
        for (size_t i = 0; i < localeIds.size(); ++i) {
            if (localeIds[i] == chainItem)
                return available.at(originalIndexes[i]);
        }
    }
    return {};
}

} // namespace IcuWrapperPrivate
