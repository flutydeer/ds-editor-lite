#include "InferController.h"
#include "InferEngine.h"
#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Model/AppStatus/AppStatus.h"
#include "InferController_p.h"

#include "InferControllerHelper.h"
#include "InferenceAutomationBridge.h"
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include "Model/AppOptions/AppOptions.h"
#include <lite/ProjectModel/InferenceData/InferSpeakerMix.h>
#include "Models/NoteInferenceSnapshot.h"
#include "Tasks/GetPhonemeNameTask.h"
#include "Tasks/GetPronunciationTask.h"
#include "Utils/InferenceApplyGate.h"
#include "Utils/InferCacheUtils.h"
#include <lite/Support/Linq.h>
#include "Utils/ValidationUtils.h"
#include "Controller/PlaybackController.h"
#include "InferPipeline.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include "Modules/Inference/EditSessionManager.h"

#include <QTimer>
#include <QPointer>
#include <QLoggingCategory>
#include <QDir>

Q_LOGGING_CATEGORY(logInferController, "infer.controller")

#include <algorithm>
#include <utility>

namespace Helper = InferControllerHelper;

namespace {
    struct PieceGlobalRange {
        int start = INT_MAX;
        int end = INT_MAX;

        [[nodiscard]] bool isValid() const {
            return start != INT_MAX;
        }
    };

    PieceGlobalRange pieceGlobalRange(int clipId, int pieceId) {
        const auto clip = dynamic_cast<SingingClip *>(appModel->findClipById(clipId));
        if (!clip)
            return {};
        const auto piece = clip->findPieceById(pieceId);
        if (!piece)
            return {};
        const auto &timeline = appModel->timeline();
        return {piece->localStartTick(timeline) + clip->start(),
                piece->localEndTick(timeline) + clip->start()};
    }

    QList<NoteInferenceSnapshot> buildNoteInferenceSnapshots(const SingingClip &clip) {
        QList<NoteInferenceSnapshot> result;
        result.reserve(clip.notes().count());
        for (const auto note : clip.notes()) {
            NoteInferenceSnapshot snapshot;
            snapshot.noteId = note->id();
            snapshot.lyric = note->lyric();
            snapshot.language = note->effectiveLanguage();
            snapshot.pronunciation = note->pronunciation().result();
            snapshot.globalStart = note->globalStart();
            snapshot.length = note->length();
            snapshot.keyIndex = note->keyIndex();
            result.append(snapshot);
        }
        return result;
    }

    // 排序键：档位越小优先级越高，同档位内距播放头越近越优先。
    // 档位 0 = 播放头落在片段范围内(必须最先推理，否则一定位就听不到声音)，
    // 1 = 播放头之后，2 = 播放头之前，3 = 片段已不存在。
    std::pair<int, int> playbackPriorityKey(const PieceGlobalRange &range, const int pos) {
        if (!range.isValid())
            return {3, 0};
        if (range.start <= pos && pos < range.end)
            return {0, 0};
        if (range.start >= pos)
            return {1, range.start - pos};
        return {2, pos - range.start};
    }

    template <typename T>
    std::function<bool(T *, T *)> makePlaybackPriorityComparator() {
        return [](T *a, T *b) {
            const auto pos = static_cast<int>(playbackController->position());
            return playbackPriorityKey(pieceGlobalRange(a->clipId(), a->pieceId()), pos) <
                   playbackPriorityKey(pieceGlobalRange(b->clipId(), b->pieceId()), pos);
        };
    }

    bool clipPiecesMatchCurrentSingerAndSpeaker(const SingingClip &clip) {
        const auto identifier = clip.singerIdentifier();
        const auto &timeline = appModel->timeline();
        for (const auto piece : clip.pieces()) {
            const auto speakerMix = InferSpeakerMixModel::effectiveSpeakerMixFromData(
                clip.speakerMixData(), clip.speakerId(),
                clip.start() + piece->localStartTick(timeline),
                clip.start() + piece->localEndTick(timeline), clip.start(), timeline);
            if (piece->identifier != identifier || piece->speakerMix != speakerMix)
                return false;
        }
        return true;
    }

    bool intersectsTempoRanges(const QList<TempoChangeRange> &ranges, const int start,
                               const int end) {
        return std::any_of(
            ranges.cbegin(), ranges.cend(),
            [start, end](const TempoChangeRange &range) { return range.intersects(start, end); });
    }

    template <typename T>
    InferenceTaskContext buildClipTaskContext(const QString &taskType, const T &task) {
        InferenceTaskContext context;
        context.documentVersion = task.documentVersion();
        context.taskType = taskType;
        context.taskId = task.id();
        context.clipId = task.clipId();
        context.clipRevision = task.clipRevision();
        context.noteIds = task.noteIds();
        return context;
    }

    Automation::InferenceMutationRequest
        pronunciationMutation(const InferenceTaskContext &context,
                              const QList<PronunciationFetchResult> &pronunciations) {
        Automation::InferenceMutationRequest request;
        request.kind = Automation::InferenceMutationKind::ApplyPronunciations;
        request.clipId = Automation::ClipId(context.clipId);
        for (qsizetype i = 0; i < pronunciations.count(); ++i) {
            request.pronunciations.append({
                .noteId = Automation::NoteId(context.noteIds.at(i)),
                .pronunciation = pronunciations.at(i).pronunciation,
                .candidates = pronunciations.at(i).candidates,
            });
        }
        return request;
    }

    Automation::InferenceMutationRequest
        phonemeNameMutation(const InferenceTaskContext &context,
                            const QList<PhonemeNameResult> &phonemeNames) {
        Automation::InferenceMutationRequest request;
        request.kind = Automation::InferenceMutationKind::ApplyPhonemeNames;
        request.clipId = Automation::ClipId(context.clipId);
        for (qsizetype i = 0; i < phonemeNames.count(); ++i) {
            request.phonemeNames.append({
                .noteId = Automation::NoteId(context.noteIds.at(i)),
                .phonemeNames = phonemeNames.at(i).phonemeNames,
            });
        }
        return request;
    }
} // namespace

