#include "DocumentImportController.h"

#include "AudioFilePreparer.h"
#include "ExternalFileClassifier.h"
#include "MidiFilePreparer.h"

#include "Controller/Actions/AppModel/Import/BatchImportActions.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include "Controller/PlaybackController.h"
#include "Controller/Tasks/DecodeAudioTask.h"
#include "Controller/TrackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/MidiBatchImportDialog.h"

#include <lite/History/HistoryManager.h>
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

namespace {
constexpr auto kFormatDataKey = "diffscope.audio.formatData";

AudioClip *buildAudioClip(const PreparedAudioItem &audio, const int startTick,
                          const Timeline &timeline) {
    const auto clip = new AudioClip;
    clip->setName(QFileInfo(audio.path).baseName());
    clip->setStart(startTick);
    clip->setClipStart(0);
    const auto posMs = timeline.tickToMs(startTick);
    const auto length = qMax(1, qRound(timeline.msToTick(posMs + audio.durationMs)) - startTick);
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

    // MIDI files parse synchronously (fast); failures become Failed items.
    auto parseData = MidiFileParser::parse(path);
    PreparedImportItem item;
    item.path = path;
    if (!parseData.valid) {
        item.kind = PreparedImportItem::Kind::Failed;
        item.errorMessage = parseData.errorMessage;
    } else {
        bool hasNotes = false;
        for (const auto &info : parseData.trackInfos) {
            if (info.noteCount > 0) {
                hasNotes = true;
                break;
            }
        }
        if (!hasNotes) {
            item.kind = PreparedImportItem::Kind::Failed;
            item.errorMessage = tr("No notes in MIDI file");
        } else {
            item.kind = PreparedImportItem::Kind::Midi;
            item.midi = std::move(parseData);
        }
    }
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
    auto *model = appModel;
    const auto language = appOptions->general()->defaultSingingLanguage;
    const auto defaultLyric = appOptions->general()->defaultLyricForLanguage(language);

    // 1. Generate MIDI tracks; tempo / time signature come from the first
    //    successfully generated MIDI file only.
    struct GeneratedMidi {
        PreparedImportItem prepared;
        MidiGenerationResult result;
    };
    QList<GeneratedMidi> generatedMidis;
    QList<Tempo> newTempos;
    QList<TimeSignature> newSignatures;
    bool hasImportedTimeline = false;
    for (auto &item : m_prepared) {
        if (item.kind != PreparedImportItem::Kind::Midi)
            continue;
        const auto options = MidiFilePreparer::makeBatchOptions(
            m_codec, m_importTempo, m_importTimeSignature, item.midi);
        auto result =
            MidiTrackGenerator::generateTracks(item.midi, options, language, defaultLyric,
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
        generatedMidis.append({item, std::move(result)});
    }

    // 2. The final timeline the batch commits; audio lengths are derived from
    //    it.
    auto finalTimeline = model->timeline();
    if (hasImportedTimeline) {
        if (!newSignatures.isEmpty())
            finalTimeline.setTimeSignatures(newSignatures);
        if (!newTempos.isEmpty())
            finalTimeline.setTempos(newTempos);
    }

    // 3. Shared track cursor: consume existing tracks first, then create new
    //    ones. One audio clip or one generated MIDI track occupies one slot.
    QList<BatchImportActions::Item> items;
    QList<Clip *> firstSuccessClips;
    QList<AudioClip *> audioClipsForHash;
    int existingCursor = 0;
    int midiCursor = 0;
    for (const auto &item : m_prepared) {
        if (item.kind == PreparedImportItem::Kind::Audio) {
            const auto startTick =
                m_target ? m_target->audioStartTick
                         : qRound(playbackController->position());
            auto *clip = buildAudioClip(item.audio, startTick, finalTimeline);

            BatchImportActions::Item entry;
            entry.clip = clip;
            if (m_target && existingCursor < m_target->existingTrackIds.size()) {
                const auto trackId = m_target->existingTrackIds.at(existingCursor++);
                auto *track = model->findTrackById(trackId);
                if (!track) {
                    // The target track vanished; the content fails instead of
                    // being moved to another track.
                    m_failedMessages.append(
                        item.path + QStringLiteral(" - ") + tr("Target track was deleted"));
                    delete clip;
                    continue;
                }
                entry.existingTrack = track;
            } else {
                const auto track = new Track;
                track->setName(QFileInfo(item.path).baseName());
                track->setDefaultLanguage(language);
                track->insertClip(clip);
                entry.newTrack = track;
            }
            items.append(entry);
            m_successCount++;
            audioClipsForHash.append(static_cast<AudioClip *>(clip));
            firstSuccessClips.append(clip);
        } else if (item.kind == PreparedImportItem::Kind::Midi && midiCursor < generatedMidis.size()) {
            auto &generated = generatedMidis.at(midiCursor++).result;
            for (auto *track : generated.tracks) {
                BatchImportActions::Item entry;
                if (m_target && existingCursor < m_target->existingTrackIds.size()) {
                    const auto trackId = m_target->existingTrackIds.at(existingCursor++);
                    auto *existingTrack = model->findTrackById(trackId);
                    if (!existingTrack) {
                        m_failedMessages.append(
                            item.path + QStringLiteral(" - ") +
                            tr("Target track was deleted"));
                        delete track;
                        continue;
                    }
                    // Write into an existing track: only the clips move in,
                    // the track properties are preserved.
                    const auto clips = track->clips();
                    for (const auto clip : clips)
                        track->removeClip(clip);
                    delete track;
                    for (const auto clip : clips) {
                        BatchImportActions::Item clipEntry;
                        clipEntry.clip = clip;
                        clipEntry.existingTrack = existingTrack;
                        items.append(clipEntry);
                        firstSuccessClips.append(clip);
                    }
                    m_successCount++;
                } else {
                    entry.newTrack = track;
                    items.append(entry);
                    const auto clips = track->clips();
                    if (clips.count() > 0)
                        firstSuccessClips.append(*clips.begin());
                    m_successCount++;
                }
            }
        }
    }

    // 4. Commit everything as one undoable history item.
    if (items.isEmpty())
        return;
    auto *batch = BatchImportActions::build(model->timeline().tempos(), newTempos,
                                            model->timeline().timeSignatures(), newSignatures,
                                            items, model);
    batch->execute();
    historyManager->record(batch);

    // 5. Activate the first successfully imported clip and start audio
    //    hashing for the committed audio clips.
    if (!firstSuccessClips.isEmpty()) {
        const auto firstId = firstSuccessClips.first()->id();
        appStatus->selectedClips = QList<int>{firstId};
        trackController->setActiveClip(firstId);
    }
    for (const auto clip : audioClipsForHash)
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
