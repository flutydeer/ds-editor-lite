#include "ComputeAudioHashTask.h"

#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>

#include <memory>

void ComputeAudioHashTask::runTask() {
    success = false;
    resultSha512.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return;

    std::unique_ptr<QSaveFile> snapshot;
    if (!snapshotPath.isEmpty()) {
        snapshot = std::make_unique<QSaveFile>(snapshotPath);
        if (!snapshot->open(QIODevice::WriteOnly))
            return;
    }

    QCryptographicHash hash(QCryptographicHash::Sha512);
    constexpr qint64 chunkSize = 1 << 20; // 1 MiB
    while (!file.atEnd()) {
        if (isTerminateRequested())
            return;
        const auto chunk = file.read(chunkSize);
        if (chunk.isEmpty() && file.error() != QFileDevice::NoError)
            return;
        if (snapshot && snapshot->write(chunk) != chunk.size())
            return;
        hash.addData(chunk);
    }
    if (snapshot && !snapshot->commit())
        return;
    resultSha512 = QString::fromLatin1(hash.result().toHex());
    success = true;
}
