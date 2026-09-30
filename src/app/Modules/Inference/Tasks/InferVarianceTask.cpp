#include "InferVarianceTask.h"

#include "Modules/Inference/InferLogging.h"

#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Modules/Inference/Utils/PitchRouting.h"
#include <lite/Support/JsonUtils.h>
#include "InferTaskCommon.h"
#include "InferStageModel.h"
#include "Modules/Inference/Utils/InferCacheUtils.h"
#include "Modules/Inference/Utils/InferFrameLayout.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <utility>

namespace Var = ds::Api::Variance::L1;

bool InferVarianceTask::InferVarianceInput::operator==(const InferVarianceInput &other) const {
    return semanticSignature() == other.semanticSignature();
}

int InferVarianceTask::clipId() const {
    return m_input.clipId;
}

int InferVarianceTask::pieceId() const {
    return m_input.pieceId;
}

InferenceTaskContext InferVarianceTask::inferenceContext() const {
    auto context = m_input.toInferenceTaskContext("variance");
    context.taskId = id();
    context.inputSignature = m_input.semanticSignature();
    return context;
}

bool InferVarianceTask::success() const {
    return m_success.load(std::memory_order_acquire);
}

InferVarianceTask::InferVarianceTask(InferVarianceInput input) : m_input(std::move(input)),
      m_cacheDirectory(appOptions->inference()->cacheDirectory) {
    buildPreviewText();
    TaskStatus status;
    status.title = tr("Infer Variance");
    status.message = tr("Pending infer: %1").arg(m_previewText);
    status.maximum = m_input.notes.count();
    setStatus(status);
    qCDebug(logInferTask) << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferVarianceTask::InferVarianceInput InferVarianceTask::input() const {
    return m_input;
}

InferVarianceTask::InferVarianceResult InferVarianceTask::result() const {
    return m_result;
}

QStringList InferVarianceTask::cacheFileNames() const {
    if (m_inputHash.isEmpty())
        return {};
    return InferCacheUtils::cacheFileNames(QStringLiteral("variance"), m_inputHash).toList();
}

void InferVarianceTask::runTask() {
    qCDebug(logInferTask) << "Running task..."
             << "pieceId:" << pieceId() << " clipId:" << clipId() << "taskId:" << id();
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    GenericInferModel model;
    const auto input = m_input.toEngineModel();
    auto cacheKey = input.hashData().toUtf8();
    cacheKey.append(":variance-output-v2");
    m_inputHash = QCryptographicHash::hash(cacheKey, QCryptographicHash::Sha1).toHex();
    const auto cacheDir = QDir(m_cacheDirectory);
    const auto cacheNames = InferCacheUtils::cacheFileNames(QStringLiteral("variance"), m_inputHash);
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
        qInfo() << "Use cached variance inference result:" << outputCachePath;
    } else {
        QString errorMessage;
        qCDebug(logInferTask) << "Variance inference cache not found. Running inference...";
        if (isTerminateRequested()) {
            abort();
            return;
        }
        if (QList<InferParam> outParams; runInference(input, outParams, errorMessage)) {
            model = input;
            model.params = std::move(outParams);
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

    processOutput(model);
    m_success.store(true, std::memory_order_release);
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

bool InferVarianceTask::runInference(const GenericInferModel &model, QList<InferParam> &outParams,
                                     QString &error) {
    const auto &identifier = model.identifier;
    Var::VarianceStartInput input;
    input.parameters = convertInputParams(model.params);
    input.steps = model.steps;

    if (isTerminateRequested()) {
        abort();
        return false;
    }

    InferDirectMLSerializationGuard dmlGuard;
    const auto stage = InferStageModel::acquire(m_activeInference, identifier,
                                                InferStage::Variance, "inferVariance", error);
    if (!stage)
        return false;
    auto *inferenceVariance = stage->executive<Var::VarianceExecutive>();
    const auto speakerMapping = stage->speakerMapping<Var::VarianceImportOptions>();
    if (!speakerMapping) {
        qCritical() << "inferVariance: Import options not found";
        return false;
    }
    if (!convertStageWords(model, *speakerMapping, true, input.words, input.speakers, error)) {
        qCritical() << "inferVariance:" << error;
        return false;
    }

    // Run variance
    std::unique_ptr<Var::VarianceResult> result;
    // Start inference
    if (isTerminateRequested()) {
        abort();
        return false;
    }
    auto exp = inferenceVariance->start(input);
    if (!exp) {
        // The caller logs this error at the end of the task, so the error records the complete
        // error chain rather than only the outermost message. The innermost cause reported by
        // the interpreter is usually the actionable cause.
        error = QString::fromUtf8(exp.error().toString());
        qCritical().noquote().nospace() << "inferVariance: Failed to start variance inference for "
                                        << identifier << ": " << error;
        return false;
    } else {
        result = exp.take();
        if (!result) {
            qCritical() << "inferVariance: result type mismatch or null result for" << identifier;
            return false;
        }
    }

    // A failure is already reported as an error by start(), so the result needs no further error
    // check. The state is checked instead: a stopped run returns a result like a completed run,
    // and using that result would treat a partial phrase as a complete phrase.
    if (inferenceVariance->state() != srt::ITask::Succeeded) {
        qCritical().noquote().nospace() << "inferVariance: the variance inference for " << identifier
                                        << " did not finish";
        return false;
    }
    outParams.reserve(result->predictions.size());
    for (const auto &param : result->predictions) {
        InferParam inferParam;
        inferParam.tag.assign(param.tag.name());
        inferParam.interval = param.interval;
        inferParam.values.assign(param.values.begin(), param.values.end());
        inferParam.dynamic = true;
        outParams.emplace_back(std::move(inferParam));
    }
    return true;
}

void InferVarianceTask::terminate() {
    IInferTask::terminate();
    m_activeInference.stop();
}

void InferVarianceTask::abort() {
    auto newStatus = status();
    newStatus.message = tr("Terminating: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    qInfo() << "Variance inference task terminated clipId:" << clipId() << "pieceId:" << pieceId()
            << "taskId:" << id();
}

void InferVarianceTask::buildPreviewText() {
    for (const auto &note : m_input.notes) {
        for (const auto &phoneme : note.phonemeNames)
            m_previewText.append(phoneme.name + " ");
    }
}

QString InferVarianceTask::InferVarianceInput::semanticSignature() const {
    return InferInputBase::semanticSignature(
        "variance", QJsonObject{
                        {"pitch",     InferInputBase::paramCurveObject(pitch)    },
                        {"toneShift", InferInputBase::paramCurveObject(toneShift)},
    });
}

GenericInferModel InferVarianceTask::InferVarianceInput::toEngineModel() const {
    auto words = InferTaskHelper::buildWords(*this, true);
    double totalLength = 0;
    // The curves are sampled on the frames of the model of this stage.
    const auto interval = InferFrameLayout::inputInterval(identifier, InferStage::Variance);
    for (const auto &word : words)
        totalLength += word.length();

    int frames = qRound(totalLength / interval);
    InferRetake retake;
    retake.end = frames;

    InferParam param;
    param.dynamic = true;
    param.interval = interval;
    param.retake = retake;

    InferParam pitch = param;
    pitch.tag = "pitch";
    pitch.values =
        PitchRouting::applyToneShift(resamplePitchToFrames(this->pitch, frames, interval),
                                     resampleCurveToFrames(this->toneShift, frames, interval));

    InferParam breathiness = param;
    breathiness.tag = "breathiness";
    InferParam tension = param;
    tension.tag = "tension";
    InferParam voicing = param;
    voicing.tag = "voicing";
    InferParam energy = param;
    energy.tag = "energy";
    InferParam mouthOpening = param;
    mouthOpening.tag = "mouth_opening";
    for (int i = 0; i < frames; i++) {
        breathiness.values.append(0);
        tension.values.append(0);
        voicing.values.append(0);
        energy.values.append(0);
        mouthOpening.values.append(0);
    }

    GenericInferModel model;
    model.speaker = speaker;
    const auto effectiveMix =
        speakerMix.isEmpty() ? InferSpeakerMixModel::staticSpeakerMix(speaker) : speakerMix;
    model.speakerMix = InferSpeakerMixModel::fitToFrames(effectiveMix, frames, interval);
    model.words = words;
    model.params = {pitch, breathiness, tension, voicing, energy, mouthOpening};
    model.steps = steps;
    model.identifier = identifier;
    return model;
}

bool InferVarianceTask::processOutput(const GenericInferModel &model) {
    const auto curveFor = [this, &model](const QString &tag) {
        for (const auto &param : model.params) {
            if (param.tag == tag)
                return m_input.resampleFramesToCurve(param.values, param.interval);
        }
        return InferParamCurve{};
    };

    m_result.breathiness = curveFor(QStringLiteral("breathiness"));
    m_result.tension = curveFor(QStringLiteral("tension"));
    m_result.voicing = curveFor(QStringLiteral("voicing"));
    m_result.energy = curveFor(QStringLiteral("energy"));
    m_result.mouthOpening = curveFor(QStringLiteral("mouth_opening"));
    return true;
}
