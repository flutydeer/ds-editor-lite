//
// Created by OpenVPI on 2026/7/22.
//

#include "InferAcousticCacheProbeTask.h"

#include <QDebug>

#include <utility>

InferAcousticCacheProbeTask::InferAcousticCacheProbeTask(
    InferAcousticTask::InferAcousticInput input)
    : m_input(std::move(input)) {
    TaskStatus status;
    status.title = tr("Probe Acoustic Cache");
    status.message = tr("Pending acoustic cache probe");
    status.isIndetermine = true;
    setStatus(status);
    qDebug() << "Acoustic cache probe created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

int InferAcousticCacheProbeTask::clipId() const {
    return m_input.clipId;
}

int InferAcousticCacheProbeTask::pieceId() const {
    return m_input.pieceId;
}

InferenceTaskContext InferAcousticCacheProbeTask::inferenceContext() const {
    auto context = m_input.toInferenceTaskContext("acoustic");
    context.taskId = id();
    context.inputSignature = m_input.semanticSignature();
    return context;
}

bool InferAcousticCacheProbeTask::success() const {
    return m_success.load(std::memory_order_acquire);
}

bool InferAcousticCacheProbeTask::cacheHit() const {
    return m_cacheHit.load(std::memory_order_acquire);
}

QString InferAcousticCacheProbeTask::result() const {
    return m_result;
}

void InferAcousticCacheProbeTask::runTask() {
    if (isTerminateRequested())
        return;

    auto newStatus = status();
    newStatus.message = tr("Probing acoustic cache");
    setStatus(newStatus);

    const auto cache = InferAcousticTask::lookupCache(m_input);
    if (isTerminateRequested())
        return;

    if (cache.hit)
        m_result = cache.outputCachePath;
    m_cacheHit.store(cache.hit, std::memory_order_release);
    m_success.store(true, std::memory_order_release);
    qDebug() << "Acoustic cache probe finished"
             << "hit:" << cache.hit << "clipId:" << clipId() << "pieceId:" << pieceId()
             << "taskId:" << id();
}
