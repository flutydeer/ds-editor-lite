//
// Created by fluty on 24-10-7.
//

#include "InferAcousticTask.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Utils/JsonUtils.h"
#include "Utils/MathUtils.h"

#include "InferTaskCommon.h"

#include <QDebug>
#include <QDir>
#include <qcryptographichash.h>

bool InferAcousticTask::InferAcousticInput::operator==(const InferAcousticInput &other) const {
    return clipId == other.clipId && notes == other.notes && configPath == other.configPath &&
           qFuzzyCompare(tempo, other.tempo) && pitch == other.pitch &&
           breathiness == other.breathiness && tension == other.tension &&
           voicing == other.voicing && energy == other.energy && mouthOpening == other.mouthOpening &&
           gender == other.gender && velocity == other.velocity && toneShift == other.toneShift;
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
    setPriority(1);
    buildPreviewText();
    TaskStatus status;
    status.title = tr("Infer Acoustic");
    status.message = tr("Pending infer: %1").arg(m_previewText);
    status.maximum = m_input.notes.count();
    setStatus(status);
    qDebug() << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferAcousticTask::InferAcousticInput InferAcousticTask::input() const {
    return m_input;
}

QString InferAcousticTask::result() const {
    return m_result;
}

void InferAcousticTask::runTask() {
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
        cacheDir.filePath(QString("infer-acoustic-input-%1.json").arg(m_inputHash));
    if (!QFile(inputCachePath).exists())
        JsonUtils::save(inputCachePath, input.serialize());
    bool useCache = false;
    const auto outputCachePath =
        cacheDir.filePath(QString("infer-acoustic-output-%1.wav").arg(m_inputHash));
    if (QFile(outputCachePath).exists())
        useCache = true;

    QString errorMessage;
    if (useCache) {
        qInfo() << "Use cached acoustic inference result:" << outputCachePath;
        m_result = outputCachePath;
    } else {
        qDebug() << "acoustic inference cache not found. Running inference...";
        if (!inferEngine->runLoadConfig(m_input.configPath)) {
            qCritical() << "Task failed" << m_input.configPath << "clipId:" << clipId()
                        << "pieceId:" << pieceId() << "taskId:" << id();
            return;
        }
        if (isTerminateRequested()) {
            abort();
            return;
        }
        if (inferEngine->inferAcoustic(input, outputCachePath, errorMessage)) {
            m_result = outputCachePath;
        } else {
            qCritical() << "Task failed:" << errorMessage;
            return;
        }
    }

    m_success = true;
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

void InferAcousticTask::terminate() {
    IInferTask::terminate();
    inferEngine->terminateInferAcousticAsync();
}

void InferAcousticTask::abort() {
    auto newStatus = status();
    newStatus.message = tr("Terminating: %1").arg(m_previewText);
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

GenericInferModel InferAcousticTask::buildInputJson() const {
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

    InferParam mouthOpening = param;
    mouthOpening.tag = "mouth_opening";
    mouthOpening.values = MathUtils::resample(m_input.mouthOpening.values, 5, newInterval);

    InferParam gender = param;
    gender.tag = "gender";
    gender.values = MathUtils::resample(m_input.gender.values, 5, newInterval);

    InferParam velocity = param;
    velocity.tag = "velocity";
    velocity.values = MathUtils::resample(m_input.velocity.values, 5, newInterval);

    InferParam toneShift = param;
    toneShift.tag = "tone_shift";
    toneShift.values = MathUtils::resample(m_input.toneShift.values, 5, newInterval);

    GenericInferModel model;
    model.singer = appOptions->general()->defaultSingerId;
    model.speaker = appOptions->general()->defaultSpeakerId;
    model.words = words;
    model.params = {pitch, breathiness, tension, voicing, energy, mouthOpening, gender, velocity, toneShift};
    model.configPath = input().configPath;
    model.steps = appOptions->inference()->samplingSteps;
    model.depth = appOptions->inference()->depth;
    return model;
}