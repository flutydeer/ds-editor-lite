//
// Created by fluty on 24-10-7.
//

#include "InferAcousticTask.h"

#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Utils/JsonUtils.h"
#include "Utils/MathUtils.h"

#include <QDebug>
#include <QDir>

bool InferAcousticTask::InferAcousticInput::operator==(const InferAcousticInput &other) const {
    return clipId == other.clipId && notes == other.notes && configPath == other.configPath &&
           qFuzzyCompare(tempo, other.tempo) && pitch == other.pitch &&
           breathiness == other.breathiness && tension == other.tension &&
           voicing == other.voicing && energy == other.energy && gender == other.gender &&
           velocity == other.velocity;
}

int InferAcousticTask::clipId() const {
    return m_input.clipId;
}

int InferAcousticTask::pieceId() const {
    return m_input.pieceId;
}

bool InferAcousticTask::success() const {
    return m_success;
}

InferAcousticTask::InferAcousticTask(InferAcousticInput input) : m_input(std::move(input)) {
    buildPreviewText();
    TaskStatus status;
    status.title = "推理声学模型";
    status.message = "正在等待：" + m_previewText;
    status.maximum = m_input.notes.count();
    setStatus(status);
    qDebug() << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferAcousticTask::InferAcousticInput InferAcousticTask::input() {
    return m_input;
}

QString InferAcousticTask::result() const {
    return m_result;
}

void InferAcousticTask::runTask() {
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

    QDir cacheDir("temp");
    if (!cacheDir.exists())
        if (!cacheDir.mkpath(".")) {
            qCritical() << "Failed to create temporary directory";
            return;
        }
    auto outputPath = QString("temp/infer-acoustic-output-%1.wav").arg(pieceId());
    QString errorMessage;
    if (!inferEngine->inferAcoustic(buildInputJson(), outputPath, errorMessage)) {
        qCritical() << "Task failed:" << errorMessage;
        return;
    }
    if (isTerminateRequested()) {
        abort();
        return;
    }

    m_success = true;
    m_result = outputPath;
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

void InferAcousticTask::abort() {
    auto newStatus = status();
    newStatus.message = "正在停止: " + m_previewText;
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    qInfo() << "声学模型推理任务被终止 clipId:" << clipId() << "pieceId:" << pieceId()
            << "taskId:" << id();
}

void InferAcousticTask::buildPreviewText() {
    for (const auto &note : m_input.notes) {
        for (const auto &phoneme : note.aheadNames)
            m_previewText.append(phoneme + " ");
        for (const auto &phoneme : note.normalNames)
            m_previewText.append(phoneme + " ");
    }
}

QString InferAcousticTask::buildInputJson() const {
    auto secToTick = [&](const double &sec) { return sec * 480 * m_input.tempo / 60; };

    auto words = InferTaskHelper::buildWords(m_input.notes, m_input.tempo, true);
    double totalLength = 0;
    auto interval = 0.01;
    for (const auto &word : words)
        totalLength += word.length();

    int frames = qRound(totalLength / interval);
    auto newInterval = secToTick(interval);
    InferRetake retake;
    retake.end = frames;

    InferParam param;
    param.dynamic = true;
    param.retake = retake;

    InferParam pitch = param;
    pitch.tag = "pitch";
    pitch.values = MathUtils::resample(m_input.pitch.values, 5 /*tick*/, newInterval);

    InferParam breathiness = param;
    breathiness.tag = "breathiness";
    breathiness.values = MathUtils::resample(m_input.breathiness.values, 5, newInterval);

    InferParam tension = param;
    tension.tag = "tension";
    tension.values = MathUtils::resample(m_input.tension.values, 5, newInterval);

    InferParam voicing = param;
    voicing.tag = "voicing";
    voicing.values = MathUtils::resample(m_input.voicing.values, 5, newInterval);

    InferParam energy = param;
    energy.tag = "energy";
    energy.values = MathUtils::resample(m_input.energy.values, 5, newInterval);

    InferParam gender = param;
    gender.tag = "gender";
    // gender.values = MathUtils::resample(m_input.gender.values, 5, newInterval);

    InferParam velocity = param;
    velocity.tag = "velocity";
    // velocity.values = MathUtils::resample(m_input.velocity.values, 5, newInterval);

    for (int i = 0; i < frames; i++) {
        gender.values.append(0);
        velocity.values.append(1);
    }

    GenericInferModel model;
    model.words = words;
    model.params = {pitch, breathiness, tension, voicing, energy, gender, velocity};
    JsonUtils::save(QString("infer-acoustic-input-%1.json").arg(pieceId()), model.serialize());
    return model.serializeToJson();
}