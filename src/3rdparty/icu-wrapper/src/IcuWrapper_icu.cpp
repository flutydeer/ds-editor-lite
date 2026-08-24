#include "IcuWrapper_p.h"

#ifdef Q_OS_WIN
#  include <icu.h>
#else
#  include <unicode/uenum.h>
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

    std::vector<const char *> localePointers;
    localePointers.reserve(localeIds.size());
    for (const QByteArray &id : localeIds)
        localePointers.push_back(id.constData());

    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *enumeration = uenum_openCharStringsEnumeration(
        localePointers.data(), localePointers.size(), &status);
    if (U_FAILURE(status) || enumeration == nullptr)
        return {};

    // Build the accept list as the explicit fallback chain of the requested
    // locale (zh_Hans_CN -> zh_Hans -> zh). The Windows SDK umbrella ICU
    // (runtime 64.2) does not fall back across script+region combinations
    // inside uloc_acceptLanguage, so the parents are enumerated manually.
    // Each chain item matches exactly, which keeps behavior uniform across
    // platforms and ICU versions.
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
    std::vector<const char *> acceptPointers;
    acceptPointers.reserve(acceptChain.size());
    for (const QByteArray &item : acceptChain)
        acceptPointers.push_back(item.constData());

    std::array<char, ULOC_FULLNAME_CAPACITY> result{};
    UAcceptResult acceptResult = ULOC_ACCEPT_FAILED;
    uloc_acceptLanguage(result.data(), result.size(), &acceptResult,
                        acceptPointers.data(), static_cast<int32_t>(acceptPointers.size()),
                        enumeration, &status);
    uenum_close(enumeration);
    if (U_FAILURE(status) || acceptResult == ULOC_ACCEPT_FAILED)
        return {};

    const QByteArray matchedId(result.data());
    for (size_t i = 0; i < localeIds.size(); ++i) {
        if (localeIds[i] == matchedId)
            return available.at(originalIndexes[i]);
    }
    return {};
}

} // namespace IcuWrapperPrivate
