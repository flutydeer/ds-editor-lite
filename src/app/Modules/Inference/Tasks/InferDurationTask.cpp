#include "InferDurationTask.h"

#include "Modules/Inference/InferLogging.h"

#include <dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include <lite/ProjectModel/Utils/PhonemeHeadLayout.h>
#include <lite/Support/JsonUtils.h>
#include "InferTaskCommon.h"
#include "InferStageModel.h"
#include "Modules/Inference/Utils/InferCacheUtils.h"

#include <algorithm>
#include <cmath>
#include <QThread>
#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <utility>

namespace Dur = ds::Api::Duration::L1;

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

InferDurationTask::InferDurationTask(InferDurInput input) : m_input(std::move(input)),
      m_cacheDirectory(appOptions->inference()->cacheDirectory) {
    buildPreviewText();
    TaskStatus status;
    status.title = tr("Infer Duration");
    status.message = tr("Pending infer: %1").arg(m_previewText);
    status.maximum = m_input.notes.count();
    setStatus(status);
    qCDebug(logInferTask) << "Task created"
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
    return InferCacheUtils::cacheFileNames(QStringLiteral("duration"), m_inputHash).toList();
}

void InferDurationTask::runTask() {
    qCDebug(logInferTask) << "Running task..."
             << "pieceId:" << pieceId() << " clipId:" << clipId() << "taskId:" << id();
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    GenericInferModel model;
    const auto input = m_input.toEngineModel();
    m_inputHash = input.hashData();
    const auto cacheDir = QDir(m_cacheDirectory);
    const auto cacheNames = InferCacheUtils::cacheFileNames(QStringLiteral("duration"), m_inputHash);
    if (!cacheDir.exists())
        cacheDir.mkpath(".");
    const auto inputCachePath =
        cacheDir.filePath(cacheNames.input);
    if (!QFile(inputCachePath).exists())
        JsonUtils::save(inputCachePath, input.serialize());
    bool useCache = false;
    const auto outputCachePath =
        cacheDir.filePath(cacheNames.output);
    if (QFile(outputCachePath).exists()) {
        QJsonObject obj;
        useCache = JsonUtils::load(outputCachePath, obj) && model.deserialize(obj);
    }

    if (useCache) {
        qInfo() << "Use cached duration inference result:" << outputCachePath;
    } else {
        QString errorMessage;
        qCDebug(logInferTask) << "Duration inference cache not found. Running inference...";
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
    const auto &identifier = model.identifier;
    Dur::DurationStartInput input;

    InferDirectMLSerializationGuard dmlGuard;
    const auto stage = InferStageModel::acquire(m_activeInference, identifier,
                                                InferStage::Duration, "inferDuration", error);
    if (!stage)
        return false;
    auto *inferenceDuration = stage->executive<Dur::DurationExecutive>();
    const auto speakerMapping = stage->speakerMapping<Dur::DurationImportOptions>();
    if (!speakerMapping) {
        qCritical() << "inferDuration: Import options not found";
        return false;
    }
    // The duration input has no frame level speakers, so only the words are converted.
    std::vector<ds::Api::Common::L1::InputSpeakerInfo> unusedSpeakers;
    if (!convertStageWords(model, *speakerMapping, false, input.words, unusedSpeakers, error)) {
        qCritical() << "inferDuration:" << error;
        return false;
    }

    // Run duration
    std::unique_ptr<Dur::DurationResult> result;
    // Start inference
    if (isTerminateRequested()) {
        abort();
        return false;
    }
    auto exp = inferenceDuration->start(input);
    if (!exp) {
        qCritical().noquote().nospace() << "inferDuration: Failed to start duration inference for "
                                        << identifier << ": " << exp.error().message();
        error = QString::fromUtf8(exp.error().toString());
        return false;
    } else {
        result = exp.take();
        if (!result) {
            qCritical() << "inferDuration: result type mismatch or null result for" << identifier;
            return false;
        }
    }

    // A failure is already reported as an error by start(), so the result needs no further error
    // check. The state is checked instead: a stopped run returns a result like a completed run,
    // and using that result would treat a partial phrase as a complete phrase.
    if (inferenceDuration->state() != srt::ITask::Succeeded) {
        qCritical().noquote().nospace() << "inferDuration: the duration inference for " << identifier
                                        << " did not finish";
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

        // Skip consecutive reserved phonemes, which belong to rests and padding.
        while (phoneIndex < outputPhones.size() &&
               m_input.reservedPhonemes.contains(outputPhones.at(phoneIndex).token)) {
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
                            << "pieceId:" << pieceId() << "taskId:" << id() << "noteId:" << note.id
                            << "expected:" << phonemeName.name << "actual:" << outputPhone.token;
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
                        << "pieceId:" << pieceId() << "taskId:" << id() << "noteId:" << note.id
                        << "offsets:" << noteOffsets;
            return false;
        }
        note.phonemeOffsets = noteOffsets;
    }

    while (phoneIndex < outputPhones.size() &&
           m_input.reservedPhonemes.contains(outputPhones.at(phoneIndex).token)) {
        phoneIndex++;
    }
    if (phoneIndex != outputPhones.size()) {
        qCritical() << "Duration output contains unmapped phonemes. clipId:" << clipId()
                    << "pieceId:" << pieceId() << "taskId:" << id()
                    << "firstUnmappedIndex:" << phoneIndex;
        return false;
    }

    if (!result.notes.isEmpty() && !result.notes.first().isRest && !result.notes.first().isSlur) {
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
