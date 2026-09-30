#include "InferPitchTask.h"

#include "Modules/Inference/InferLogging.h"

#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include <lite/Support/JsonUtils.h>
#include <lite/Support/Linq.h>
#include "InferTaskCommon.h"
#include "InferStageModel.h"
#include "Modules/Inference/Utils/InferCacheUtils.h"
#include "Modules/Inference/Utils/InferFrameLayout.h"

#include <QDebug>
#include <QDir>
#include <QJsonDocument>

namespace Pit = ds::Api::Pitch::L1;

bool InferPitchTask::InferPitchInput::operator==(const InferPitchInput &other) const {
    return semanticSignature() == other.semanticSignature();
}

int InferPitchTask::clipId() const {
    return m_input.clipId;
}

int InferPitchTask::pieceId() const {
    return m_input.pieceId;
}

InferenceTaskContext InferPitchTask::inferenceContext() const {
    auto context = m_input.toInferenceTaskContext("pitch");
    context.taskId = id();
    context.inputSignature = m_input.semanticSignature();
    return context;
}

bool InferPitchTask::success() const {
    return m_success.load(std::memory_order_acquire);
}

InferPitchTask::InferPitchTask(InferPitchInput input) : m_input(std::move(input)),
      m_cacheDirectory(appOptions->inference()->cacheDirectory) {
    buildPreviewText();
    TaskStatus status;
    status.title = tr("Infer Pitch");
    status.message = tr("Pending infer: %1").arg(m_previewText);
    status.maximum = m_input.notes.count();
    setStatus(status);
    qCDebug(logInferTask) << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferPitchTask::InferPitchInput InferPitchTask::input() const {
    return m_input;
}

InferParamCurve InferPitchTask::result() {
    return m_result;
}

QStringList InferPitchTask::cacheFileNames() const {
    if (m_inputHash.isEmpty())
        return {};
    return InferCacheUtils::cacheFileNames(QStringLiteral("pitch"), m_inputHash).toList();
}

void InferPitchTask::runTask() {
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
    const auto cacheNames = InferCacheUtils::cacheFileNames(QStringLiteral("pitch"), m_inputHash);
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
        qInfo() << "Use cached pitch inference result:" << outputCachePath;
    } else {
        QString errorMessage;
        qCDebug(logInferTask) << "Pitch inference cache not found. Running inference...";
        if (isTerminateRequested()) {
            abort();
            return;
        }
        if (InferParam resultPitch; runInference(input, resultPitch, errorMessage)) {
            model = input;
            for (auto &param : model.params) {
                if (param.tag == "pitch") {
                    param = std::move(resultPitch);
                    break;
                }
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

    processOutput(model);
    m_success.store(true, std::memory_order_release);
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

bool InferPitchTask::runInference(const GenericInferModel &model, InferParam &outPitch,
                                  QString &error) {
    const auto &identifier = model.identifier;
    Pit::PitchStartInput input;
    input.parameters = convertInputParams(model.params);
    input.steps = model.steps;

    InferDirectMLSerializationGuard dmlGuard;
    const auto stage = InferStageModel::acquire(m_activeInference, identifier, InferStage::Pitch,
                                                "inferPitch", error);
    if (!stage)
        return false;
    auto *inferencePitch = stage->executive<Pit::PitchExecutive>();
    const auto speakerMapping = stage->speakerMapping<Pit::PitchImportOptions>();
    if (!speakerMapping) {
        qCritical() << "inferPitch: Import options not found";
        return false;
    }
    if (!convertStageWords(model, *speakerMapping, true, input.words, input.speakers, error)) {
        qCritical() << "inferPitch:" << error;
        return false;
    }

    // Run pitch
    std::unique_ptr<Pit::PitchResult> result;
    // Start inference
    if (isTerminateRequested()) {
        abort();
        return false;
    }
    auto exp = inferencePitch->start(input);
    if (!exp) {
        qCritical().noquote().nospace() << "inferPitch: Failed to start pitch inference for "
                                        << identifier << ": " << exp.error().message();
        error = QString::fromUtf8(exp.error().toString());
        return false;
    } else {
        result = exp.take();
        if (!result) {
            qCritical() << "inferPitch: result type mismatch or null result for" << identifier;
            return false;
        }
    }

    // A failure is already reported as an error by start(), so the result needs no further error
    // check. The state is checked instead: a stopped run returns a result like a completed run,
    // and using that result would treat a partial phrase as a complete phrase.
    if (inferencePitch->state() != srt::ITask::Succeeded) {
        qCritical().noquote().nospace() << "inferPitch: the pitch inference for " << identifier
                                        << " did not finish";
        return false;
    }

    outPitch.tag = "pitch";
    outPitch.interval = result->interval;
    outPitch.values.assign(result->pitch.begin(), result->pitch.end());

    return true;
}

void InferPitchTask::terminate() {
    IInferTask::terminate();
    m_activeInference.stop();
}

void InferPitchTask::abort() {
    auto newStatus = status();
    newStatus.message = tr("Terminating: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    qInfo() << "Pitch inference task terminated. clipId:" << clipId() << "pieceId:" << pieceId()
            << "taskId:" << id();
}

void InferPitchTask::buildPreviewText() {
    // TODO: maybe use lyrics for the preview text?
    for (const auto &note : m_input.notes) {
        for (const auto &phoneme : note.phonemeNames)
            m_previewText.append(phoneme.name + " ");
    }
}

QString InferPitchTask::InferPitchInput::semanticSignature() const {
    return InferInputBase::semanticSignature(
        "pitch", QJsonObject{
                     {"expressiveness", InferInputBase::paramCurveObject(expressiveness)}
    });
}

GenericInferModel InferPitchTask::InferPitchInput::toEngineModel() const {
    auto words = InferTaskHelper::buildWords(*this, true);
    double totalLength = 0;
    // The curves are sampled on the frames of the model of this stage.
    const auto interval = InferFrameLayout::inputInterval(identifier, InferStage::Pitch);
    for (const auto &word : words)
        totalLength += word.length();

    const int frames = qRound(totalLength / interval);
    InferRetake retake;
    retake.end = frames;

    InferParam param;
    param.dynamic = true;
    param.interval = interval;
    param.retake = retake;

    InferParam expr = param;
    expr.tag = "expr";
    expr.values = resampleCurveToFrames(expressiveness, frames, interval);

    InferParam pitch = param;
    pitch.tag = "pitch";
    for (int i = 0; i < frames; i++)
        pitch.values.append(0);

    GenericInferModel model;
    model.speaker = speaker;
    const auto effectiveMix =
        speakerMix.isEmpty() ? InferSpeakerMixModel::staticSpeakerMix(speaker) : speakerMix;
    model.speakerMix = InferSpeakerMixModel::fitToFrames(effectiveMix, frames, interval);
    model.words = words;
    model.params = {pitch, expr};
    model.steps = steps;
    model.identifier = identifier;
    return model;
}

bool InferPitchTask::processOutput(const GenericInferModel &model) {
    const auto oriPitch = Linq::where(model.params, L_PRED(p, p.tag == "pitch")).first();
    m_result = m_input.resampleFramesToCurve(oriPitch.values, oriPitch.interval);
    return true;
}
