#include "DocumentImportController.h"

#include "AudioFilePreparer.h"
#include "ExternalFileClassifier.h"
#include "MidiFilePreparer.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Automation/ProjectAutomationDtos.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/PlaybackController.h"
#include "Controller/Tasks/DecodeAudioTask.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/MidiBatchImportDialog.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/Utils/DiffscopeAudioWorkspace.h>
#include <lite/Tasking/Task.h>
#include <lite/Tasking/TaskManager.h>
#include <lite/GUI/Controls/Button.h>

#include "UI/Dialogs/Base/Dialog.h"
#include "UI/Dialogs/Base/TaskDialog.h"

#include <QDialog>
#include <QFileInfo>
#include <QJsonObject>

#include <algorithm>
#include <memory>

namespace {
    constexpr auto kFormatDataKey = "diffscope.audio.formatData";

    AudioClip *buildAudioClip(const PreparedAudioItem &audio, const int startTick,
                              const Timeline &timeline) {
        const auto clip = new AudioClip;
        clip->setName(QFileInfo(audio.path).baseName());
        clip->setStart(startTick);
        clip->setClipStart(0);
        const auto posMs = timeline.tickToMs(startTick);
        const auto length =
            qMax(1, qRound(timeline.msToTick(posMs + audio.durationMs)) - startTick);
        clip->setLength(length);
        clip->setClipLen(length);
        clip->setPath(audio.path);
        clip->setAudioInfo(audio.audioInfo);
        clip->setRealTimeAnchor(0, audio.durationMs, audio.durationMs);
        clip->workspace().insert(QLatin1String(kFormatDataKey), audio.workspace);
        clip->setPathInfo({DiffscopeAudioWorkspace::relativeDirFor(
                               audio.path, documentWorkflowController->projectPath()),
                           {}});
        return clip;
    }

    Automation::CoreRuntime *automationRuntime() {
        return AppContext::instance<Automation::CoreRuntime>();
    }
} // namespace

