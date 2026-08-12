#include "FileRevision.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>

bool FileRevisionUtils::capture(const QString &path, FileRevision &revision, QString *error) {
    revision = {};
    const QFileInfo info(path);
    if (!info.exists())
        return true;
    if (!info.isFile()) {
        if (error)
            *error = QStringLiteral("Path is not a regular file: %1").arg(path);
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        if (error)
            *error = file.errorString();
        return false;
    }

    revision.exists = true;
    revision.size = info.size();
    revision.lastModifiedMs = info.lastModified().toMSecsSinceEpoch();
    revision.sha256 = hash.result();
    return true;
}

bool FileRevisionUtils::matchesCurrent(const QString &path, const FileRevision &expected,
                                       QString *error) {
    FileRevision current;
    return capture(path, current, error) && current == expected;
}
