#include "SingleInstanceIdentity.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <lite/ProductMetadata.h>

namespace {
    constexpr auto instanceIdentity = "OpenVPI.DsEditorLite.v1";
    constexpr auto lockFileName = "instance-v1.lock";

    QString dataRoot() {
#ifdef Q_OS_WIN
        auto root = qEnvironmentVariable("APPDATA");
        if (!root.isEmpty())
            return root;
#endif
        return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    }
}

QString SingleInstanceIdentity::productIdentity() {
    return QString::fromLatin1(instanceIdentity);
}

QString SingleInstanceIdentity::defaultDataDirectory() {
    return QDir(dataRoot())
        .filePath(
            QStringLiteral("%1/%2").arg(QString::fromLatin1(LiteProductMetadata::Publisher),
                                        QString::fromLatin1(LiteProductMetadata::ProductName)));
}

QString SingleInstanceIdentity::normalizeDataDirectory(const QString &dataDirectory) {
    const auto path = dataDirectory.isEmpty() ? defaultDataDirectory() : dataDirectory;
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString SingleInstanceIdentity::serviceName(const QString &dataDirectory) {
    const auto identity = normalizeDataDirectory(dataDirectory).toUtf8();
    const auto suffix =
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex().left(16);
    return QStringLiteral("%1-%2").arg(productIdentity(), QString::fromLatin1(suffix));
}

QString SingleInstanceIdentity::lockFilePath(const QString &dataDirectory) {
    return QDir(normalizeDataDirectory(dataDirectory)).filePath(QString::fromLatin1(lockFileName));
}
