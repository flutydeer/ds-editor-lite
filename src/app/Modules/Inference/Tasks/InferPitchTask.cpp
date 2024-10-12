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

    QString resultJson;
    QString errorMessage;
    if (!inferEngine->inferPitch(buildInputJson(), resultJson, errorMessage)) {
        qCritical() << "Task failed:" << errorMessage;
        return;
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

QString InferPitchTask::buildInputJson() const {
    // auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };
    auto words = InferTaskHelper::buildWords(m_input.notes, m_input.tempo, true);
    double totalLength = 0;
    for (const auto &word : words)
        totalLength += word.length();
    InferParam pitch;
    pitch.tag = "pitch";
    pitch.dynamic = true;

    int frames = qRound(totalLength / pitch.interval);
    InferRetake retake;
    retake.end = frames;

    pitch.retake = retake;
    for (int i = 0; i < frames; i++)
        pitch.values.append(0);

    GenericInferModel model;
    model.words = words;
    model.params = {pitch};
    JsonUtils::save(QString("temp/infer-pitch-input-%1.json").arg(pieceId()), model.serialize());
    return model.serializeToJson();
}

bool InferPitchTask::processOutput(const QString &json) {
    // QByteArray data = json.toUtf8();
    // auto object = QJsonDocument::fromJson(data).object();
    // JsonUtils::save(QString("infer-pitch-output-%1.json").arg(id()), object);

    GenericInferModel model;
    if (!model.deserializeFromJson(json))
        return false;

    auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };
    auto oriPitch = Linq::where(model.params, L_PRED(p, p.tag == "pitch")).first();
    auto newInterval = tickToSec(5);
    m_result.values = MathUtils::resample(oriPitch.values, oriPitch.interval, newInterval);
    return true;
}