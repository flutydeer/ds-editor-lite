//
// Created by OrangeCat on 24-9-3.
//

#include "InferController.h"
#include "InferController_p.h"

#include "InferControllerHelper.h"
#include "Model/InferenceData/InferPiece.h"
#include "Model/AppModel/Note.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/InferenceData/InferSpeakerMix.h"
#include "Models/NoteInferenceSnapshot.h"
#include "Tasks/GetPhonemeNameTask.h"
#include "Tasks/GetPronunciationTask.h"
#include "Utils/InferenceApplyGate.h"
#include <lite/Support/Linq.h>
#include "Utils/ValidationUtils.h"
#include "Controller/PlaybackController.h"
#include "InferPipeline.h"
#include "Model/AppModel/AppModel.h"
#include "Modules/Inference/EditSessionManager.h"

#include <QTimer>
#include <QPointer>

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
        return {piece->localStartTick() + clip->start(), piece->localEndTick() + clip->start()};
    }

    QList<NoteInferenceSnapshot> buildNoteInferenceSnapshots(const SingingClip &clip) {
        QList<NoteInferenceSnapshot> result;
        result.reserve(clip.notes().count());
        for (const auto note : clip.notes()) {
            NoteInferenceSnapshot snapshot;
            snapshot.noteId = note->id();
            snapshot.lyric = note->lyric();
            snapshot.language = note->language();
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
        const Timeline timeline{{{0, appModel->tempo()}}};
        for (const auto piece : clip.pieces()) {
            const auto speakerMix = InferSpeakerMixModel::effectiveSpeakerMixFromData(
                clip.speakerMixData(), clip.speakerId(), piece->localStartTick(),
                piece->localEndTick(), timeline);
            if (piece->identifier != identifier || piece->speakerMix != speakerMix)
                return false;
        }
        return true;
    }

    template <typename T>
    InferenceTaskContext buildClipTaskContext(const QString &taskType, const T &task) {
        InferenceTaskContext context;
        context.taskType = taskType;
        context.taskId = task.id();
        context.clipId = task.clipId();
        context.clipRevision = task.clipRevision();
        context.noteIds = task.noteIds();
        return context;
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
}

InferController::~InferController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(InferController)

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
        auto awaitingPipelines = Linq::where(m_inferPipelines, [](const InferPipeline *p) {
            return p->piece().acousticInferStatus == Pending;
        });

        std::sort(awaitingPipelines.begin(), awaitingPipelines.end(),
                  makePlaybackPriorityComparator<InferPipeline>());

        notifyNextPipeline(awaitingPipelines, 0);
    }
}

