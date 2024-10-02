//
// Created by OrangeCat on 24-9-4.
//

#include "InferDurationTask.h"

#include "InferEngine.h"
#include "Model/Inference/GenericInferModel.h"
#include "Model/Inference/InferTaskHelper.h"
#include "Utils/JsonUtils.h"

#include <QThread>
#include <QDebug>
#include <QJsonDocument>
#include <utility>

namespace dsonnxinfer {
    struct Segment;
}

bool InferDurationTask::InferDurInput::operator==(const InferDurInput &other) const {
    return clipId == other.clipId && pieceId == other.pieceId && notes == other.notes &&
           configPath == other.configPath && qFuzzyCompare(tempo, other.tempo);
}

int InferDurationTask::clipId() const {
    return m_input.clipId;
}

int InferDurationTask::pieceId() const {
    return m_input.pieceId;
}

bool InferDurationTask::success() const {
    return m_success;
}

InferDurationTask::InferDurationTask(InferDurInput input) : m_input(std::move(input)) {
    buildPreviewText();
    TaskStatus status;
    status.title = "推理音素长度";
    status.message = "正在等待：" + m_previewText;
    status.maximum = m_input.notes.count();
    setStatus(status);
    qDebug() << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferDurationTask::InferDurInput InferDurationTask::input() {
    QMutexLocker locker(&m_mutex);
    return m_input;
}

QList<InferDurPitNote> InferDurationTask::result() {
    QMutexLocker locker(&m_mutex);
    return m_result.notes;
}

void InferDurationTask::runTask() {
    qDebug() << "Running task..."
             << "pieceId:" << pieceId() << " clipId:" << clipId() << "taskId:" << id();
    auto newStatus = status();
    newStatus.message = "正在推理: " + m_previewText;
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    if (!inferEngine->runLoadConfig(m_input.configPath)) {
        qCritical() << "Task failed" << m_input.configPath << "clipId:" << clipId()
                    << "pieceId:" << pieceId() << "taskId:" << id();
        return;
    }
    if (isTerminateRequested()) {
        abort();
        return;
    }

    QString resultJson;
    QString errorMessage;
    if (!inferEngine->inferDuration(buildInputJson(), resultJson, errorMessage)) {
        qCritical() << "Task failed:" << errorMessage;
    }
    if (isTerminateRequested()) {
        abort();
        return;
    }

    m_success = processOutput(resultJson);
    if (m_success)
        qInfo() << "Success:"
                << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
    else
        qCritical() << "Failed:"
                    << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

void InferDurationTask::abort() {
    auto newStatus = status();
    newStatus.message = "正在停止: " + m_previewText;
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    qInfo() << "时长推理任务被终止 clipId:" << clipId() << "pieceId:" << pieceId()
            << "taskId:" << id();
}

void InferDurationTask::buildPreviewText() {
    for (const auto &note : m_input.notes) {
        for (const auto &phoneme : note.aheadNames)
            m_previewText.append(phoneme + " ");
        for (const auto &phoneme : note.normalNames)
            m_previewText.append(phoneme + " ");
    }
}

QString InferDurationTask::buildInputJson() const {
    GenericInferModel model;
    model.words = InferTaskHelper::buildWords(m_input.notes, m_input.tempo);
    return model.serializeToJson();
}

bool InferDurationTask::processOutput(const QString &json) {
    GenericInferModel model;
    if (!model.deserializeFromJson(json))
        return false;

    QList<std::pair<double, double>> offsets;
    for (const auto &word : model.words) {
        for (const auto &phoneme : word.phones) {
            offsets.append({word.length(), phoneme.start});
        }
    }
    m_result = m_input;
    int phoneIndex = 1; // Skip SP phoneme
    int noteIndex = 0;
    for (auto &note : m_result.notes) {
        note.aheadOffsets.clear();
        for (int aheadIndex = 0; aheadIndex < note.aheadNames.count(); aheadIndex++) {
            note.aheadOffsets.append(
                qRound((offsets[phoneIndex].first - offsets[phoneIndex].second) * 1000));
            phoneIndex++;
        }
        note.normalOffsets.clear();
        for (int normalIndex = 0; normalIndex < note.normalNames.count(); normalIndex++) {
            note.normalOffsets.append(qRound(offsets[phoneIndex].second * 1000));
            phoneIndex++;
        }
        noteIndex++;
    }
    return true;
}