InferController::InferController(QObject *parent)
    : QObject(parent), d_ptr(new InferControllerPrivate(this)) {
    Q_D(InferController);
    d->m_autoStartAcousticInfer = appOptions->inference()->autoStartInfer;

    d->m_inferDurTasks.setPriorityComparator(makePlaybackPriorityComparator<InferDurationTask>());
    d->m_inferPitchTasks.setPriorityComparator(makePlaybackPriorityComparator<InferPitchTask>());
    d->m_inferVarianceTasks.setPriorityComparator(
        makePlaybackPriorityComparator<InferVarianceTask>());
    d->m_inferAcousticTasks.setPriorityComparator(
        makePlaybackPriorityComparator<InferAcousticTask>());
    d->m_inferAcousticCacheProbeTasks.setPriorityComparator(
        makePlaybackPriorityComparator<InferAcousticCacheProbeTask>());

    connect(appStatus, &AppStatus::moduleStatusChanged, d,
            &InferControllerPrivate::onModuleStatusChanged);
    connect(appStatus, &AppStatus::editingChanged, d, &InferControllerPrivate::onEditingChanged);
    connect(editSessionManager, &EditSessionManager::editSessionEnded, d,
            &InferControllerPrivate::onEditSessionEnded);
    connect(appOptions, &AppOptions::optionsChanged, d,
            &InferControllerPrivate::onInferOptionChanged);
    connect(playbackController, &PlaybackController::playbackStatusChanged, d,
            &InferControllerPrivate::onPlaybackStatusChanged);
    connect(playbackController, &PlaybackController::positionChanged, d,
            &InferControllerPrivate::onPlaybackPositionChanged);
    d->syncSingerSessions();
}

InferController::~InferController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(InferController)

void InferController::restartPieceInference(InferPiece &piece) {
    Q_D(InferController);
    d->createPipeline(piece);
}

void InferController::cancelPieceInference(const int pieceId) {
    Q_D(InferController);
    d->cancelPieceRelatedTasks(pieceId);
}

void InferController::addInferDurationTask(InferDurationTask &task) {
    Q_D(InferController);
    d->m_inferDurTasks.add(&task);
}

void InferController::cancelInferDurationTask(int taskId) {
    Q_D(InferController);
    d->m_inferDurTasks.cancelIf(L_PRED(t, t->id() == taskId));
}

bool InferController::finishCurrentInferDurationTask(InferDurationTask *task) {
    Q_D(InferController);
    if (task) {
        const auto cacheDir = appOptions->inference()->cacheDirectory;
        for (const auto &fileName : task->cacheFileNames())
            InferCacheUtils::registerCacheFile(QDir(cacheDir).filePath(fileName));
    }
    return d->m_inferDurTasks.onCurrentFinished(task);
}

void InferController::addInferPitchTask(InferPitchTask &task) {
    Q_D(InferController);
    d->m_inferPitchTasks.add(&task);
}

void InferController::cancelInferPitchTask(int taskId) {
    Q_D(InferController);
    d->m_inferPitchTasks.cancelIf(L_PRED(t, t->id() == taskId));
}

bool InferController::finishCurrentInferPitchTask(InferPitchTask *task) {
    Q_D(InferController);
    if (task) {
        const auto cacheDir = appOptions->inference()->cacheDirectory;
        for (const auto &fileName : task->cacheFileNames())
            InferCacheUtils::registerCacheFile(QDir(cacheDir).filePath(fileName));
    }
    return d->m_inferPitchTasks.onCurrentFinished(task);
}

void InferController::addInferVarianceTask(InferVarianceTask &task) {
    Q_D(InferController);
    d->m_inferVarianceTasks.add(&task);
}

void InferController::cancelInferVarianceTask(int taskId) {
    Q_D(InferController);
    d->m_inferVarianceTasks.cancelIf(L_PRED(t, t->id() == taskId));
}

bool InferController::finishCurrentInferVarianceTask(InferVarianceTask *task) {
    Q_D(InferController);
    if (task) {
        const auto cacheDir = appOptions->inference()->cacheDirectory;
        for (const auto &fileName : task->cacheFileNames())
            InferCacheUtils::registerCacheFile(QDir(cacheDir).filePath(fileName));
    }
    return d->m_inferVarianceTasks.onCurrentFinished(task);
}

void InferController::addInferAcousticTask(InferAcousticTask &task) {
    Q_D(InferController);
    d->m_inferAcousticTasks.add(&task);
}

void InferController::cancelInferAcousticTask(int taskId) {
    Q_D(InferController);
    d->m_inferAcousticTasks.cancelIf(L_PRED(t, t->id() == taskId));
}

bool InferController::finishCurrentInferAcousticTask(InferAcousticTask *task) {
    Q_D(InferController);
    if (task) {
        const auto cacheDir = appOptions->inference()->cacheDirectory;
        for (const auto &fileName : task->cacheFileNames())
            InferCacheUtils::registerCacheFile(QDir(cacheDir).filePath(fileName));
    }
    return d->m_inferAcousticTasks.onCurrentFinished(task);
}

void InferController::addInferAcousticCacheProbeTask(InferAcousticCacheProbeTask &task) {
    Q_D(InferController);
    d->m_inferAcousticCacheProbeTasks.add(&task);
}

void InferController::cancelInferAcousticCacheProbeTask(int taskId) {
    Q_D(InferController);
    d->m_inferAcousticCacheProbeTasks.cancelIf(L_PRED(t, t->id() == taskId));
}

bool InferController::finishCurrentInferAcousticCacheProbeTask(InferAcousticCacheProbeTask *task) {
    Q_D(InferController);
    return d->m_inferAcousticCacheProbeTasks.onCurrentFinished(task);
}

void InferController::startPendingAcousticInference(const QList<Track *> &tracks) {
    Q_D(InferController);
    const QSet<Track *> trackSet(tracks.cbegin(), tracks.cend());
    for (const auto pipeline : std::as_const(d->m_inferPipelines)) {
        Track *owningTrack = nullptr;
        const auto clip =
            dynamic_cast<SingingClip *>(appModel->findClipById(pipeline->clipId(), owningTrack));
        if (!clip || !owningTrack)
            continue;
        if (!trackSet.isEmpty() && !trackSet.contains(owningTrack))
            continue;
        if (pipeline->piece().acousticInferStatus == Pending)
            pipeline->notifyPlaybackStarted();
    }
}

void InferController::suspendPendingAcousticInference(const QList<Track *> &tracks) {
    Q_D(InferController);
    const QSet<Track *> trackSet(tracks.cbegin(), tracks.cend());
    d->suspendPendingAcousticPipelines(trackSet);
}

void InferControllerPrivate::onModuleStatusChanged(const AppStatus::ModuleType module,
                                                   const AppStatus::ModuleStatus status) {
    if (module == AppStatus::ModuleType::Language)
        handleLanguageModuleStatusChanged(status);

    if ((module == AppStatus::ModuleType::Language || module == AppStatus::ModuleType::Inference ||
         module == AppStatus::ModuleType::Package) &&
        status == AppStatus::ModuleStatus::Ready)
        scheduleRetryAllSingingClips();
}