void InferControllerPrivate::handleModelChanged() {
    qInfo() << "Reset inference state for model change";
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

void InferControllerPrivate::handleTempoChanged(double tempo) {
    reset();
    recreateAllInferTasks();
}

void InferControllerPrivate::handleSingingClipInserted(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipInserted(clip);

    if (!clip->pieces().isEmpty()) {
        // Cross-track move: keep existing pipelines only when their singer/speaker context is
        // still valid. Follow Track clips can inherit a different singer/speaker on the target
        // track and must be re-inferred.
        if (!clipPiecesMatchCurrentSingerAndSpeaker(*clip)) {
            clip->removeAllPieces();
            if (canStartClipInference(*clip))
                createAndRunGetPronTask(*clip);
        }
        return;
    }

    // Trigger inference if language module is already ready and clip has a valid singer
    if (canStartClipInference(*clip)) {
        createAndRunGetPronTask(*clip);
    }
}

void InferControllerPrivate::handleSingingClipRemoved(SingingClip *clip) {
    ModelChangeHandler::handleSingingClipRemoved(clip);

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
    Helper::updateAllOriginalParam(*clip);
}

void InferControllerPrivate::handleNoteChanged(const SingingClip::NoteChangeType type,
                                               const QList<Note *> &notes, SingingClip *clip) {
    switch (type) {
        case SingingClip::Remove:
            for (const auto &piece : clip->findPiecesByNotes(notes))
                piece->dirty = true;

            // If all notes are removed, clear all pieces directly
            if (clip->notes().count() <= 0) {
                clip->removeAllPieces();
                return;
            }
            if (canStartClipInference(*clip))
                createAndRunGetPronTask(*clip);
            break;
        case SingingClip::Insert:
        case SingingClip::EditedWordPropertyChange:
        case SingingClip::EditedPhonemeOffsetChange:
        case SingingClip::TimeKeyPropertyChange:
            for (const auto &piece : clip->findPiecesByNotes(notes)) {
                piece->dirty = true;
            }
            // TODO 重跑获取发音->音素，跑之前先判断发音序列？
            if (canStartClipInference(*clip))
                createAndRunGetPronTask(*clip);
            break;
        case SingingClip::EditedPronunciationOnly:
            // User edited pronunciation after G2P has already run.
            // Skip G2P and only re-run S2P to get the new phoneme sequence.
            for (const auto &piece : clip->findPiecesByNotes(notes)) {
                piece->dirty = true;
            }
            if (canStartClipInference(*clip))
                createAndRunGetPhoneTask(*clip);
            break;
        default:
            break;
    } // Ignore original word property change
}

void InferControllerPrivate::handleParamChanged(const ParamInfo::Name name, const Param::Type type,
                                                SingingClip *clip) {
    if (type != Param::Edited)
        return;
    auto dirtyPieces = Helper::getParamDirtyPiecesAndUpdateInput(name, *clip);
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
        case ParamInfo::ToneShift:
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
            qFatal() << "Speaker mix is not a regular param";
            break;
        case ParamInfo::Unknown:
            qFatal() << "Unknown param";
            break;
    }
}

