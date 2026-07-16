//
// Created by OrangeCat on 24-9-4.
//

#include "InferDurationTask.h"

#include <diffsinger/Infer/dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Utils/JsonUtils.h"
#include "InferTaskCommon.h"

#include <QThread>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <utility>

namespace Dur = srt::svs::Api::Duration::L1;

bool InferDurationTask::InferDurInput::operator==(const InferDurInput &other) const {
    const bool clipIdEqual = clipId == other.clipId;
    const bool pieceIdEqual = pieceId == other.pieceId;
    const bool notesEqual = notes == other.notes;
    const bool identifierEqual = identifier == other.identifier;
    const bool timelineEqual = timeline == other.timeline;
    const bool speakerEqual = speaker == other.speaker;
    return clipIdEqual && pieceIdEqual && notesEqual && identifierEqual && timelineEqual &&
           speakerEqual;
}

int InferDurationTask::clipId() const {
    return m_input.clipId;
}

int InferDurationTask::pieceId() const {
    return m_input.pieceId;
}

InferenceTaskContext InferDurationTask::inferenceContext() const {
    auto context = m_input.toInferenceTaskContext("duration");
    context.taskId = id();
    context.inputSignature = m_input.semanticSignature();
    return context;
}

bool InferDurationTask::success() const {
    return m_success.load(std::memory_order_acquire);
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

InferDurationTask::InferDurInput InferDurationTask::input() const {
    return m_input;
}

QList<InferInputNote> InferDurationTask::result() const {
    QReadLocker readLocker(&m_rwLock);
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
    const auto input = m_input.toEngineModel();
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

    if (useCache) {
        qInfo() << "Use cached duration inference result:" << outputCachePath;
    } else {
        QString errorMessage;
        qDebug() << "Duration inference cache not found. Running inference...";
        if (isTerminateRequested()) {
            abort();
            return;
        }
        if (std::vector<double> durations; runInference(input, durations, errorMessage)) {
            auto updatePhonemeStarts = [](QList<InferWord> &words,
                                          const std::vector<double> &phonemeDurations) {
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
        JsonUtils::save(outputCachePath, model.serialize());
    }

    if (isTerminateRequested()) {
        abort();
        return;
    }

    if (!processOutput(model)) {
        qCritical() << "Duration inference output is invalid. clipId:" << clipId()
                    << "pieceId:" << pieceId() << "taskId:" << id();
        return;
    }
    m_success.store(true, std::memory_order_release);
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

bool InferDurationTask::runInference(const GenericInferModel &model,
                                     std::vector<double> &outDuration, QString &error) {
    if (!inferEngine->initialized()) {
        qCritical().noquote() << "inferDuration: Environment is not initialized";
        return false;
    }

    const auto &identifier = model.identifier;
    std::string speakerName = model.speaker.toStdString();
    const auto input = srt::core::NO<Dur::DurationStartInput>::create();

    const auto session = inferEngine->acquireSingerSession(identifier);
    if (!session) {
        qCritical() << "inferDuration: failed to acquire singer session for" << identifier;
        return false;
    }
    auto modelExp = m_activeInference.acquire(session, ds::infer::StageKind::Duration);
    if (!modelExp) {
        qCritical().noquote().nospace()
            << "inferDuration: failed to load duration model for " << identifier << ": "
            << QString::fromUtf8(modelExp.error().message());
        return false;
    }
    auto activeInference = modelExp.take();
    auto &acquiredModel = activeInference.model();
    auto inferenceDuration = acquiredModel.inference;
    if (!inferenceDuration) {
        qCritical() << "inferDuration: Duration inference not found for" << identifier;
        return false;
    }

    // Convert singer speaker id to inference speaker id
    if (!acquiredModel.importOptions) {
        qCritical() << "inferDuration: Import options not found";
        return false;
    }
    const auto importOptions = acquiredModel.importOptions.as<Dur::DurationImportOptions>();
    if (!importOptions) {
        qCritical() << "inferDuration: Import options not found";
        return false;
    }
    const auto &speakerMapping = importOptions->speakerMapping;
    input->words = convertInputWords(model.words, speakerName, model.speakerMix, speakerMapping, error);
    if (!error.isEmpty()) {
        qCritical() << "inferDuration:" << error;
        return false;
    }

    // Run duration
    srt::core::NO<Dur::DurationResult> result;
    // Start inference
    if (isTerminateRequested()) {
        abort();
        return false;
    }
    auto exp = inferenceDuration->start(input);
    if (!exp) {
        qCritical().noquote().nospace() << "inferDuration: Failed to start duration inference for "
                                        << identifier << ": " << exp.error().message();
        return false;
    } else {
        result = exp.take().as<Dur::DurationResult>();
        if (!result) {
            qCritical() << "inferDuration: result type mismatch or null result for" << identifier;
            return false;
        }
    }

    if (!result->error.ok()) {
        qCritical().noquote().nospace() << "inferDuration: Failed to run duration inference for "
                                        << identifier << ": " << result->error.message();
        return false;
    }

    if (inferenceDuration->state() == srt::core::ITask::Failed) {
        qCritical().noquote().nospace() << "inferDuration: Failed to run duration inference for "
                                        << identifier << ": " << result->error.message();
        return false;
    }

    size_t phonemeCount = 0;
    for (const auto &word : model.words) {
        phonemeCount += static_cast<size_t>(word.phones.size());
    }
    if (result->durations.size() != phonemeCount) {
        error = QStringLiteral("Duration result size mismatch: expected %1 phonemes, got %2")
                    .arg(static_cast<qulonglong>(phonemeCount))
                    .arg(static_cast<qulonglong>(result->durations.size()));
        qCritical().noquote() << "inferDuration:" << error;
        return false;
    }

    outDuration = std::move(result->durations);

    return true;
}

void InferDurationTask::terminate() {
    IInferTask::terminate();
    m_activeInference.stop();
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
        for (const auto &phoneme : note.phonemeNames)
            m_previewText.append(phoneme.name + " ");
    }
}

QString InferDurationTask::InferDurInput::semanticSignature() const {
    return InferInputBase::semanticSignature("duration");
}

GenericInferModel InferDurationTask::InferDurInput::toEngineModel() const {
    GenericInferModel model;
    model.speaker = speaker;
    model.speakerMix = speakerMix;
    model.words = InferTaskHelper::buildWords(*this);
    model.identifier = identifier;
    model.steps = steps;
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
    QWriteLocker writeLocker(&m_rwLock);
    m_result = m_input;
    int phoneIndex = 1;
    int noteIndex = 0;
    for (auto &note : m_result.notes) {
        // 跳过连续的休止、换气和转音音符
        if (note.isRest || note.isSlur)
            continue;

        // 跳过连续的 SP 和 AP 音素
        while (phoneIndex < isRestPhones.size() && isRestPhones.at(phoneIndex) == true) {
            phoneIndex++;
        }
        if (phoneIndex >= offsets.size()) {
            return false;
        }

        qsizetype headerPhonemeCount = 0;
        qsizetype normalPhonemeCount = 0;
        bool foundOnset = false;
        for (int i = 0; i < note.phonemeNames.count(); i++) {
            auto phonemeName = note.phonemeNames.at(i);
            if (phonemeName.isOnset) {
                foundOnset = true;
            }
            if (foundOnset) {
                normalPhonemeCount++;
            } else {
                headerPhonemeCount++;
            }
        }

        QList<int> aheadOffsets;
        for (int aheadIndex = 0; aheadIndex < headerPhonemeCount; aheadIndex++) {
            if (phoneIndex >= offsets.size()) {
                return false;
            }
            aheadOffsets.append(
                qRound((offsets[phoneIndex].second - offsets[phoneIndex].first) * 1000));
            phoneIndex++;
        }

        QList<int> normalOffsets;
        for (int normalIndex = 0; normalIndex < normalPhonemeCount; normalIndex++) {
            if (phoneIndex >= offsets.size()) {
                return false;
            }
            normalOffsets.append(qRound(offsets[phoneIndex].second * 1000));
            phoneIndex++;
        }

        note.phonemeOffsets = aheadOffsets + normalOffsets;
        noteIndex++;
    }
    return true;
}
