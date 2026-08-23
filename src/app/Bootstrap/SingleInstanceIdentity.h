#ifndef SINGLEINSTANCEIDENTITY_H
#define SINGLEINSTANCEIDENTITY_H

#include <QString>

namespace SingleInstanceIdentity {
    QString productIdentity();
    QString defaultDataDirectory();
    QString normalizeDataDirectory(const QString &dataDirectory);
    QString serviceName(const QString &dataDirectory = {});
    QString lockFilePath(const QString &dataDirectory = {});
}

#endif // SINGLEINSTANCEIDENTITY_H
