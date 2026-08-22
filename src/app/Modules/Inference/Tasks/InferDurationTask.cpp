#include "InferDurationTask.h"

#include <diffsinger/Infer/dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include <lite/ProjectModel/Utils/PhonemeHeadLayout.h>
#include <lite/Support/JsonUtils.h>
#include "InferTaskCommon.h"

#include <algorithm>
#include <cmath>
#include <QThread>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <utility>

namespace Dur = srt::svs::Api::Duration::L1;

bool InferDurationTask::InferDurInput::operator==(const InferDurInput &other) const {
    return semanticSignature() == other.semanticSignature();
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

QStringList InferDurationTask::cacheFileNames() const {
    if (m_inputHash.isEmpty())
        return {};
    return {QStringLiteral("infer-duration-input-%1.json").arg(m_inputHash),
            QStringLiteral("infer-duration-output-%1.json").arg(m_inputHash)};
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
    if (!cacheDir.exists())
        cacheDir.mkpath(".");
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
                        if (i >= phonemeDurations.size())
                            return false;
                        phoneme.start = timeCursor;
                        timeCursor += phonemeDurations[i];
                        ++i;
                    }
                }
                return i == phonemeDurations.size();
            };

            model = input;
            if (!updatePhonemeStarts(model.words, durations)) {
                qCritical() << "Duration result mapping failed. clipId:" << clipId()
                            << "pieceId:" << pieceId() << "taskId:" << id();
                return;
            }
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

    InferDirectMLSerializationGuard dmlGuard;
    const auto handle = inferEngine->acquireSingerSession(identifier);
    if (!handle) {
        qCritical() << "inferDuration: failed to acquire singer session for" << identifier;
        return false;
    }
    auto modelExp = m_activeInference.acquire(handle, ds::infer::StageKind::Duration);
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
    input->words =
        convertInputWords(model.words, speakerName, model.speakerMix, speakerMapping, error);
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

    for (size_t i = 0; i < result->durations.size(); ++i) {
        const auto duration = result->durations[i];
        if (!std::isfinite(duration) || duration < 0) {
            error = QStringLiteral("Duration result contains an invalid value at index %1: %2")
                        .arg(static_cast<qulonglong>(i))
                        .arg(duration);
            qCritical().noquote() << "inferDuration:" << error << "clipId:" << clipId()
                                  << "pieceId:" << pieceId() << "taskId:" << id();
            return false;
        }
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
    qInfo() << "Duration inference task terminated clipId:" << clipId() << "pieceId:" << pieceId()
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
    class OutputPhone {
    public:
        QString token;
        double wordLength = 0;
        double start = 0;
    };

    QList<OutputPhone> outputPhones;
    for (const auto &word : model.words) {
        const auto wordLength = word.length();
        if (!std::isfinite(wordLength) || wordLength < 0) {
            qCritical() << "Duration output has invalid word length. clipId:" << clipId()
                        << "pieceId:" << pieceId() << "taskId:" << id()
                        << "wordLength:" << wordLength;
            return false;
        }
        for (const auto &phoneme : word.phones) {
            if (!std::isfinite(phoneme.start) || phoneme.start < 0) {
                qCritical() << "Duration output has invalid phoneme start. clipId:" << clipId()
                            << "pieceId:" << pieceId() << "taskId:" << id()
                            << "phone:" << phoneme.token << "start:" << phoneme.start;
                return false;
            }
            outputPhones.append({phoneme.token, wordLength, phoneme.start});
        }
    }

    auto result = m_input;
    int phoneIndex = 0;
    for (auto &note : result.notes) {
        // Skip consecutive rest, breath, and slur notes.
        if (note.isRest || note.isSlur)
            continue;

        // Skip consecutive SP and AP phonemes.
        while (phoneIndex < outputPhones.size() &&
               (outputPhones.at(phoneIndex).token == "SP" ||
                outputPhones.at(phoneIndex).token == "AP")) {
            phoneIndex++;
        }

        QList<int> noteOffsets;
        bool foundOnset = false;
        for (const auto &phonemeName : note.phonemeNames) {
            if (phoneIndex >= outputPhones.size()) {
                qCritical() << "Duration output ended before the note phoneme mapping. clipId:"
                            << clipId() << "pieceId:" << pieceId() << "taskId:" << id()
                            << "noteId:" << note.id;
                return false;
            }
            const auto &outputPhone = outputPhones.at(phoneIndex);
            if (outputPhone.token != phonemeName.name) {
                qCritical() << "Duration output phoneme mapping mismatch. clipId:" << clipId()
                            << "pieceId:" << pieceId() << "taskId:" << id()
                            << "noteId:" << note.id << "expected:" << phonemeName.name
                            << "actual:" << outputPhone.token;
                return false;
            }
            if (phonemeName.isOnset)
                foundOnset = true;
            const auto offsetSeconds =
                foundOnset ? outputPhone.start : outputPhone.start - outputPhone.wordLength;
            noteOffsets.append(qRound(offsetSeconds * 1000));
            phoneIndex++;
        }

        if (!std::is_sorted(noteOffsets.cbegin(), noteOffsets.cend())) {
            qCritical() << "Duration output produced unordered phoneme offsets. clipId:" << clipId()
                        << "pieceId:" << pieceId() << "taskId:" << id()
                        << "noteId:" << note.id << "offsets:" << noteOffsets;
            return false;
        }
        note.phonemeOffsets = noteOffsets;
    }

    while (phoneIndex < outputPhones.size() &&
           (outputPhones.at(phoneIndex).token == "SP" ||
            outputPhones.at(phoneIndex).token == "AP")) {
        phoneIndex++;
    }
    if (phoneIndex != outputPhones.size()) {
        qCritical() << "Duration output contains unmapped phonemes. clipId:" << clipId()
                    << "pieceId:" << pieceId() << "taskId:" << id()
                    << "firstUnmappedIndex:" << phoneIndex;
        return false;
    }

    if (!result.notes.isEmpty() && !result.notes.first().isRest &&
        !result.notes.first().isSlur) {
        const auto headLayout =
            PhonemeHeadLayout::calculate(result.paddingStartMs, result.headAvailableLengthMs,
                                         result.notes.first().phonemeOffsets);
        if (!headLayout.isWithinBounds()) {
            qCritical() << "Duration output exceeds the piece head boundary. clipId:" << clipId()
                        << "pieceId:" << pieceId() << "taskId:" << id()
                        << "noteId:" << result.notes.first().id
                        << "minimumOffsetMs:" << headLayout.minimumFirstOffsetMs
                        << "requiredHeadLengthMs:" << headLayout.requiredHeadLengthMs
                        << "maximumHeadLengthMs:" << headLayout.maximumHeadLengthMs;
            return false;
        }
    }

    QWriteLocker writeLocker(&m_rwLock);
    m_result = std::move(result);
    return true;
}
