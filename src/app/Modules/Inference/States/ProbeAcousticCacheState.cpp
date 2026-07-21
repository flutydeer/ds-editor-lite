//
// Created by OpenVPI on 2026/7/22.
//

#include "ProbeAcousticCacheState.h"

#include "Controller/PlaybackController.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferController.h"
#include "Modules/Inference/InferControllerHelper.h"
#include "Modules/Inference/InferPipeline.h"

#include <QDebug>
#include <QTimer>

namespace Helper = InferControllerHelper;

ProbeAcousticCacheState::ProbeAcousticCacheState(InferPipeline &pipeline, QState *parent)
    : QState(parent), m_pipeline(pipeline) {
}

void ProbeAcousticCacheState::onEntry(QEvent *event) {
    qDebug() << "ProbeAcousticCacheState::onEntry";
    QState::onEntry(event);

    cancelCurrentTask();

    auto &piece = m_pipeline.piece();
    piece.acousticInferStatus = Pending;
    piece.state = QString("Acoustic.CacheProbe");
    Helper::resetAcoustic(piece);
    m_pipeline.setAcousticResult({});

    const auto input = Helper::buildInferAcousticInput(piece, piece.clip->singerIdentifier());
    auto *task = new InferAcousticCacheProbeTask(input);
    connect(task, &InferAcousticCacheProbeTask::finished, this,
            [this, task] { handleTaskFinished(*task); });
    m_currentTask = task;
    inferController->addInferAcousticCacheProbeTask(*task);
}

void ProbeAcousticCacheState::onExit(QEvent *event) {
    qDebug() << "ProbeAcousticCacheState::onExit";
    cancelCurrentTask();
    QState::onExit(event);
}

void ProbeAcousticCacheState::handleTaskFinished(InferAcousticCacheProbeTask &task) {
    if (!m_currentTask || m_currentTask != &task) {
        qDebug() << "Ignoring acoustic cache probe that is no longer current";
        return;
    }

    const auto finishCurrentTask = [this, &task] {
        if (!inferController->finishCurrentInferAcousticCacheProbeTask(&task)) {
            qWarning() << "Failed to finish acoustic cache probe because it is not current"
                       << "taskId:" << task.id();
        }
        m_currentTask = nullptr;
    };

    if (task.terminated()) {
        finishCurrentTask();
        return;
    }

    if (!task.success()) {
        m_pipeline.notifyDropped("cache-probe-failed");
        finishCurrentTask();
        QTimer::singleShot(0, this, [this] { emit dropped(); });
        return;
    }

    m_pipeline.setApplyContext(task.inferenceContext());
    const auto gate = m_pipeline.resolveApplyContext(-1, false);
    if (gate.decision != InferenceApplyGate::Decision::Apply) {
        m_pipeline.notifyDropped(gate.reason);
        finishCurrentTask();
        QTimer::singleShot(0, this, [this] { emit dropped(); });
        return;
    }

    const bool hasCacheHit = task.cacheHit();
    if (hasCacheHit)
        m_pipeline.setAcousticResult(task.result());
    const bool immediateInference = appOptions->inference()->autoStartInfer ||
                                    playbackController->playbackStatus() ==
                                        PlaybackStatus::Playing;
    finishCurrentTask();

    QTimer::singleShot(0, this, [this, hasCacheHit, immediateInference] {
        if (hasCacheHit)
            emit cacheHit();
        else if (immediateInference)
            emit cacheMissWithImmediateInference();
        else
            emit cacheMissWithLazyInference();
    });
}

void ProbeAcousticCacheState::cancelCurrentTask() {
    if (!m_currentTask)
        return;

    m_currentTask->disconnect(this);
    inferController->cancelInferAcousticCacheProbeTask(m_currentTask->id());
    m_currentTask = nullptr;
}