void InferControllerPrivate::onEditingChanged(const AppStatus::EditObjectType type) {
    if (type != AppStatus::EditObjectType::None) {
        qDebug() << "Editing project began" << "editObject:" << static_cast<int>(type)
                 << "hasEditSession:" << editSessionManager->hasActiveTransaction();
    } else {
        qDebug() << "Editing project finished"
                 << "lastEditObject:" << static_cast<int>(m_lastEditObjectType);
        editSessionManager->endActiveTransaction(EditSessionEndReason::Unknown);
    }
    m_lastEditObjectType = type;
}

void InferControllerPrivate::onEditSessionEnded(const EditSession &session,
                                                const EditSessionEndReason reason) {
    QTimer::singleShot(0, this, [this, session, reason] { flushPendingApplies(session, reason); });
}

void InferControllerPrivate::onInferOptionChanged(const AppOptionsGlobal::Option option) {
    if (option != AppOptionsGlobal::All && option != AppOptionsGlobal::Inference)
        return;

    m_autoStartAcousticInfer = appOptions->inference()->autoStartInfer;
    m_inferAcousticCacheProbeTasks.cancelAll();
}

void InferControllerPrivate::onPlaybackStatusChanged(const PlaybackGlobal::PlaybackStatus status) {
    if (status == PlaybackGlobal::Playing) {
        // Playing: only start pending pieces inside the lookahead window, instead of
        // enqueuing everything behind the playhead at once
        const auto pos = static_cast<double>(playbackController->position());
        refreshPlaybackWindow(pos);
    } else { // Paused / Stopped
        // Paused/Stopped: let the currently running acoustic task finish,
        // then put remaining Running/Pending pipelines back to probe-wait state
        // so they stop queueing
        suspendPendingAcousticPipelines();
    }
}

void InferControllerPrivate::onPlaybackPositionChanged(double tick) {
    // As the playhead advances, newly entering pending pieces are pulled into the window
    if (playbackController->playbackStatus() != PlaybackGlobal::Playing)
        return;
    refreshPlaybackWindow(tick);
}

void InferControllerPrivate::refreshPlaybackWindow(const double pos) {
    if (m_autoStartAcousticInfer)
        return; // auto-start mode bypasses playback-window scheduling

    // Lookahead window: covers [pos, pos + windowSeconds] in wall-clock time. Converted
    // to ticks via the timeline because inference engine operates on seconds, not ticks.
    const double windowTicks =
        appModel->timeline().secToTick(appOptions->inference()->playbackLookaheadSeconds);

    QList<InferPipeline *> inWindow;
    for (const auto pipeline : std::as_const(m_inferPipelines)) {
        if (pipeline->piece().acousticInferStatus != Pending)
            continue;
        const auto range = pieceGlobalRange(pipeline->clipId(), pipeline->pieceId());
        if (!range.isValid())
            continue;
        if (range.end <= pos || range.start >= pos + windowTicks)
            continue;
        inWindow.append(pipeline);
    }
    std::sort(inWindow.begin(), inWindow.end(), makePlaybackPriorityComparator<InferPipeline>());

    // Window holds a bounded set of pieces, so trigger them synchronously
    // rather than chaining QTimer::singleShot.
    for (const auto pipeline : inWindow) {
        if (m_inferPipelines.contains(pipeline) && pipeline->piece().acousticInferStatus == Pending)
            pipeline->notifyPlaybackStarted();
    }
}

void InferControllerPrivate::suspendPendingAcousticPipelines(const QSet<Track *> &tracks) {
    // Keep the currently running acoustic task and let it finish naturally;
    // the other Running/Pending pipelines get playbackSuspended back to probe state.
    const auto currentPieceId =
        m_inferAcousticTasks.current ? m_inferAcousticTasks.current->pieceId() : -1;
    for (const auto pipeline : std::as_const(m_inferPipelines)) {
        if (pipeline->pieceId() == currentPieceId)
            continue; // let the current piece finish in TaskQueue
        if (!tracks.isEmpty() && !tracks.contains(pipelineTrack(pipeline)))
            continue;
        const auto status = pipeline->piece().acousticInferStatus;
        if (status == Running || status == Pending)
            pipeline->notifyPlaybackSuspended();
    }
}

Track *InferControllerPrivate::pipelineTrack(const InferPipeline *pipeline) const {
    Track *owningTrack = nullptr;
    const auto clip =
        dynamic_cast<SingingClip *>(appModel->findClipById(pipeline->clipId(), owningTrack));
    if (!clip || !owningTrack)
        return nullptr;
    return owningTrack;
}

void InferControllerPrivate::handleModelChanged() {
    qInfo() << "Reset inference state for model change";
    syncSingerSessions();
    editSessionManager->clear();
    appStatus->currentEditObject = AppStatus::EditObjectType::None;
    clearAllPendingApplies("pending-cleared-model-changed");
    m_getPronTasks.cancelAll();
    m_getPhoneTasks.cancelAll();
    m_inferDurTasks.cancelAll();
    m_inferPitchTasks.cancelAll();
    m_inferVarianceTasks.cancelAll();
    m_inferAcousticTasks.cancelAll();
    m_inferAcousticCacheProbeTasks.cancelAll();
    for (const auto pipeline : std::as_const(m_inferPipelines))
        delete pipeline;
    m_inferPipelines.clear();
    m_retryAllScheduled = false;
}

