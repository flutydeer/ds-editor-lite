//
// Created by fluty on 24-10-7.
//

#include "InferAcousticTask.h"

#include <sndfile.hh>

#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Utils/JsonUtils.h"
#include "Utils/MathUtils.h"
#include "Utils/StringUtils.h"

#include "InferTaskCommon.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>

namespace Co = ds::Api::Common::L1;
namespace Ac = ds::Api::Acoustic::L1;
namespace Vo = ds::Api::Vocoder::L1;

bool InferAcousticTask::InferAcousticInput::operator==(const InferAcousticInput &other) const {
    return clipId == other.clipId && notes == other.notes && identifier == other.identifier &&
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
    return m_success.load(std::memory_order_acquire);
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
        if (!inferEngine->loadInferencesForSinger(m_input.identifier)) {
            qCritical() << "Task failed" << m_input.identifier << "clipId:" << clipId()
                        << "pieceId:" << pieceId() << "taskId:" << id();
            return;
        }
        if (isTerminateRequested()) {
            abort();
            return;
        }
        if (runInference(input, outputCachePath, errorMessage)) {
            m_result = outputCachePath;
        } else {
            qCritical() << "Task failed:" << errorMessage;
            return;
        }
    }

    m_success.store(true, std::memory_order_release);
    qInfo() << "Success:"
            << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

bool InferAcousticTask::runInference(const GenericInferModel &model, const QString &outputPath,
                                     QString &error) {
    if (!inferEngine->initialized()) {
        qCritical().noquote() << "inferAcoustic: Environment is not initialized";
        return false;
    }

    const auto &identifier = model.identifier;
    std::string speakerName = model.speaker.toStdString();
    const auto input = srt::NO<Ac::AcousticStartInput>::create();
    input->parameters = convertInputParams(model.params);
    input->depth = appOptions->inference()->depth;
    input->steps = appOptions->inference()->samplingSteps;

    srt::NO<srt::Inference> inferenceAcoustic;
    srt::NO<srt::Inference> inferenceVocoder;
    auto loader = inferEngine->findLoaderForSinger(identifier);
    if (!loader) {
        qCritical() << "inferAcoustic: Inference loader not found for" << identifier;
        return false;
    }

    // Convert singer speaker id to inference speaker id
    const auto importOptions = loader->importOptions().variance.as<Ac::AcousticImportOptions>();
    if (!importOptions) {
        qCritical() << "inferAcoustic: Import options not found";
    }
    const auto &speakerMapping = importOptions->speakerMapping;
    input->words = convertInputWords(model.words, speakerName, speakerMapping);
    if (const auto it = speakerMapping.find(speakerName); it == speakerMapping.end()) {
        if (!speakerMapping.empty()) {
            qCritical() << "inferAcoustic: Speaker mapping not found for speaker" << speakerName;
            return false;
        }
    } else {
        input->speakers = std::vector{createStaticSpeaker(it->second)};
        qDebug() << "mapped speaker" << speakerName << "to" << it->second;
    }

    // Create inference
    auto expAcoustic = loader->createAcoustic();
    auto expVocoder = loader->createVocoder();
    if (!expAcoustic || !expVocoder) {
        if (!expAcoustic) {
            qCritical().noquote().nospace() << "inferenceAcoustic: Failed to create acoustic inference for "
                                << identifier << ": " << expAcoustic.getError();
        }
        if (!expVocoder) {
            qCritical().noquote().nospace() << "inferenceAcoustic: Failed to create vocoder inference for "
                    << identifier << ": " << expVocoder.getError();
        }
        return false;
    }
    inferenceAcoustic = expAcoustic.get();
    inferenceVocoder = expVocoder.get();

    m_inferenceAcoustic = inferenceAcoustic;
    m_inferenceVocoder = inferenceVocoder;

    // Infer acoustic
    srt::NO<ds::ITensor> mel;
    srt::NO<ds::ITensor> f0;
    {
        srt::NO<Ac::AcousticResult> result;
        // Start inference
        if (isTerminateRequested()) {
            abort();
            return false;
        }
        if (auto exp = inferenceAcoustic->start(input); !exp) {
            qCritical().noquote().nospace() << "inferAcoustic: Failed to start acoustic inference for "
                                            << identifier << ": " << exp.error().message();
            return false;
        } else {
            result = exp.take().as<Ac::AcousticResult>();
        }

        if (inferenceAcoustic->state() == srt::ITask::Failed) {
            qCritical().noquote().nospace() << "inferAcoustic: Failed to run acoustic inference for "
                                            << identifier << ": " << result->error.message();
            return false;
        }
        mel = result->mel;
        f0 = result->f0;
    }
    // Run vocoder
    {
        const auto vocoderInput = srt::NO<Vo::VocoderStartInput>::create();
        vocoderInput->mel = mel;
        vocoderInput->f0 = f0;

        srt::NO<Vo::VocoderResult> result;
        // Start inference
        if (isTerminateRequested()) {
            abort();
            return false;
        }
        if (auto exp = inferenceVocoder->start(vocoderInput); !exp) {
            qCritical().noquote().nospace() << "inferAcoustic: Failed to start vocoder inference for "
                                            << identifier << ": " << exp.error().message();
            return false;
        } else {
            result = exp.take().as<Vo::VocoderResult>();
        }

        if (inferenceVocoder->state() == srt::ITask::Failed) {
            qCritical().noquote().nospace() << "inferAcoustic: Failed to run vocoder inference for "
                                            << identifier << ": " << result->error.message();
            return false;
        }
        const auto &audioRawData = result->audioData;

        const auto outputPathStr = StringUtils::qstr_to_native(outputPath);

        if (isTerminateRequested()) {
            abort();
            return false;
        }

        SndfileHandle audioFile(outputPathStr.c_str(), SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_FLOAT,
                                1, 44100);
        if (audioFile.error() != SF_ERR_NO_ERROR) {
            qDebug() << "Failed to run acoustic inference: " << audioFile.strError() << '\n';
            return false;
        }
        const auto audioData = reinterpret_cast<const float *>(audioRawData.data());
        const auto audioSize = static_cast<sf_count_t>(audioRawData.size() / sizeof(float));
        if (audioFile.write(audioData, audioSize) != audioSize) {
            qDebug() << "Failed to run acoustic inference: " << audioFile.strError() << '\n';
            return false;
        }
    }
    return true;
}

void InferAcousticTask::terminate() {
    if (m_inferenceAcoustic) {
        m_inferenceAcoustic->stop();
    }
    if (m_inferenceVocoder) {
        m_inferenceVocoder->stop();
    }
    IInferTask::terminate();
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
    model.speaker = m_input.speaker;
    model.words = words;
    model.params = {pitch, breathiness, tension, voicing, energy, mouthOpening, gender, velocity, toneShift};
    model.steps = appOptions->inference()->samplingSteps;
    model.depth = appOptions->inference()->depth;
    model.identifier = m_input.identifier;
    return model;
}