DocumentImportController::DocumentImportController(QObject *parent) : QObject(parent) {
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(DocumentImportController)

void DocumentImportController::requestImport(const QStringList &paths,
                                             std::optional<FileImportDropTarget> target) {
    if (m_currentTask || !m_pendingPaths.isEmpty() || !m_prepared.isEmpty())
        return;

    // Classify and route the batch.
    QStringList importablePaths;
    QString projectPath;
    int projectCount = 0;
    for (const auto &path : paths) {
        const auto result = ExternalFileClassifier::classify(path);
        switch (result.kind) {
            case ExternalFileKind::Project:
                projectCount++;
                projectPath = path;
                break;
            case ExternalFileKind::Midi:
            case ExternalFileKind::Audio:
                importablePaths.append(path);
                break;
            case ExternalFileKind::Unsupported:
                m_failedMessages.append(path + QStringLiteral(" - ") + result.reason);
                break;
        }
    }

    if (projectCount == 1 && importablePaths.isEmpty() && m_failedMessages.isEmpty()) {
        documentWorkflowController->requestImport(projectPath);
        return;
    }
    if (projectCount > 0) {
        showMessageDialog(tr("Import"),
                          tr("A project file cannot be imported together with other files."));
        m_failedMessages.clear();
        return;
    }
    if (importablePaths.isEmpty()) {
        showMessageDialog(tr("Import"), tr("No importable files in the selection."));
        m_failedMessages.clear();
        return;
    }

    m_target = std::move(target);
    m_pendingPaths = importablePaths;
    startPreparation();
}

void DocumentImportController::startPreparation() {
    if (auto *runtime = automationRuntime())
        m_generationAnchor = runtime->documentVersion();
    m_prepared.clear();
    m_successCount = 0;
    m_canceled = false;
    prepareNext();
}

void DocumentImportController::prepareNext() {
    if (m_canceled || m_pendingPaths.isEmpty()) {
        onAllPrepared();
        return;
    }
    const auto path = m_pendingPaths.takeFirst();
    const auto kind = ExternalFileClassifier::classify(path).kind;
    if (kind == ExternalFileKind::Audio) {
        auto *task = AudioFilePreparer::createPrepareTask(path);
        m_currentTask = task;
        const auto dlg = new TaskDialog(task, true, true, nullptr);
        dlg->show();
        connect(task, &Task::finished, this, [this, task] { onAudioTaskFinished(task); });
        taskManager->addTask(task);
        taskManager->startTask(task);
        return;
    }

    auto prepared = MidiFilePreparer::prepare({path});
    auto item = std::move(prepared.first());
    if (const auto failure = MidiFilePreparer::failureMessage(item); !failure.isEmpty())
        m_failedMessages.append(failure);
    m_prepared.append(std::move(item));
    prepareNext();
}

void DocumentImportController::onAudioTaskFinished(DecodeAudioTask *task) {
    const auto terminated = task->terminated();
    taskManager->removeTask(task);
    m_currentTask = nullptr;
    if (terminated) {
        delete task;
        m_canceled = true;
        prepareNext();
        return;
    }
    if (task->success) {
        PreparedImportItem item;
        item.kind = PreparedImportItem::Kind::Audio;
        item.path = task->path;
        item.audio = AudioFilePreparer::prepareResult(task);
        m_prepared.append(std::move(item));
    } else {
        m_failedMessages.append(task->path + QStringLiteral(" - ") + task->errorMessage);
    }
    delete task;
    prepareNext();
}

void DocumentImportController::onAllPrepared() {
    if (!m_canceled) {
        // One shared configuration dialog for the whole MIDI batch.
        QList<PreparedImportItem> midiItems;
        for (const auto &item : m_prepared) {
            if (item.kind == PreparedImportItem::Kind::Midi)
                midiItems.append(item);
        }
        if (!midiItems.isEmpty()) {
            MidiBatchImportDialog dlg(MidiFilePreparer::detectCommonCodec(midiItems),
                                      Dialog::globalParent());
            if (dlg.exec() != QDialog::Accepted) {
                // Canceling the options dialog cancels the whole batch.
                reset();
                return;
            }
            m_codec = dlg.selectedCodec();
            m_importTempo = dlg.importTempo();
            m_importTimeSignature = dlg.importTimeSignature();
        }
    }
    commitBatch();
    showSummary();
    reset();
}

void DocumentImportController::commitBatch() {
    auto *runtime = automationRuntime();
    if (!runtime) {
        m_failedMessages.append(tr("Automation runtime is unavailable"));
        return;
    }
    const auto context = runtime->documentWorkflowCommitContext(m_generationAnchor);
    if (!context) {
        m_failedMessages.append(tr("Batch commit failed: %1").arg(context.getError().message));
        return;
    }
    auto *model = appModel;
    const auto language = appOptions->general()->defaultSingingLanguage;
    const auto defaultLyric = appOptions->general()->defaultLyricForLanguage(language);

    struct GeneratedMidi {
        int preparedIndex = -1;
        MidiGenerationResult result;
    };

    QList<GeneratedMidi> generatedMidis;
    QList<Tempo> newTempos;
    QList<TimeSignature> newSignatures;
    bool hasImportedTimeline = false;
    for (int preparedIndex = 0; preparedIndex < m_prepared.size(); ++preparedIndex) {
        auto &item = m_prepared[preparedIndex];
        if (item.kind != PreparedImportItem::Kind::Midi)
            continue;
        const auto options = MidiFilePreparer::makeBatchOptions(m_codec, m_importTempo,
                                                                m_importTimeSignature, item.midi);
        auto result = MidiTrackGenerator::generateTracks(item.midi, options, language, defaultLyric,
                                                         model->timeline());
        if (!result.errorMessage.isEmpty()) {
            m_failedMessages.append(item.path + QStringLiteral(" - ") + result.errorMessage);
            continue;
        }
        if (!hasImportedTimeline && result.hasTimeline) {
            newTempos = result.tempos;
            newSignatures = result.timeSignatures;
            hasImportedTimeline = true;
        }
        generatedMidis.append({preparedIndex, std::move(result)});
    }

    auto finalTimeline = model->timeline();
    if (hasImportedTimeline) {
        if (!newSignatures.isEmpty())
            finalTimeline.setTimeSignatures(newSignatures);
        if (!newTempos.isEmpty())
            finalTimeline.setTempos(newTempos);
    }

    Automation::BatchImportDraftDto batch;
    batch.timeline = finalTimeline;
    int existingCursor = 0;
    for (int preparedIndex = 0; preparedIndex < m_prepared.size(); ++preparedIndex) {
        const auto &item = m_prepared.at(preparedIndex);
        if (item.kind == PreparedImportItem::Kind::Audio) {
            const auto startTick =
                m_target ? m_target->audioStartTick : qRound(playbackController->position());
            const std::unique_ptr<AudioClip> clip(
                buildAudioClip(item.audio, startTick, finalTimeline));
            Automation::BatchImportItemDraftDto entry;
            entry.clips.append(Automation::clipDraftDto(*clip));
            if (m_target && existingCursor < m_target->existingTrackIds.size()) {
                const auto trackId = m_target->existingTrackIds.at(existingCursor++);
                if (!model->findTrackById(trackId)) {
                    m_failedMessages.append(item.path + QStringLiteral(" - ") +
                                            tr("Target track was deleted"));
                    continue;
                }
                entry.existingTrackId = Automation::TrackId(trackId);
            } else {
                entry.newTrack.name = QFileInfo(item.path).baseName();
                entry.newTrack.defaultLanguage = language;
            }
            batch.items.append(std::move(entry));
            m_successCount++;
        } else if (item.kind == PreparedImportItem::Kind::Midi) {
            const auto generatedIt =
                std::find_if(generatedMidis.begin(), generatedMidis.end(),
                             [preparedIndex](const auto &generated) {
                                 return generated.preparedIndex == preparedIndex;
                             });
            if (generatedIt == generatedMidis.end())
                continue;
            auto &generated = generatedIt->result;
            for (auto *track : generated.tracks) {
                const std::unique_ptr<Track> ownedTrack(track);
                Automation::BatchImportItemDraftDto entry;
                entry.newTrack = Automation::trackDraftDto(*track);
                entry.clips = std::move(entry.newTrack.clips);
                entry.newTrack.clips.clear();
                if (m_target && existingCursor < m_target->existingTrackIds.size()) {
                    const auto trackId = m_target->existingTrackIds.at(existingCursor++);
                    if (!model->findTrackById(trackId)) {
                        m_failedMessages.append(item.path + QStringLiteral(" - ") +
                                                tr("Target track was deleted"));
                        continue;
                    }
                    entry.existingTrackId = Automation::TrackId(trackId);
                }
                batch.items.append(std::move(entry));
                m_successCount++;
            }
        }
    }

    if (batch.items.isEmpty())
        return;
    const auto result = runtime->project().commitBatchImport(context.get(), batch);
    if (!result) {
        m_successCount = 0;
        m_failedMessages.append(tr("Batch commit failed: %1").arg(result.getError().message));
        return;
    }

    int firstClipId = -1;
    QList<AudioClip *> audioClipsForHash;
    for (const auto &affected : result.get().affectedObjects) {
        if (affected.kind != Automation::ObjectKind::Clip)
            continue;
        if (firstClipId < 0)
            firstClipId = affected.value;
        if (auto *audio = dynamic_cast<AudioClip *>(model->findClipById(affected.value)))
            audioClipsForHash.append(audio);
    }
    if (firstClipId >= 0) {
        const auto firstId = firstClipId;
        trackController->setSelectedClips({firstId});
        trackController->setActiveClip(firstId);
    }
    for (auto *clip : audioClipsForHash)
        trackController->scheduleHashUpdate(clip);
}

void DocumentImportController::showSummary() const {
    if (m_failedMessages.isEmpty())
        return;
    showMessageDialog(tr("Import completed with errors"),
                      tr("The following files were not imported:\n%1")
                          .arg(m_failedMessages.join(QLatin1Char('\n'))));
}

void DocumentImportController::reset() {
    m_pendingPaths.clear();
    m_prepared.clear();
    m_currentTask = nullptr;
    m_target.reset();
    m_failedMessages.clear();
    m_successCount = 0;
    m_canceled = false;
}

void DocumentImportController::showMessageDialog(const QString &title,
                                                 const QString &message) const {
    auto *dlg = new Dialog(Dialog::globalParent());
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(title);
    dlg->setTitle(title);
    dlg->setMessage(message);
    const auto btnClose = new Button(tr("Close"), dlg);
    dlg->setPositiveButton(btnClose);
    connect(btnClose, &Button::clicked, dlg, &Dialog::accept);
    dlg->exec();
}
