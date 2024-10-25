//
// Created by fluty on 24-10-2.
//

#include "InferPitchTask.h"

#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Utils/JsonUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <QDebug>
#include <QJsonDocument>

bool InferPitchTask::InferPitchInput::operator==(const InferPitchInput &other) const {
    return clipId == other.clipId /*&& pieceId == other.pieceId*/ && notes == other.notes &&
           configPath == other.configPath && qFuzzyCompare(tempo, other.tempo) &&
           expressiveness == other.expressiveness;
}

int InferPitchTask::clipId() const {
    return m_input.clipId;
}

int InferPitchTask::pieceId() const {
    return m_input.pieceId;
}

bool InferPitchTask::success() const {
    return m_success;
}

InferPitchTask::InferPitchTask(InferPitchInput input) : m_input(std::move(input)) {
    buildPreviewText();
    TaskStatus status;
    status.title = "推理音高参数";
    status.message = "正在等待：" + m_previewText;
    status.maximum = m_input.notes.count();
    setStatus(status);
    qDebug() << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferPitchTask::InferPitchInput InferPitchTask::input() {
    return m_input;
}

InferParamCurve InferPitchTask::result() {
    return m_result;
}

void InferPitchTask::runTask() {
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

    GenericInferModel model;
    auto input = buildInputJson();
    m_inputHash = input.hashData();
    JsonUtils::save(QString("temp/infer-pitch-input-%1.json").arg(m_inputHash),
                    input.serialize());
    bool useCache = false;
    auto cachePath = QString("temp/infer-pitch-output-%1.json").arg(m_inputHash);
    if (QFile(cachePath).exists()) {
        QJsonObject obj;
        useCache = JsonUtils::load(cachePath, obj) && model.deserialize(obj);
    }

    QString resultJson;
    QString errorMessage;
    if (useCache) {
        qInfo() << "Use cached pitch inference result:" << cachePath;
    } else {
        qDebug() << "Pitch inference cache not found. Running inference...";
        if (inferEngine->inferPitch(input.serializeToJson(), resultJson, errorMessage)) {
            model.deserializeFromJson(resultJson);
        } else {
            qCritical() << "Task failed:" << errorMessage;
            return;
        }
    }

    if (isTerminateRequested()) {
        abort();
        return;
    }

    JsonUtils::save(cachePath, model.serialize());
    processOutput(model);
    m_success = true;
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

void InferPitchTask::terminate() {
    IInferTask::terminate();
    inferEngine->terminateInferPitchAsync();
}

void InferPitchTask::abort() {
    auto newStatus = status();
    newStatus.message = "正在停止: " + m_previewText;
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
    auto interval = 0.01;
    for (const auto &word : words)
        totalLength += word.length();

    auto newInterval = secToTick(interval);
    int frames = qRound(totalLength / interval);
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
    model.words = words;
    model.params = {pitch, expr};
    return model;
}

bool InferPitchTask::processOutput(const GenericInferModel &model) {
    auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };
    auto oriPitch = Linq::where(model.params, L_PRED(p, p.tag == "pitch")).first();
    auto newInterval = tickToSec(5);
    m_result.values = MathUtils::resample(oriPitch.values, oriPitch.interval, newInterval);
    return true;
}