void InferControllerPrivate::handleTempoChanged() {
    const auto &ranges = tempoChangeRanges();
    if (ranges.isEmpty())
        return;

    const auto &timeline = appModel->timeline();
    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() != IClip::Singing)
                continue;
            const auto singingClip = static_cast<SingingClip *>(clip);
            const int clipStart = singingClip->start() + singingClip->clipStart();
            const int clipEnd = clipStart + singingClip->clipLen();
            if (!intersectsTempoRanges(ranges, clipStart, clipEnd) ||
                singingClip->pieces().isEmpty())
                continue;

            Automation::InferenceMutationRequest resegmentRequest;
            resegmentRequest.kind = Automation::InferenceMutationKind::ResegmentClip;
            resegmentRequest.clipId = Automation::ClipId(singingClip->id());
            resegmentRequest.bumpClipInferenceRevision = false;
            const auto resegment = InferenceAutomationBridge::executeCurrent(resegmentRequest);
            if (!resegment) {
                qWarning() << "Failed to resegment clip after tempo change:"
                           << resegment.getError().message;
                continue;
            }

            // Only surviving pieces that overlap an effective BPM change need
            // their state machines restarted. Unrelated pieces keep their
            // current result and in-flight state.
            const auto pipelines = Linq::where(
                m_inferPipelines, [singingClip, &ranges, &timeline](const InferPipeline *pipeline) {
                    if (pipeline->clipId() != singingClip->id())
                        return false;
                    const auto &piece = pipeline->piece();
                    const int start = singingClip->start() + piece.localStartTick(timeline);
                    const int end = singingClip->start() + piece.localEndTick(timeline);
                    return intersectsTempoRanges(ranges, start, end);
                });
            Automation::InferenceMutationRequest refreshRequest;
            refreshRequest.kind = Automation::InferenceMutationKind::RefreshSpeakerMix;
            refreshRequest.clipId = Automation::ClipId(singingClip->id());
            for (const auto *pipeline : pipelines)
                refreshRequest.pieceIds.append(Automation::PieceId(pipeline->pieceId()));
            if (!pipelines.isEmpty()) {
                const auto refreshed = InferenceAutomationBridge::executeCurrent(refreshRequest);
                if (!refreshed) {
                    qWarning() << "Failed to refresh inference speaker mix after tempo change:"
                               << refreshed.getError().message;
                    continue;
                }
            }
            for (const auto pipeline : pipelines)
                pipeline->onTimelineChanged();

            for (const auto pieceId : resegment.get().sideEffects.addedPieces) {
                if (auto *piece = singingClip->findPieceById(pieceId.value()))
                    createPipeline(*piece);
            }
        }
    }
}

void InferControllerPrivate::handleTrackInserted(Track *track) {
    ModelChangeHandler::handleTrackInserted(track);
    connect(track, &Track::voiceContextChanged, this, [this] { syncSingerSessions(); });
    syncSingerSessions();
}

void InferControllerPrivate::handleTrackRemoved(Track *track) {
    disconnect(track, &Track::voiceContextChanged, this, nullptr);
    ModelChangeHandler::handleTrackRemoved(track);
    syncSingerSessions();
}

void InferControllerPrivate::handleSingingClipInserted(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipInserted(clip);
    syncSingerSessions();

    if (!clip->pieces().isEmpty()) {
        // Cross-track move: keep existing pipelines only when their singer/speaker context is
        // still valid. Follow Track clips can inherit a different singer/speaker on the target
        // track and must be re-inferred.
        if (!clipPiecesMatchCurrentSingerAndSpeaker(*clip)) {
            Automation::InferenceMutationRequest request;
            request.kind = Automation::InferenceMutationKind::InvalidateClip;
            request.clipId = Automation::ClipId(clip->id());
            const auto invalidated = InferenceAutomationBridge::executeCurrent(request);
            if (!invalidated) {
                qWarning() << "Failed to invalidate moved singing clip:"
                           << invalidated.getError().message;
                return;
            }
            ensureClipInferenceStarted(*clip);
        }
        return;
    }

    // Trigger inference if language module is already ready and clip has a valid singer
    ensureClipInferenceStarted(*clip);
}

void InferControllerPrivate::handleSingingClipRemoved(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipRemoved(clip);
    syncSingerSessions();

    if (appModel->findClipById(clip->id())) {
        // Cross-track move: inference pipelines continue running,
        // don't cancel tasks or delete pipelines
        return;
    }

    cancelClipRelatedTasks(clip);
    // Remove related pipelines
    auto pipelines = Linq::where(m_inferPipelines, [clip](const InferPipeline *p) {
        return p->piece().clipId() == clip->id();
    });
    for (const auto &pipeline : pipelines) {
        m_inferPipelines.removeOne(pipeline);
        delete pipeline;
    }
}

void InferControllerPrivate::handlePiecesChanged(const PieceList &newPieces,
                                                 const PieceList &discardedPieces,
                                                 SingingClip *clip) {
    for (const auto &piece : discardedPieces) {
        cancelPieceRelatedTasks(piece->id());
        auto pipelines = Linq::where(m_inferPipelines, [piece](const InferPipeline *p) {
            return p->pieceId() == piece->id();
        });
        for (const auto &pipeline : pipelines) {
            m_inferPipelines.removeOne(pipeline);
            delete pipeline;
        }
    }
    Automation::InferenceMutationRequest request;
    request.kind = Automation::InferenceMutationKind::RebuildOriginalParams;
    request.clipId = Automation::ClipId(clip->id());
    const auto rebuilt = InferenceAutomationBridge::executeCurrent(request);
    if (!rebuilt)
        qWarning() << "Failed to rebuild original inference parameters:"
                   << rebuilt.getError().message;
}

void InferControllerPrivate::handleNoteChanged(const SingingClip::NoteChangeType type,
                                               const QList<Note *> &notes, SingingClip *clip) {
    switch (type) {
        case SingingClip::Remove:
            for (const auto &piece : clip->findPiecesByNotes(notes))
                piece->dirty = true;

            // If all notes are removed, clear all pieces directly
            if (clip->notes().count() <= 0) {
                Automation::InferenceMutationRequest request;
                request.kind = Automation::InferenceMutationKind::InvalidateClip;
                request.clipId = Automation::ClipId(clip->id());
                const auto invalidated = InferenceAutomationBridge::executeCurrent(request);
                if (!invalidated)
                    qWarning() << "Failed to invalidate empty singing clip:"
                               << invalidated.getError().message;
                return;
            }
            ensureClipInferenceStarted(*clip);
            break;
        case SingingClip::Insert:
        case SingingClip::EditedWordPropertyChange:
        case SingingClip::EditedPhonemeOffsetChange:
        case SingingClip::TimeKeyPropertyChange:
            for (const auto &piece : clip->findPiecesByNotes(notes)) {
                piece->dirty = true;
            }
            // TODO 重跑获取发音->音素，跑之前先判断发音序列？
            ensureClipInferenceStarted(*clip);
            break;
        case SingingClip::EditedPronunciationOnly:
            // User edited pronunciation after G2P has already run.
            // Skip G2P and only re-run S2P to get the new phoneme sequence.
            for (const auto &piece : clip->findPiecesByNotes(notes)) {
                piece->dirty = true;
            }
            ensureClipInferenceStarted(*clip, ClipInferenceStartStage::Phoneme);
            break;
        default:
            break;
    } // Ignore original word property change
}

