#include "AudioFilePreparer.h"

#include "Controller/Tasks/DecodeAudioTask.h"

#include <QDataStream>
#include <QFileInfo>
#include <QIODevice>

#include <TalcsFormat/AbstractAudioFormatIO.h>
#include <TalcsFormat/FormatEntry.h>
#include <TalcsFormat/FormatManager.h>

#include "Modules/Audio/AudioContext.h"

DecodeAudioTask *AudioFilePreparer::createPrepareTask(const QString &path,
                                                      talcs::AbstractAudioFormatIO *io,
                                                      const QJsonObject &workspace) {
    auto *task = new DecodeAudioTask;
    task->path = path;
    task->workspace = workspace.isEmpty() ? probeWorkspace(path) : workspace;
    if (!io)
        io = audioContext->formatManager()->getFormatLoad(path);
    task->io = io;
    return task;
}

PreparedAudioItem AudioFilePreparer::prepareResult(const DecodeAudioTask *task) {
    PreparedAudioItem item;
    item.path = task->path;
    item.workspace = task->workspace;
    item.audioInfo = task->result();
    const auto sampleRate = item.audioInfo.sampleRate;
    item.durationMs = sampleRate > 0
                          ? static_cast<double>(item.audioInfo.frames) * 1000.0 / sampleRate
                          : 0.0;
    return item;
}

QJsonObject AudioFilePreparer::makeWorkspace(const QVariant &userData,
                                             const QString &entryClassName) {
    QByteArray dataBuffer;
    QDataStream stream(&dataBuffer, QIODevice::WriteOnly);
    stream << userData;
    return {
        {QStringLiteral("userData"),       QString::fromLatin1(dataBuffer.toBase64())},
        {QStringLiteral("entryClassName"), entryClassName                            },
    };
}

QJsonObject AudioFilePreparer::probeWorkspace(const QString &path) {
    const auto *formatManager = audioContext->formatManager();
    const auto *entry = formatManager->hintFromExtension(QFileInfo(path).suffix());
    if (!entry)
        return {};
    return makeWorkspace({}, QString::fromLatin1(entry->metaObject()->className()));
}
