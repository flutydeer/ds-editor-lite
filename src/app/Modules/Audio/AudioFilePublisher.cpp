#include "AudioFilePublisher.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <utility>

namespace Audio::Internal {
    namespace {
        bool replaceFile(const QString &temporary, const QString &target) {
            QFile source(temporary);
            if (!source.open(QIODevice::ReadOnly))
                return false;
            QSaveFile destination(target);
            destination.setDirectWriteFallback(false);
            if (!destination.open(QIODevice::WriteOnly))
                return false;

            constexpr qint64 chunkSize = 1 << 20;
            while (!source.atEnd()) {
                const auto chunk = source.read(chunkSize);
                if (chunk.isEmpty() && source.error() != QFileDevice::NoError)
                    return false;
                if (destination.write(chunk) != chunk.size())
                    return false;
            }
            return destination.commit();
        }
    }

    AudioFilePublicationResult publishAudioFiles(const QHash<QString, QString> &pendingFiles,
                                                 const bool allowOverwrite) {
        QStringList targets = pendingFiles.keys();
        targets.sort(Qt::CaseSensitive);
        AudioFilePublicationResult result;
        for (const auto &target : std::as_const(targets)) {
            const auto temporary = pendingFiles.value(target);
            const auto published = allowOverwrite ? replaceFile(temporary, target)
                                                  : QFile(temporary).rename(target);
            if (!published) {
                result.failedTarget = target;
                break;
            }
            if (allowOverwrite && !QFile::remove(temporary))
                result.remainingTemporaryFiles.append(temporary);
        }
        if (!result.succeeded()) {
            for (const auto &target : std::as_const(targets)) {
                const auto temporary = pendingFiles.value(target);
                if (QFileInfo::exists(temporary) &&
                    !result.remainingTemporaryFiles.contains(temporary)) {
                    result.remainingTemporaryFiles.append(temporary);
                }
            }
        }
        return result;
    }
}