void InferControllerPrivate::handleParamChanged(const ParamInfo::Name name, const Param::Type type,
                                                SingingClip *clip) {
    if (type != Param::Edited)
        return;
    Automation::InferenceMutationRequest request;
    request.kind = Automation::InferenceMutationKind::RefreshParamInput;
    request.clipId = Automation::ClipId(clip->id());
    request.parameterName = name;
    const auto refreshed = InferenceAutomationBridge::executeCurrent(request);
    if (!refreshed) {
        qWarning() << "Failed to refresh inference parameter input:"
                   << refreshed.getError().message;
        return;
    }
    QList<InferPiece *> dirtyPieces;
    for (const auto pieceId : refreshed.get().sideEffects.changedPieces) {
        if (auto *piece = clip->findPieceById(pieceId.value()))
            dirtyPieces.append(piece);
    }
    switch (name) {
        case ParamInfo::Expressiveness:
            for (const auto &piece : dirtyPieces) {
                auto pred = L_PRED(t, t->pieceId() == piece->id());
                m_inferPitchTasks.cancelIf(pred);
                m_inferVarianceTasks.cancelIf(pred);
                m_inferAcousticTasks.cancelIf(pred);
                m_inferAcousticCacheProbeTasks.cancelIf(pred);

                auto pipelines = Linq::where(m_inferPipelines, [piece](const InferPipeline *p) {
                    return p->pieceId() == piece->id();
                });
                Q_ASSERT(pipelines.size() == 1);
                pipelines.first()->onExpressivenessChanged();
            }
            break;
        case ParamInfo::Pitch:
        case ParamInfo::ToneShift:
            for (const auto &piece : dirtyPieces) {
                auto pred = L_PRED(t, t->pieceId() == piece->id());
                m_inferVarianceTasks.cancelIf(pred);
                m_inferAcousticTasks.cancelIf(pred);
                m_inferAcousticCacheProbeTasks.cancelIf(pred);

                auto pipelines = Linq::where(m_inferPipelines, [piece](const InferPipeline *p) {
                    return p->pieceId() == piece->id();
                });
                Q_ASSERT(pipelines.size() == 1);
                pipelines.first()->onPitchChanged();
            }
            break;
        case ParamInfo::Energy:
        case ParamInfo::Breathiness:
        case ParamInfo::Voicing:
        case ParamInfo::Tension:
        case ParamInfo::MouthOpening:
        case ParamInfo::Gender:
        case ParamInfo::Velocity:
            for (const auto &piece : dirtyPieces) {
                auto pred = L_PRED(t, t->pieceId() == piece->id());
                m_inferAcousticTasks.cancelIf(pred);
                m_inferAcousticCacheProbeTasks.cancelIf(pred);

                auto pipelines = Linq::where(m_inferPipelines, [piece](const InferPipeline *p) {
                    return p->pieceId() == piece->id();
                });
                Q_ASSERT(pipelines.size() == 1);
                pipelines.first()->onVarianceChanged();
            }
            break;
        case ParamInfo::SpeakerMix:
            qCCritical(logInferController) << "Speaker mix is not a regular param";
            break;
        case ParamInfo::Unknown:
            qCCritical(logInferController) << "Unknown param";
            break;
    }
}

void InferControllerPrivate::handleVoiceContextChanged(const VoiceContextChange &change,
                                                       SingingClip *clip) {
    if (!clip)
        return;

    syncSingerSessions();
    const bool singerChanged = change.before.singer != change.after.singer;
    const bool speakerChanged = change.before.speaker != change.after.speaker;
    if (singerChanged || speakerChanged) {
        Automation::InferenceMutationRequest request;
        request.kind = Automation::InferenceMutationKind::InvalidateClip;
        request.clipId = Automation::ClipId(clip->id());
        const auto invalidated = InferenceAutomationBridge::executeCurrent(request);
        if (!invalidated) {
            qWarning() << "Failed to invalidate inference after voice change:"
                       << invalidated.getError().message;
            return;
        }
        ensureClipInferenceStarted(*clip);
        return;
    }

    if (change.before.speakerMix == change.after.speakerMix)
        return;

    if (clip->pieces().isEmpty()) {
        ensureClipInferenceStarted(*clip);
        return;
    }

    Automation::InferenceMutationRequest refreshRequest;
    refreshRequest.kind = Automation::InferenceMutationKind::RefreshSpeakerMix;
    refreshRequest.clipId = Automation::ClipId(clip->id());
    const auto refreshed = InferenceAutomationBridge::executeCurrent(refreshRequest);
    if (!refreshed) {
        qWarning() << "Failed to refresh inference speaker mix:" << refreshed.getError().message;
        return;
    }

    for (const auto piece : clip->pieces()) {
        auto pred = L_PRED(t, t->pieceId() == piece->id());
        m_inferPitchTasks.cancelIf(pred);
        m_inferVarianceTasks.cancelIf(pred);
        m_inferAcousticTasks.cancelIf(pred);
        m_inferAcousticCacheProbeTasks.cancelIf(pred);

        auto pipelines = Linq::where(m_inferPipelines, [piece](const InferPipeline *p) {
            return p->pieceId() == piece->id();
        });
        if (pipelines.isEmpty()) {
            createPipeline(*piece);
            continue;
        }
        for (const auto pipeline : pipelines)
            pipeline->onExpressivenessChanged();
    }
}

void InferControllerPrivate::syncSingerSessions() {
    QSet<SingerIdentifier> identifiers;
    for (const auto track : appModel->tracks()) {
        const auto trackIdentifier = track->singerIdentifier();
        if (!trackIdentifier.isEmpty()) {
            identifiers.insert(trackIdentifier);
        }
        for (const auto clip : track->clips()) {
            if (clip->clipType() != IClip::Singing) {
                continue;
            }
            const auto identifier = static_cast<SingingClip *>(clip)->singerIdentifier();
            if (!identifier.isEmpty()) {
                identifiers.insert(identifier);
            }
        }
    }
    inferEngine->retainSingerSessions(identifiers);
}

void InferControllerPrivate::handleLanguageModuleStatusChanged(
    const AppStatus::ModuleStatus status) {
    if (status == AppStatus::ModuleStatus::Ready) {
        qDebug() << "Language module is ready. Tasks will be started.";
    } else if (status == AppStatus::ModuleStatus::Error) {
        clearAllPendingApplies("pending-cleared-module-error");
        m_getPronTasks.disposePendingTasks();
        if (auto *runtime = AppContext::instance<Automation::CoreRuntime>()) {
            runtime->settings().updateG2pLanguage({}, {});
        }
        qCritical() << "Failed to start the language module; tasks have been canceled.";
    }
}

