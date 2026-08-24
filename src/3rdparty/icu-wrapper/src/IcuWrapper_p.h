#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

namespace IcuWrapperPrivate {

QString normalizeLanguageTag(const QString &value);
QString platformBestMatch(const QString &requested,
                          const QStringList &available);

} // namespace IcuWrapperPrivate
