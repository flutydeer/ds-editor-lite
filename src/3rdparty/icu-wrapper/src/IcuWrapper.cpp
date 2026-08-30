#include <IcuWrapper/IcuWrapper.h>

#include "IcuWrapper_p.h"

namespace IcuWrapperPrivate {

QString normalizeLanguageTag(const QString &value)
{
    QString result = value.trimmed();
    const qsizetype dot = result.indexOf(u'.');
    if (dot >= 0)
        result.truncate(dot);
    const qsizetype at = result.indexOf(u'@');
    if (at >= 0)
        result.truncate(at);
    return result.replace(u'_', u'-');
}

} // namespace IcuWrapperPrivate

QString IcuWrapper::bestMatch(const QString &requested,
                              const QStringList &available)
{
    const QString normalizedRequested =
        IcuWrapperPrivate::normalizeLanguageTag(requested);
    if (normalizedRequested.isEmpty() || available.isEmpty())
        return {};

    return IcuWrapperPrivate::platformBestMatch(normalizedRequested, available);
}