bool InferControllerPrivate::allRequiredModulesReady() const {
    return appStatus->languageModuleStatus == AppStatus::ModuleStatus::Ready &&
           appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready &&
           appStatus->inferEngineEnvStatus == AppStatus::ModuleStatus::Ready;
}

bool InferControllerPrivate::canStartClipInference(const SingingClip &clip) const {
    return allRequiredModulesReady() && !clip.singerInfo().isEmpty() &&
           !clip.singerIdentifier().isEmpty();
}

void InferControllerPrivate::ensureClipInferenceStarted(SingingClip &clip,
                                                        const ClipInferenceStartStage stage) {
    const QPointer<SingingClip> guardedClip(&clip);
    // Model signals run inside ActionSequence::execute(); defer until the committer advances
    // revision.
    QTimer::singleShot(0, this, [this, guardedClip, stage] {
        if (!guardedClip || appModel->findClipById(guardedClip->id()) != guardedClip)
            return;

        if (!canStartClipInference(*guardedClip))
            return;

        if (stage == ClipInferenceStartStage::Pronunciation)
            createAndRunGetPronTask(*guardedClip);
        else
            createAndRunGetPhoneTask(*guardedClip);
    });
}

void InferControllerPrivate::scheduleRetryAllSingingClips() {
    if (m_retryAllScheduled)
        return;

    m_retryAllScheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_retryAllScheduled = false;
        retryAllSingingClips();
    });
}

void InferControllerPrivate::retryAllSingingClips() {
    if (!allRequiredModulesReady())
        return;

    qDebug() << "Inference dependencies are ready. Retrying singing clips.";
    for (const auto track : appModel->tracks()) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() == IClip::Singing)
                createAndRunGetPronTask(*static_cast<SingingClip *>(clip));
        }
    }
}

void InferControllerPrivate::handleGetPronTaskFinished(GetPronunciationTask &task) {
    if (!m_getPronTasks.isCurrent(&task))
        return;

    const auto context = buildClipTaskContext("pronunciation", task);
    if (task.terminated()) {
        InferenceApplyGate::logDrop(context, "clip-task", "task-terminated");
        m_getPronTasks.onCurrentFinished(&task);
        return;
    }

    InferenceApplyGate::Options options;
    options.phase = "clip-task";
    options.expectedNoteCount = task.result.count();
    options.requirePiece = false;
    options.requireNotesInPiece = false;
    options.checkSingerSpeaker = false;
    options.checkEditSession = true;

    InferenceTaskResolution resolution;
    switch (InferenceApplyGate::resolve(context, resolution, options)) {
        case InferenceApplyGate::Decision::Apply: {
            const auto applied = InferenceAutomationBridge::executeAfterGate(
                context.documentVersion, pronunciationMutation(context, task.result));
            if (!applied) {
                InferenceApplyGate::logDrop(
                    context, "clip-task",
                    InferenceAutomationBridge::dropReason(applied.getError()));
                scheduleRetryAllSingingClips();
                break;
            }
            if (!resolution.clip->singerInfo().isEmpty())
                createAndRunGetPhoneTask(*resolution.clip);
            break;
        }
        case InferenceApplyGate::Decision::Defer:
            storePendingPronunciationApply(context, task.result);
            break;
        case InferenceApplyGate::Decision::Drop:
            break;
    }
    m_getPronTasks.onCurrentFinished(&task);
}

// TODO 任何音符改动，都会触发获取剪辑所有音符发音->获取剪辑所有音符音素名称
// TODO
// 对于连续的多个音符，如果其中有音符缺少音素名称信息（发音有误等原因导致），则整句将在划分时忽略
// TODO 对于以-开头的连续多个音符，同样被忽略
// TODO 分段结果确保为多个有效片段
void InferControllerPrivate::handleGetPhoneTaskFinished(GetPhonemeNameTask &task) {
    if (!m_getPhoneTasks.isCurrent(&task))
        return;

    const auto context = buildClipTaskContext("phoneme-name", task);
    if (task.terminated()) {
        InferenceApplyGate::logDrop(context, "clip-task", "task-terminated");
        m_getPhoneTasks.onCurrentFinished(&task);
        return;
    }

    InferenceApplyGate::Options options;
    options.phase = "clip-task";
    options.expectedNoteCount = task.result.count();
    options.requirePiece = false;
    options.requireNotesInPiece = false;
    options.checkSingerSpeaker = false;
    options.checkEditSession = true;

    InferenceTaskResolution resolution;
    switch (InferenceApplyGate::resolve(context, resolution, options)) {
        case InferenceApplyGate::Decision::Apply: {
            const auto applied = InferenceAutomationBridge::executeAfterGate(
                context.documentVersion, phonemeNameMutation(context, task.result));
            if (!applied) {
                InferenceApplyGate::logDrop(
                    context, "clip-task",
                    InferenceAutomationBridge::dropReason(applied.getError()));
                scheduleRetryAllSingingClips();
                break;
            }
            for (const auto pieceId : applied.get().sideEffects.addedPieces) {
                if (auto *piece = resolution.clip->findPieceById(pieceId.value()))
                    createPipeline(*piece);
            }
            break;
        }
        case InferenceApplyGate::Decision::Defer:
            storePendingPhonemeNameApply(context, task.result);
            break;
        case InferenceApplyGate::Decision::Drop:
            break;
    }
    m_getPhoneTasks.onCurrentFinished(&task);
}

