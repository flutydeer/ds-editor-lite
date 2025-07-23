//
// Created by OrangeCat on 24-9-4.
//

#include "InferDurationTask.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Utils/JsonUtils.h"

#include <QThread>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <utility>

namespace dsonnxinfer {
    struct Segment;
}

bool InferDurationTask::InferDurInput::operator==(const InferDurInput &other) const {
    bool clipIdEqual = clipId == other.clipId;
    bool pieceIdEqual = pieceId == other.pieceId;
    bool notesEqual = notes == other.notes;
    bool configPathEqual = configPath == other.configPath;
    bool tempoEqual = tempo == other.tempo;
    return clipIdEqual && pieceIdEqual && notesEqual && configPathEqual && tempoEqual;
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
    status.title = tr("Infer Duration");
    status.message = tr("Pending infer: %1").arg(m_previewText);
    status.maximum = m_input.notes.count();
    setStatus(status);
    qDebug() << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferDurationTask::InferDurInput InferDurationTask::input() {
    QMutexLocker locker(&m_mutex);
    return m_input;
}

QList<InferInputNote> InferDurationTask::result() {
    QMutexLocker locker(&m_mutex);
    return m_result.notes;
}

void InferDurationTask::runTask() {
    qDebug() << "Running task..."
             << "pieceId:" << pieceId() << " clipId:" << clipId() << "taskId:" << id();
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    GenericInferModel model;
    const auto input = buildInputJson();
    m_inputHash = input.hashData();
    const auto cacheDir = QDir(appOptions->inference()->cacheDirectory);
    const auto inputCachePath =
        cacheDir.filePath(QString("infer-duration-input-%1.json").arg(m_inputHash));
    if (!QFile(inputCachePath).exists())
        JsonUtils::save(inputCachePath, input.serialize());
    bool useCache = false;
    const auto outputCachePath =
        cacheDir.filePath(QString("infer-duration-output-%1.json").arg(m_inputHash));
    if (QFile(outputCachePath).exists()) {
        QJsonObject obj;
        useCache = JsonUtils::load(outputCachePath, obj) && model.deserialize(obj);
    }

    QString errorMessage;
    if (useCache) {
        qInfo() << "Use cached duration inference result:" << outputCachePath;
    } else {
        qDebug() << "Duration inference cache not found. Running inference...";
        if (!inferEngine->runLoadConfig(m_input.configPath)) {
            qCritical() << "Task failed" << m_input.configPath << "clipId:" << clipId()
                        << "pieceId:" << pieceId() << "taskId:" << id();
            return;
        }
        if (isTerminateRequested()) {
            abort();
            return;
        }
        if (std::vector<double> durations; inferEngine->inferDuration(input, durations, errorMessage)) {
            auto updatePhonemeStarts = [](QList<InferWord> &words, const std::vector<double> &phonemeDurations){
                size_t i = 0;
                for (auto &word : words) {
                    double timeCursor = 0.0;
                    for (auto &phoneme : word.phones) {
                        if (i >= phonemeDurations.size()) {
                            return;
                        }
                        phoneme.start = timeCursor;
                        timeCursor += phonemeDurations[i];
                        ++i;
                    }
                }
            };

            model = input;
            updatePhonemeStarts(model.words, durations);
        } else {
            qCritical() << "Task failed:" << errorMessage;
            return;
        }
    }

    if (isTerminateRequested()) {
        abort();
        return;
    }

    JsonUtils::save(outputCachePath, model.serialize());
    processOutput(model);
    m_success = true;
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

void InferDurationTask::terminate() {
    IInferTask::terminate();
    inferEngine->terminateInferDurationAsync();
}

void InferDurationTask::abort() {
    auto newStatus = status();
    newStatus.message = tr("Terminating: %1").arg(m_previewText);
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

GenericInferModel InferDurationTask::buildInputJson() const {
    GenericInferModel model;
    model.singer = appOptions->general()->defaultSingerId;
    model.speaker = appOptions->general()->defaultSpeakerId;
    model.words = InferTaskHelper::buildWords(m_input.notes, m_input.tempo);
    model.configPath = m_input.configPath;
    model.steps = appOptions->inference()->samplingSteps;
    return model;
}

bool InferDurationTask::processOutput(const GenericInferModel &model) {
    QList<bool> isRestPhones;
    QList<std::pair<double, double>> offsets;
    for (const auto &word : model.words) {
        for (const auto &phoneme : word.phones) {
            isRestPhones.append(phoneme.token == "SP" || phoneme.token == "AP");
            offsets.append({word.length(), phoneme.start});
        }
    }
    m_result = m_input;
    int phoneIndex = 1;
    int noteIndex = 0;
    for (auto &note : m_result.notes) {
        // 跳过连续的休止、换气和转音音符
        if (note.isRest || note.isSlur)
            continue;

        // 跳过连续的 SP 和 AP 音素
        while (isRestPhones.at(phoneIndex) == true) {
            phoneIndex++;
        }

        note.aheadOffsets.clear();
        for (int aheadIndex = 0; aheadIndex < note.aheadNames.count(); aheadIndex++) {
            note.aheadOffsets.append(
                qRound((offsets[phoneIndex].first - offsets[phoneIndex].second) * 1000));
            phoneIndex++;
        }
        note.normalOffsets.clear();
        for (int normalIndex = 0; normalIndex < note.normalNames.count(); normalIndex++) {
            note.normalOffsets.append(qRound(offsets[phoneIndex].second * 1000));
            note.normalOffsets.append(0);
            phoneIndex++;
        }
        noteIndex++;
    }
    return true;
}