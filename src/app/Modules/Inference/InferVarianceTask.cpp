//
// Created by fluty on 24-10-5.
//

#include "InferVarianceTask.h"

#include "InferEngine.h"
#include "Model/Inference/GenericInferModel.h"
#include "Model/Inference/InferInputNote.h"
#include "Model/Inference/InferTaskHelper.h"
#include "Utils/JsonUtils.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

#include <QDebug>
#include <utility>

bool InferVarianceTask::InferVarianceInput::operator==(const InferVarianceInput &other) const {
    return clipId == other.clipId /*&& pieceId == other.pieceId*/ && notes == other.notes &&
           configPath == other.configPath && qFuzzyCompare(tempo, other.tempo) &&
           pitch == other.pitch;
}

int InferVarianceTask::clipId() const {
    return m_input.clipId;
}

int InferVarianceTask::pieceId() const {
    return m_input.pieceId;
}

bool InferVarianceTask::success() const {
    return m_success;
}

InferVarianceTask::InferVarianceTask(InferVarianceInput input) : m_input(std::move(input)) {
    buildPreviewText();
    TaskStatus status;
    status.title = "推理唱法参数";
    status.message = "正在等待：" + m_previewText;
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
    if (!inferEngine->inferVariance(buildInputJson(), resultJson, errorMessage)) {
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

void InferVarianceTask::abort() {
    auto newStatus = status();
    newStatus.message = "正在停止: " + m_previewText;
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

QString InferVarianceTask::buildInputJson() const {
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
    for (int i = 0; i < frames; i++) {
        breathiness.values.append(0);
        tension.values.append(0);
        voicing.values.append(0);
        energy.values.append(0);
    }

    GenericInferModel model;
    model.words = words;
    model.params = {pitch, breathiness, tension, voicing, energy};
    JsonUtils::save(QString("infer-variance-input-%1.json").arg(id()), model.serialize());
    return model.serializeToJson();
}

bool InferVarianceTask::processOutput(const QString &json) {
    QByteArray data = json.toUtf8();
    auto object = QJsonDocument::fromJson(data).object();
    JsonUtils::save(QString("infer-variance-output-%1.json").arg(id()), object);

    GenericInferModel model;
    if (!model.deserializeFromJson(json))
        return false;

    auto tickToSec = [&](const double &tick) { return tick * 60 / m_input.tempo / 480; };
    auto newInterval = tickToSec(5);

    auto breathiness = Linq::where(model.params, L_PRED(p, p.tag == "breathiness")).first();
    m_result.breathiness.values =
        MathUtils::resample(breathiness.values, breathiness.interval, newInterval);

    auto tension = Linq::where(model.params, L_PRED(p, p.tag == "tension")).first();
    m_result.tension.values = MathUtils::resample(tension.values, tension.interval, newInterval);

    auto voicing = Linq::where(model.params, L_PRED(p, p.tag == "voicing")).first();
    m_result.voicing.values = MathUtils::resample(voicing.values, voicing.interval, newInterval);

    auto energy = Linq::where(model.params, L_PRED(p, p.tag == "energy")).first();
    m_result.energy.values = MathUtils::resample(energy.values, energy.interval, newInterval);
    return true;
}