InferControllerPrivate::PendingApplyResult InferControllerPrivate::tryApplyPronunciation(
    const InferenceTaskContext &context, const QList<PronunciationFetchResult> &pronunciations,
    const QString &phase) {
    InferenceApplyGate::Options options;
    options.phase = phase;
    options.expectedNoteCount = pronunciations.count();
    options.requirePiece = false;
    options.requireNotesInPiece = false;
    options.checkSingerSpeaker = false;
    options.checkEditSession = true;

    InferenceTaskResolution resolution;
    switch (InferenceApplyGate::resolve(context, resolution, options)) {
        case InferenceApplyGate::Decision::Apply: {
            const auto applied = InferenceAutomationBridge::executeAfterGate(
                context.documentVersion, pronunciationMutation(context, pronunciations));
            if (!applied) {
                InferenceApplyGate::logDrop(
                    context, phase, InferenceAutomationBridge::dropReason(applied.getError()));
                scheduleRetryAllSingingClips();
                return PendingApplyResult::Dropped;
            }
            InferenceApplyGate::logDecision(context, phase, InferenceApplyGate::Decision::Apply,
                                            phase == "pending-flush" ? "edit-session-flush-apply"
                                                                     : "clip-task-apply",
                                            resolution.clip->inferenceRevision());
            if (!resolution.clip->singerInfo().isEmpty())
                createAndRunGetPhoneTask(*resolution.clip);
            return PendingApplyResult::Applied;
        }
        case InferenceApplyGate::Decision::Drop:
            if (phase == "pending-flush") {
                const auto reason =
                    resolution.dropReason == "revision-mismatch"
                        ? "edit-session-flush-drop-revision-mismatch"
                        : QString("edit-session-flush-drop-%1").arg(resolution.dropReason);
                InferenceApplyGate::logDecision(context, phase, InferenceApplyGate::Decision::Drop,
                                                reason);
            }
            return PendingApplyResult::Dropped;
        case InferenceApplyGate::Decision::Defer:
            if (phase == "pending-flush")
                InferenceApplyGate::logDecision(context, phase, InferenceApplyGate::Decision::Defer,
                                                "edit-session-flush-defer");
            return PendingApplyResult::Deferred;
    }
    return PendingApplyResult::Dropped;
}

InferControllerPrivate::PendingApplyResult
    InferControllerPrivate::tryApplyPhonemeName(const InferenceTaskContext &context,
                                                const QList<PhonemeNameResult> &phonemeNames,
                                                const QString &phase) {
    InferenceApplyGate::Options options;
    options.phase = phase;
    options.expectedNoteCount = phonemeNames.count();
    options.requirePiece = false;
    options.requireNotesInPiece = false;
    options.checkSingerSpeaker = false;
    options.checkEditSession = true;

    InferenceTaskResolution resolution;
    switch (InferenceApplyGate::resolve(context, resolution, options)) {
        case InferenceApplyGate::Decision::Apply: {
            const auto applied = InferenceAutomationBridge::executeAfterGate(
                context.documentVersion, phonemeNameMutation(context, phonemeNames));
            if (!applied) {
                InferenceApplyGate::logDrop(
                    context, phase, InferenceAutomationBridge::dropReason(applied.getError()));
                scheduleRetryAllSingingClips();
                return PendingApplyResult::Dropped;
            }
            InferenceApplyGate::logDecision(context, phase, InferenceApplyGate::Decision::Apply,
                                            phase == "pending-flush" ? "edit-session-flush-apply"
                                                                     : "clip-task-apply",
                                            resolution.clip->inferenceRevision());
            for (const auto pieceId : applied.get().sideEffects.addedPieces) {
                if (auto *piece = resolution.clip->findPieceById(pieceId.value()))
                    createPipeline(*piece);
            }
            return PendingApplyResult::Applied;
        }
        case InferenceApplyGate::Decision::Drop:
            if (phase == "pending-flush") {
                const auto reason =
                    resolution.dropReason == "revision-mismatch"
                        ? "edit-session-flush-drop-revision-mismatch"
                        : QString("edit-session-flush-drop-%1").arg(resolution.dropReason);
                InferenceApplyGate::logDecision(context, phase, InferenceApplyGate::Decision::Drop,
                                                reason);
            }
            return PendingApplyResult::Dropped;
        case InferenceApplyGate::Decision::Defer:
            if (phase == "pending-flush")
                InferenceApplyGate::logDecision(context, phase, InferenceApplyGate::Decision::Defer,
                                                "edit-session-flush-defer");
            return PendingApplyResult::Deferred;
    }
    return PendingApplyResult::Dropped;
}

void InferControllerPrivate::storePendingPronunciationApply(
    const InferenceTaskContext &context, const QList<PronunciationFetchResult> &pronunciations) {
    const bool replaced = m_pendingPronunciationApplies.contains(context.clipId);
    m_pendingPronunciationApplies.insert(context.clipId, {context, pronunciations});
    InferenceApplyGate::logDecision(context, "pending-store", InferenceApplyGate::Decision::Defer,
                                    replaced ? "pending-replaced" : "pending-added");
}

void InferControllerPrivate::storePendingPhonemeNameApply(
    const InferenceTaskContext &context, const QList<PhonemeNameResult> &phonemeNames) {
    const bool replaced = m_pendingPhonemeNameApplies.contains(context.clipId);
    m_pendingPhonemeNameApplies.insert(context.clipId, {context, phonemeNames});
    InferenceApplyGate::logDecision(context, "pending-store", InferenceApplyGate::Decision::Defer,
                                    replaced ? "pending-replaced" : "pending-added");
}

void InferControllerPrivate::flushPendingApplies(const EditSession &session,
                                                 const EditSessionEndReason reason) {
    Q_UNUSED(session)
    Q_UNUSED(reason)

    const auto pronunciationKeys = m_pendingPronunciationApplies.keys();
    for (const auto clipId : pronunciationKeys) {
        if (!m_pendingPronunciationApplies.contains(clipId))
            continue;
        const auto pending = m_pendingPronunciationApplies.value(clipId);
        const auto result =
            tryApplyPronunciation(pending.context, pending.pronunciations, "pending-flush");
        if (result != PendingApplyResult::Deferred)
            m_pendingPronunciationApplies.remove(clipId);
    }

    const auto phonemeNameKeys = m_pendingPhonemeNameApplies.keys();
    for (const auto clipId : phonemeNameKeys) {
        if (!m_pendingPhonemeNameApplies.contains(clipId))
            continue;
        const auto pending = m_pendingPhonemeNameApplies.value(clipId);
        const auto result =
            tryApplyPhonemeName(pending.context, pending.phonemeNames, "pending-flush");
        if (result != PendingApplyResult::Deferred)
            m_pendingPhonemeNameApplies.remove(clipId);
    }
}

void InferControllerPrivate::clearAllPendingApplies(const QString &reason) {
    for (const auto &pending : std::as_const(m_pendingPronunciationApplies)) {
        InferenceApplyGate::logDecision(pending.context, "pending-clear",
                                        InferenceApplyGate::Decision::Drop, reason);
    }
    for (const auto &pending : std::as_const(m_pendingPhonemeNameApplies)) {
        InferenceApplyGate::logDecision(pending.context, "pending-clear",
                                        InferenceApplyGate::Decision::Drop, reason);
    }
    m_pendingPronunciationApplies.clear();
    m_pendingPhonemeNameApplies.clear();
}