void InferControllerPrivate::handleVoiceContextChanged(const VoiceContextChange &change,
                                                       SingingClip *clip) {
    if (!clip)
        return;

    const bool singerChanged = change.before.singer != change.after.singer;
    const bool speakerChanged = change.before.speaker != change.after.speaker;
    if (singerChanged || speakerChanged) {
        clip->removeAllPieces();
        ensureClipInferenceStarted(*clip);
        return;
    }

    if (change.before.speakerMix == change.after.speakerMix)
        return;

    if (clip->pieces().isEmpty()) {
        ensureClipInferenceStarted(*clip);
        return;
    }

    const Timeline timeline{{{0, appModel->tempo()}}};
    for (const auto piece : clip->pieces()) {
        const auto speakerMix = InferSpeakerMixModel::effectiveSpeakerMixFromData(
            clip->speakerMixData(), clip->speakerId(), piece->localStartTick(),
            piece->localEndTick(), timeline);
        piece->speakerMix = speakerMix;
        piece->speaker = speakerMix.fallbackSpeaker;
        Helper::resetPitch(*piece);
        piece->acousticInferStatus = Pending;

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

void InferControllerPrivate::handleLanguageModuleStatusChanged(
    const AppStatus::ModuleStatus status) {
    if (status == AppStatus::ModuleStatus::Ready) {
        qDebug() << "Language module is ready. Tasks will be started.";
    } else if (status == AppStatus::ModuleStatus::Error) {
        clearAllPendingApplies("pending-cleared-module-error");
        m_getPronTasks.disposePendingTasks();
        appOptions->g2pLanguage()->langOrder.clear();
        appOptions->saveAndNotify(AppOptionsGlobal::G2pLanguage);
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

void InferControllerPrivate::ensureClipInferenceStarted(SingingClip &clip) {
    const QPointer<SingingClip> guardedClip(&clip);
    QTimer::singleShot(0, this, [this, guardedClip] {
        if (!guardedClip || appModel->findClipById(guardedClip->id()) != guardedClip)
            return;

        if (canStartClipInference(*guardedClip))
            createAndRunGetPronTask(*guardedClip);
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
        case InferenceApplyGate::Decision::Apply:
            Helper::updatePronunciation(resolution.notes, task.result, *resolution.clip);
            if (!resolution.clip->singerInfo().isEmpty())
                createAndRunGetPhoneTask(*resolution.clip);
            break;
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
        case InferenceApplyGate::Decision::Apply:
            Helper::updatePhoneName(resolution.notes, task.result, *resolution.clip);
            if (!resolution.clip->singerInfo().isEmpty()) {
                auto result = resolution.clip->reSegment();
                for (const auto piece : result.addedPieces)
                    createPipeline(*piece);
            }
            break;
        case InferenceApplyGate::Decision::Defer:
            storePendingPhonemeNameApply(context, task.result);
            break;
        case InferenceApplyGate::Decision::Drop:
            break;
    }
    m_getPhoneTasks.onCurrentFinished(&task);
}

InferControllerPrivate::PendingApplyResult InferControllerPrivate::tryApplyPronunciation(
    const InferenceTaskContext &context, const QStringList &pronunciations, const QString &phase) {
    InferenceApplyGate::Options options;
    options.phase = phase;
    options.expectedNoteCount = pronunciations.count();
    options.requirePiece = false;
    options.requireNotesInPiece = false;
    options.checkSingerSpeaker = false;
    options.checkEditSession = true;

    InferenceTaskResolution resolution;
    switch (InferenceApplyGate::resolve(context, resolution, options)) {
        case InferenceApplyGate::Decision::Apply:
            Helper::updatePronunciation(resolution.notes, pronunciations, *resolution.clip);
            InferenceApplyGate::logDecision(context, phase, InferenceApplyGate::Decision::Apply,
                                            phase == "pending-flush" ? "edit-session-flush-apply"
                                                                     : "clip-task-apply",
                                            resolution.clip->inferenceRevision());
            if (!resolution.clip->singerInfo().isEmpty())
                createAndRunGetPhoneTask(*resolution.clip);
            return PendingApplyResult::Applied;
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
        case InferenceApplyGate::Decision::Apply:
            Helper::updatePhoneName(resolution.notes, phonemeNames, *resolution.clip);
            InferenceApplyGate::logDecision(context, phase, InferenceApplyGate::Decision::Apply,
                                            phase == "pending-flush" ? "edit-session-flush-apply"
                                                                     : "clip-task-apply",
                                            resolution.clip->inferenceRevision());
            if (!resolution.clip->singerInfo().isEmpty()) {
                const auto result = resolution.clip->reSegment();
                for (const auto piece : result.addedPieces)
                    createPipeline(*piece);
            }
            return PendingApplyResult::Applied;
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

void InferControllerPrivate::storePendingPronunciationApply(const InferenceTaskContext &context,
                                                            const QStringList &pronunciations) {
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

void InferControllerPrivate::recreateAllInferTasks() {
    for (const auto &track : appModel->tracks())
        for (const auto &clip : track->clips()) {
            if (clip->clipType() != IClip::Singing)
                continue;
            const auto singingClip = static_cast<SingingClip *>(clip);
            for (const auto &piece : singingClip->pieces()) {
                Helper::resetPhoneOffset(piece->notes, *piece);
                piece->dirty = true;
            }
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

    auto task = new GetPronunciationTask(clip.id(), clip.inferenceRevision(),
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

    auto task = new GetPhonemeNameTask(clip.id(), clip.inferenceRevision(),
                                       buildNoteInferenceSnapshots(clip), clip.singerInfo(),
                                       appModel->tempo());
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

        // A signature mismatch means the old task was correctly rejected, but the piece may still
        // need inference with its new inputs. Restart from duration instead of keeping a dead final
        // pipeline around.
        if (reason != "input-signature-mismatch")
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

void InferControllerPrivate::notifyNextPipeline(const QList<InferPipeline *> &pipelines,
                                                int index) {
    if (index >= pipelines.size())
        return;

    auto pipeline = pipelines[index];
    if (m_inferPipelines.contains(pipeline) && pipeline->piece().acousticInferStatus == Pending)
        pipeline->notifyPlaybackStarted();

    QTimer::singleShot(0, this,
                       [this, pipelines, index] { notifyNextPipeline(pipelines, index + 1); });
}
