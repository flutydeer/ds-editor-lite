//
// Created by fluty on 24-10-5.
//

#include "InferVarianceTask.h"

#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Utils/JsonUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"
#include "InferTaskCommon.h"

#include <QDebug>
#include <QDir>
#include <utility>

namespace Co = ds::Api::Common::L1;
namespace Var = ds::Api::Variance::L1;

bool InferVarianceTask::InferVarianceInput::operator==(const InferVarianceInput &other) const {
    return clipId == other.clipId /*&& pieceId == other.pieceId*/ && notes == other.notes &&
           identifier == other.identifier && qFuzzyCompare(tempo, other.tempo) &&
           pitch == other.pitch;
}

int InferVarianceTask::clipId() const {
    return m_input.clipId;
}

int InferVarianceTask::pieceId() const {
    return m_input.pieceId;
}

bool InferVarianceTask::success() const {
    return m_success.load(std::memory_order_acquire);
}

InferVarianceTask::InferVarianceTask(InferVarianceInput input) : m_input(std::move(input)) {
    buildPreviewText();
    TaskStatus status;
    status.title = tr("Infer Variance");
    status.message = tr("Pending infer: %1").arg(m_previewText);
    status.maximum = m_input.notes.count();
    setStatus(status);
    qDebug() << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferVarianceTask::InferVarianceInput InferVarianceTask::input() const {
    return m_input;
}

InferVarianceTask::InferVarianceResult InferVarianceTask::result() const {
    return m_result;
}

void InferVarianceTask::runTask() {
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
        cacheDir.filePath(QString("infer-variance-input-%1.json").arg(m_inputHash));
    if (!QFile(inputCachePath).exists())
        JsonUtils::save(inputCachePath, input.serialize());
    bool useCache = false;
    const auto outputCachePath =
        cacheDir.filePath(QString("infer-variance-output-%1.json").arg(m_inputHash));
    if (QFile(outputCachePath).exists()) {
        QJsonObject obj;
        useCache = JsonUtils::load(outputCachePath, obj) && model.deserialize(obj);
    }

    if (useCache) {
        qInfo() << "Use cached variance inference result:" << outputCachePath;
    } else {
        QString errorMessage;
        qDebug() << "Variance inference cache not found. Running inference...";
        if (!inferEngine->loadInferencesForSinger(m_input.identifier)) {
            qCritical() << "Task failed" << m_input.identifier << "clipId:" << clipId()
                        << "pieceId:" << pieceId() << "taskId:" << id();
            return;
        }
        if (isTerminateRequested()) {
            abort();
            return;
        }
        if (QList<InferParam> outParams; runInference(input, outParams, errorMessage)) {
            model = input;
            for (auto &param : model.params) {
                for (auto &outParam : outParams) {
                    if (param.tag == outParam.tag) {
                        param = outParam;
                        break;
                    }
                }
            }
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
    m_success.store(true, std::memory_order_release);
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

bool InferVarianceTask::runInference(const GenericInferModel &model, QList<InferParam> &outParams,
                                     QString &error) {
    if (!inferEngine->initialized()) {
        qCritical().noquote() << "inferVariance: Environment is not initialized";
        return false;
    }

    const auto &identifier = model.identifier;
    std::string speakerName = model.speaker.toStdString();
    const auto input = srt::NO<Var::VarianceStartInput>::create();
    input->parameters = convertInputParams(model.params);
    input->steps = appOptions->inference()->samplingSteps;

    srt::NO<srt::Inference> inferenceVariance;
    auto loader = inferEngine->findLoaderForSinger(identifier);
    if (!loader) {
        qCritical() << "inferVariance: Inference loader not found for" << identifier;
        return false;
    }

    // Convert singer speaker id to inference speaker id
    const auto importOptions = loader->importOptions().variance.as<Var::VarianceImportOptions>();
    if (!importOptions) {
        qCritical() << "inferVariance: Import options not found";
    }
    const auto &speakerMapping = importOptions->speakerMapping;
    input->words = convertInputWords(model.words, speakerName, speakerMapping);
    if (const auto it = speakerMapping.find(speakerName); it == speakerMapping.end()) {
        if (!speakerMapping.empty()) {
            qCritical() << "inferVariance: Speaker mapping not found for speaker" << speakerName;
            return false;
        }
    } else {
        input->speakers = std::vector{createStaticSpeaker(it->second)};
        qDebug() << "mapped speaker" << speakerName << "to" << it->second;
    }

    // Create inference
    if (auto exp = loader->createVariance(); !exp) {
        qCritical().noquote().nospace() << "inferVariance: Failed to create variance inference for "
                                        << identifier << ": " << exp.getError();
        return false;
    } else {
        inferenceVariance = exp.get();
    }
    m_inferenceVariance = inferenceVariance;

    // Run variance
    srt::NO<Var::VarianceResult> result;
    // Start inference
    if (isTerminateRequested()) {
        abort();
        return false;
    }
    if (auto exp = inferenceVariance->start(input); !exp) {
        qCritical().noquote().nospace() << "inferVariance: Failed to start variance inference for "
                                        << identifier << ": " << exp.error().message();
        return false;
    } else {
        result = exp.take().as<Var::VarianceResult>();
    }

    if (inferenceVariance->state() == srt::ITask::Failed) {
        qCritical().noquote().nospace() << "inferVariance: Failed to run variance inference for "
                                        << identifier << ": " << result->error.message();
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
    if (m_inferenceVariance) {
        m_inferenceVariance->stop();
    }
    IInferTask::terminate();
}

void InferVarianceTask::abort() {
    auto newStatus = status();
    newStatus.message = tr("Terminating: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    qInfo() << "唱法参数推理任务被终止 clipId:" << clipId() << "pieceId:" << pieceId()
            << "taskId:" << id();
}

void InferVarianceTask::buildPreviewText() {
    for (const auto &note : m_input.notes) {
        for (const auto &phoneme : note.aheadNames)
            m_previewText.append(phoneme + " ");
        for (const auto &phoneme : note.normalNames)
            m_previewText.append(phoneme + " ");
    }
}

GenericInferModel InferVarianceTask::buildInputJson() const {
    auto secToTick = [&](const double &sec) { return sec * 480 * m_input.tempo / 60; };

    auto words = InferTaskHelper::buildWords(m_input.notes, m_input.tempo, true);
    double totalLength = 0;
    auto interval = 0.01;
    for (const auto &word : words)
        totalLength += word.length();

    int frames = qRound(totalLength / interval);
    InferRetake retake;
    retake.end = frames;

    InferParam param;
    param.dynamic = true;
    param.retake = retake;

    InferParam pitch = param;
    pitch.tag = "pitch";
    pitch.values =
        MathUtils::resample(m_input.pitch.values, 5 /*tick*/, secToTick(interval)); // 重采样

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
    model.speaker = m_input.speaker;
    model.words = words;
    model.params = {pitch, breathiness, tension, voicing, energy, mouthOpening};
    model.steps = appOptions->inference()->samplingSteps;
    model.identifier = m_input.identifier;
    return model;
}

bool InferVarianceTask::processOutput(const GenericInferModel &model) {
    auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };
    const auto newInterval = tickToSec(5);

    const auto breathiness = Linq::where(model.params, L_PRED(p, p.tag == "breathiness")).first();
    m_result.breathiness.values =
        MathUtils::resample(breathiness.values, breathiness.interval, newInterval);

    const auto tension = Linq::where(model.params, L_PRED(p, p.tag == "tension")).first();
    m_result.tension.values = MathUtils::resample(tension.values, tension.interval, newInterval);

    const auto voicing = Linq::where(model.params, L_PRED(p, p.tag == "voicing")).first();
    m_result.voicing.values = MathUtils::resample(voicing.values, voicing.interval, newInterval);

    const auto energy = Linq::where(model.params, L_PRED(p, p.tag == "energy")).first();
    m_result.energy.values = MathUtils::resample(energy.values, energy.interval, newInterval);

    const auto mouthOpening =
        Linq::where(model.params, L_PRED(p, p.tag == "mouth_opening")).first();
    m_result.mouthOpening.values =
        MathUtils::resample(mouthOpening.values, mouthOpening.interval, newInterval);
    return true;
}