void InferControllerPrivate::clearPendingForClip(const int clipId, const QString &reason) {
    if (m_pendingPronunciationApplies.contains(clipId)) {
        InferenceApplyGate::logDecision(m_pendingPronunciationApplies.value(clipId).context,
                                        "pending-clear", InferenceApplyGate::Decision::Drop,
                                        reason);
        m_pendingPronunciationApplies.remove(clipId);
    }
    if (m_pendingPhonemeNameApplies.contains(clipId)) {
        InferenceApplyGate::logDecision(m_pendingPhonemeNameApplies.value(clipId).context,
                                        "pending-clear", InferenceApplyGate::Decision::Drop,
                                        reason);
        m_pendingPhonemeNameApplies.remove(clipId);
    }
}

void InferControllerPrivate::createAndRunGetPronTask(const SingingClip &clip) {
    if (!canStartClipInference(clip))
        return;

    if (clip.notes().count() <= 0) {
        qDebug() << "createAndRunGetPhoneTask:"
                 << "Note list is empty";
        return;
    }
    const auto clipId = clip.id();
    clearPendingForClip(clipId, "pending-cleared-new-task");
    auto pred = [clipId](const auto t) { return t->clipId() == clipId; };
    m_getPronTasks.cancelIf(pred);
    m_getPhoneTasks.cancelIf(pred);

    auto task = new GetPronunciationTask(InferenceAutomationBridge::currentDocumentVersion(),
                                         clip.id(), clip.inferenceRevision(),
                                         buildNoteInferenceSnapshots(clip), clip.singerInfo());
    connect(task, &Task::finished, this, [task, this] { handleGetPronTaskFinished(*task); });
    m_getPronTasks.add(task);
}

void InferControllerPrivate::createAndRunGetPhoneTask(const SingingClip &clip) {
    if (!canStartClipInference(clip))
        return;

    const auto clipId = clip.id();
    if (m_pendingPhonemeNameApplies.contains(clipId)) {
        InferenceApplyGate::logDecision(m_pendingPhonemeNameApplies.value(clipId).context,
                                        "pending-clear", InferenceApplyGate::Decision::Drop,
                                        "pending-cleared-new-task");
        m_pendingPhonemeNameApplies.remove(clipId);
    }
    m_getPhoneTasks.cancelIf([clipId](const auto t) { return t->clipId() == clipId; });

    auto task = new GetPhonemeNameTask(InferenceAutomationBridge::currentDocumentVersion(),
                                       clip.id(), clip.inferenceRevision(),
                                       buildNoteInferenceSnapshots(clip), clip.singerInfo());
    connect(task, &Task::finished, this, [task, this] { handleGetPhoneTaskFinished(*task); });
    m_getPhoneTasks.add(task);
}

void InferControllerPrivate::createPipeline(InferPiece &piece) {
    if (!piece.clip || !canStartClipInference(*piece.clip))
        return;

    // Keep one live state machine per piece. Dropped/final machines are removed before a fresh
    // one is allowed to observe later model events.
    const auto duplicatePipelines = Linq::where(
        m_inferPipelines, [&piece](const InferPipeline *p) { return p->pieceId() == piece.id(); });
    for (const auto pipeline : duplicatePipelines) {
        m_inferPipelines.removeOne(pipeline);
        pipeline->deleteLater();
    }

    auto pipeline = new InferPipeline(piece);
    m_inferPipelines.append(pipeline);
    connect(pipeline, &InferPipeline::dropped, this,
            [this, pipeline](const QString &reason, int, const QString &) {
                handlePipelineDropped(pipeline, reason);
            });
    pipeline->run();
}

void InferControllerPrivate::handlePipelineDropped(InferPipeline *pipeline, const QString &reason) {
    if (!pipeline)
        return;

    const QPointer<InferPipeline> guardedPipeline(pipeline);
    const auto clipId = pipeline->clipId();
    const auto pieceId = pipeline->pieceId();
    QTimer::singleShot(0, this, [this, guardedPipeline, clipId, pieceId, reason] {
        if (!guardedPipeline || !m_inferPipelines.contains(guardedPipeline.data()))
            return;

        m_inferPipelines.removeOne(guardedPipeline.data());
        guardedPipeline->deleteLater();

        // A stale task can be rejected while the piece still needs inference with a fresh snapshot.
        if (reason != "input-signature-mismatch" && reason != "document-revision-mismatch")
            return;

        const auto clip = dynamic_cast<SingingClip *>(appModel->findClipById(clipId));
        if (!clip || !canStartClipInference(*clip))
            return;
        const auto piece = clip->findPieceById(pieceId);
        if (!piece)
            return;
        createPipeline(*piece);
    });
}

void InferControllerPrivate::reset() {
    clearAllPendingApplies("pending-cleared-reset");
    m_getPronTasks.cancelAll();
    m_getPhoneTasks.cancelAll();
    m_inferDurTasks.cancelAll();
    m_inferPitchTasks.cancelAll();
    m_inferVarianceTasks.cancelAll();
    m_inferAcousticTasks.cancelAll();
    m_inferAcousticCacheProbeTasks.cancelAll();
}

void InferControllerPrivate::cancelAllInferTasks() {
    for (const auto &track : appModel->tracks())
        for (const auto &clip : track->clips()) {
            if (clip->clipType() != IClip::Singing)
                continue;
            const auto singingClip = static_cast<SingingClip *>(clip);
            this->cancelClipRelatedTasks(singingClip);
        }
}

void InferControllerPrivate::cancelClipRelatedTasks(const SingingClip *clip) {
    qInfo() << "Cancel singing-clip related tasks" << "clipId:" << clip->id();
    clearPendingForClip(clip->id(), "pending-cleared-clip-removed");
    auto pred = L_PRED(t, t->clipId() == clip->id());
    m_getPronTasks.cancelIf(pred);
    m_getPhoneTasks.cancelIf(pred);
    for (const auto piece : clip->pieces())
        cancelPieceRelatedTasks(piece->id());
}

void InferControllerPrivate::cancelPieceRelatedTasks(int pieceId) {
    qInfo() << "Cancel infer-piece related tasks" << "pieceId:" << pieceId;
    auto pred = L_PRED(t, t->pieceId() == pieceId);
    m_inferDurTasks.cancelIf(pred);
    m_inferPitchTasks.cancelIf(pred);
    m_inferVarianceTasks.cancelIf(pred);
    m_inferAcousticTasks.cancelIf(pred);
    m_inferAcousticCacheProbeTasks.cancelIf(pred);
}
