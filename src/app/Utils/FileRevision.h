#ifndef FILEREVISION_H
#define FILEREVISION_H

#include <QByteArray>
#include <QString>

struct FileRevision {
    bool exists = false;
    qint64 size = 0;
    qint64 lastModifiedMs = 0;
    QByteArray sha256;

    friend bool operator==(const FileRevision &, const FileRevision &) = default;
};

namespace FileRevisionUtils {
    bool capture(const QString &path, FileRevision &revision, QString *error = nullptr);
    bool matchesCurrent(const QString &path, const FileRevision &expected,
                        QString *error = nullptr);
}

#endif // FILEREVISION_H
