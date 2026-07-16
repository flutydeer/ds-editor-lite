#include "ResolveAudioPathTask.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

void ResolveAudioPathTask::runTask() {
    result = Result::Miss;
    resolvedPath.clear();
    if (fileName.isEmpty() || projectDir.isEmpty())
        return;

    struct Candidate {
        QString path;
        Result hit;
    };
    QList<Candidate> candidates;
    const QDir projDir(projectDir);
    if (!relativeDir.isEmpty())
        candidates.append({QDir::cleanPath(projDir.filePath(relativeDir + '/' + fileName)),
                           Result::HitRelative});
    candidates.append({QDir::cleanPath(projDir.filePath(fileName)), Result::HitSibling});

    for (const auto &candidate : candidates) {
        if (isTerminateRequested())
            return;
        if (!QFileInfo::exists(candidate.path))
            continue;
        if (expectedSha512.isEmpty()) {
            // No digest to verify against; matched by file name, pending user confirmation
            result = Result::HitUnconfirmed;
            resolvedPath = candidate.path;
            return;
        }
        const auto actual = computeSha512(candidate.path);
        if (actual.compare(expectedSha512, Qt::CaseInsensitive) == 0) {
            result = candidate.hit;
            resolvedPath = candidate.path;
            return;
        }
    }
}

QString ResolveAudioPathTask::computeSha512(const QString &filePath) const {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha512);
    constexpr qint64 chunkSize = 1 << 20; // 1 MiB
    while (!file.atEnd()) {
        if (isTerminateRequested())
            return {};
        hash.addData(file.read(chunkSize));
    }
    return QString::fromLatin1(hash.result().toHex());
}
