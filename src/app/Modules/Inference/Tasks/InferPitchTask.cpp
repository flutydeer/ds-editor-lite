//
// Created by fluty on 24-10-2.
//

#include "InferPitchTask.h"

#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Utils/JsonUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"
#include "InferTaskCommon.h"

#include <QDebug>
#include <QDir>
#include <QJsonDocument>

namespace Co = ds::Api::Common::L1;
namespace Pit = ds::Api::Pitch::L1;

bool InferPitchTask::InferPitchInput::operator==(const InferPitchInput &other) const {
    return clipId == other.clipId /*&& pieceId == other.pieceId*/ && notes == other.notes &&
           identifier == other.identifier && qFuzzyCompare(tempo, other.tempo) &&
           expressiveness == other.expressiveness;
}

int InferPitchTask::clipId() const {
    return m_input.clipId;
}

int InferPitchTask::pieceId() const {
    return m_input.pieceId;
}

bool InferPitchTask::success() const {
    return m_success.load(std::memory_order_acquire);
}

InferPitchTask::InferPitchTask(InferPitchInput input) : m_input(std::move(input)) {
    buildPreviewText();
    TaskStatus status;
    status.title = tr("Infer Pitch");
    status.message = tr("Pending infer: %1").arg(m_previewText);
    status.maximum = m_input.notes.count();
    setStatus(status);
    qDebug() << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferPitchTask::InferPitchInput InferPitchTask::input() const {
    return m_input;
}

InferParamCurve InferPitchTask::result() {
    return m_result;
}

void InferPitchTask::runTask() {
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
        cacheDir.filePath(QString("infer-pitch-input-%1.json").arg(m_inputHash));
    if (!QFile(inputCachePath).exists())
        JsonUtils::save(inputCachePath, input.serialize());
    bool useCache = false;
    const auto outputCachePath =
        cacheDir.filePath(QString("infer-pitch-output-%1.json").arg(m_inputHash));
    if (QFile(outputCachePath).exists()) {
        QJsonObject obj;
        useCache = JsonUtils::load(outputCachePath, obj) && model.deserialize(obj);
    }

    if (useCache) {
        qInfo() << "Use cached pitch inference result:" << outputCachePath;
    } else {
        QString errorMessage;
        qDebug() << "Pitch inference cache not found. Running inference...";
        if (!inferEngine->loadInferencesForSinger(m_input.identifier)) {
            qCritical() << "Task failed" << m_input.identifier << "clipId:" << clipId()
                        << "pieceId:" << pieceId() << "taskId:" << id();
            return;
        }
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

bool InferPitchTask::runInference(const GenericInferModel &model, InferParam &outPitch,
                             QString &error) {
    if (!inferEngine->initialized()) {
        qCritical().noquote() << "inferPitch: Environment is not initialized";
        return false;
    }

    const auto &identifier = model.identifier;
    std::string speakerName = model.speaker.toStdString();
    const auto input = srt::NO<Pit::PitchStartInput>::create();
    input->parameters = convertInputParams(model.params);
    input->steps = appOptions->inference()->samplingSteps;

    srt::NO<srt::Inference> inferencePitch;
    auto loader = inferEngine->findLoaderForSinger(identifier);
    if (!loader) {
        qCritical() << "inferPitch: Inference loader not found for" << identifier;
        return false;
    }

    // Convert singer speaker id to inference speaker id
    const auto importOptions = loader->importOptions().pitch.as<Pit::PitchImportOptions>();
    if (!importOptions) {
        qCritical() << "inferPitch: Import options not found";
    }
    const auto &speakerMapping = importOptions->speakerMapping;
    input->words = convertInputWords(model.words, speakerName, speakerMapping);
    if (const auto it = speakerMapping.find(speakerName); it == speakerMapping.end()) {
        if (!speakerMapping.empty()) {
            qCritical() << "inferPitch: Speaker mapping not found for speaker" << speakerName;
            return false;
        }
    } else {
        input->speakers = std::vector{createStaticSpeaker(it->second)};
        qDebug() << "mapped speaker" << speakerName << "to" << it->second;
    }

    // Create inference
    if (auto exp = loader->createPitch(); !exp) {
        qCritical().noquote().nospace() << "inferPitch: Failed to create pitch inference for "
                                        << identifier << ": " << exp.getError();
        return false;
    } else {
        inferencePitch = exp.get();
    }
    m_inferencePitch = inferencePitch;

    // Run pitch
    srt::NO<Pit::PitchResult> result;
    // Start inference
    if (isTerminateRequested()) {
        abort();
        return false;
    }
    if (auto exp = inferencePitch->start(input); !exp) {
        qCritical().noquote().nospace() << "inferPitch: Failed to start pitch inference for "
                                        << identifier << ": " << exp.error().message();
        return false;
    } else {
        result = exp.take().as<Pit::PitchResult>();
    }

    if (inferencePitch->state() == srt::ITask::Failed) {
        qCritical().noquote().nospace() << "inferPitch: Failed to run pitch inference for "
                                        << identifier << ": " << result->error.message();
        return false;
    }

    outPitch.tag = "pitch";
    outPitch.interval = result->interval;
    outPitch.values.assign(result->pitch.begin(), result->pitch.end());

    return true;
}

void InferPitchTask::terminate() {
    if (m_inferencePitch) {
        m_inferencePitch->stop();
    }
    IInferTask::terminate();
}

void InferPitchTask::abort() {
    auto newStatus = status();
    newStatus.message = tr("Terminating: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    qInfo() << "音高参数推理任务被终止 clipId:" << clipId() << "pieceId:" << pieceId()
            << "taskId:" << id();
}

void InferPitchTask::buildPreviewText() {
    // 可能用歌词会比较好？
    for (const auto &note : m_input.notes) {
        for (const auto &phoneme : note.aheadNames)
            m_previewText.append(phoneme + " ");
        for (const auto &phoneme : note.normalNames)
            m_previewText.append(phoneme + " ");
    }
}

GenericInferModel InferPitchTask::buildInputJson() const {
    auto secToTick = [&](const double &sec) { return sec * 480 * m_input.tempo / 60; };
    auto words = InferTaskHelper::buildWords(m_input.notes, m_input.tempo, true);
    double totalLength = 0;
    constexpr auto interval = 0.01;
    for (const auto &word : words)
        totalLength += word.length();

    const auto newInterval = secToTick(interval);
    const int frames = qRound(totalLength / interval);
    InferRetake retake;
    retake.end = frames;

    InferParam param;
    param.dynamic = true;
    param.retake = retake;

    InferParam expr = param;
    expr.tag = "expr";
    expr.values = MathUtils::resample(m_input.expressiveness.values, 5, newInterval);

    InferParam pitch = param;
    pitch.tag = "pitch";
    for (int i = 0; i < frames; i++)
        pitch.values.append(0);

    GenericInferModel model;
    model.speaker = m_input.speaker;
    model.words = words;
    model.params = {pitch, expr};
    model.steps = appOptions->inference()->samplingSteps;
    model.identifier = m_input.identifier;
    return model;
}

bool InferPitchTask::processOutput(const GenericInferModel &model) {
    auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };
    const auto oriPitch = Linq::where(model.params, L_PRED(p, p.tag == "pitch")).first();
    const auto newInterval = tickToSec(5);
    m_result.values = MathUtils::resample(oriPitch.values, oriPitch.interval, newInterval);
    return true;
}