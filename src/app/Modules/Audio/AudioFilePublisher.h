#ifndef AUDIO_AUDIOFILEPUBLISHER_H
#define AUDIO_AUDIOFILEPUBLISHER_H

#include <QHash>
#include <QStringList>

namespace Audio::Internal {

    struct AudioFilePublicationResult {
        QString failedTarget;
        QStringList recoveryFailures;
        QStringList remainingTemporaryFiles;
        QStringList backupCleanupFailures;

        [[nodiscard]] bool succeeded() const {
            return failedTarget.isEmpty();
        }
    };

    [[nodiscard]] AudioFilePublicationResult
        publishAudioFiles(const QHash<QString, QString> &pendingFiles);

}

#endif // AUDIO_AUDIOFILEPUBLISHER_H
