#include "ComputeAudioHashTask.h"

#include <QCryptographicHash>
#include <QFile>

void ComputeAudioHashTask::runTask() {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        success = false;
        return;
    }

    QCryptographicHash hash(QCryptographicHash::Sha512);
    constexpr qint64 chunkSize = 1 << 20; // 1 MiB
    while (!file.atEnd()) {
        if (isTerminateRequested()) {
            success = false;
            return;
        }
        hash.addData(file.read(chunkSize));
    }
    resultSha512 = QString::fromLatin1(hash.result().toHex());
    success = true;
}
