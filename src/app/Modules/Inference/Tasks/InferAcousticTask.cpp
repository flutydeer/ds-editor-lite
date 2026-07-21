//
// Created by fluty on 24-10-7.
//

#include "InferAcousticTask.h"

#include <sndfile.hh>

#include <diffsinger/Infer/dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <diffsinger/Infer/dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>

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
#include <QFile>

namespace Ac = srt::svs::Api::Acoustic::L1;
namespace Vo = srt::svs::Api::Vocoder::L1;

bool InferAcousticTask::InferAcousticInput::operator==(const InferAcousticInput &other) const {
    return clipId == other.clipId && notes == other.notes && identifier == other.identifier &&
           timeline == other.timeline && pitch == other.pitch && breathiness == other.breathiness &&
           tension == other.tension && voicing == other.voicing && energy == other.energy &&
           mouthOpening == other.mouthOpening && gender == other.gender &&
           velocity == other.velocity && toneShift == other.toneShift && speaker == other.speaker &&
           speakerMix == other.speakerMix;
}

int InferAcousticTask::clipId() const {
    return m_input.clipId;
}

int InferAcousticTask::pieceId() const {
    return m_input.pieceId;
}

InferenceTaskContext InferAcousticTask::inferenceContext() const {
    auto context = m_input.toInferenceTaskContext("acoustic");
    context.taskId = id();
    context.inputSignature = m_input.semanticSignature();
    return context;
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

InferAcousticTask::AcousticCacheLookup
InferAcousticTask::lookupCache(const InferAcousticInput &input) {
    AcousticCacheLookup lookup;
    lookup.model = input.toEngineModel();
    lookup.inputHash = lookup.model.hashData();

    const QDir cacheDir(appOptions->inference()->cacheDirectory);
    lookup.inputCachePath =
        cacheDir.filePath(QString("infer-acoustic-input-%1.json").arg(lookup.inputHash));
    lookup.outputCachePath =
        cacheDir.filePath(QString("infer-acoustic-output-%1.wav").arg(lookup.inputHash));

    if (!QFile::exists(lookup.outputCachePath))
        return lookup;

    const auto nativeOutputPath = StringUtils::qstr_to_native(lookup.outputCachePath);
    const SndfileHandle outputFile(nativeOutputPath.c_str());
    lookup.hit = outputFile.error() == SF_ERR_NO_ERROR &&
                 (outputFile.format() & SF_FORMAT_TYPEMASK) == SF_FORMAT_WAV &&
                 outputFile.channels() == 1 && outputFile.samplerate() == 44100 &&
                 outputFile.frames() > 0;
    return lookup;
}

void InferAcousticTask::runTask() {
    qDebug() << "Running task..."
             << "pieceId:" << pieceId() << " clipId:" << clipId() << "taskId:" << id();
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    const auto cache = lookupCache(m_input);
    m_inputHash = cache.inputHash;
    if (!QFile::exists(cache.inputCachePath))
        JsonUtils::save(cache.inputCachePath, cache.model.serialize());

    QString errorMessage;
    if (cache.hit) {
        qInfo() << "Use cached acoustic inference result:" << cache.outputCachePath;
        m_result = cache.outputCachePath;
    } else {
        qDebug() << "acoustic inference cache not found. Running inference...";
        if (isTerminateRequested()) {
            abort();
            return;
        }
        if (runInference(cache.model, cache.outputCachePath, errorMessage)) {
            m_result = cache.outputCachePath;
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
    const auto input = srt::core::NO<Ac::AcousticStartInput>::create();
    input->parameters = convertInputParams(model.params);
    input->depth = model.depth;
    input->steps = model.steps;

    const auto session = inferEngine->acquireSingerSession(identifier);
    if (!session) {
        qCritical() << "inferAcoustic: failed to acquire singer session for" << identifier;
        return false;
    }
    // Infer acoustic
    srt::core::NO<srt::core::ITensor> mel;
    srt::core::NO<srt::core::ITensor> f0;
    {
        auto acousticExp = m_activeInference.acquire(session, ds::infer::StageKind::Acoustic);
        if (!acousticExp) {
            qCritical().noquote().nospace()
                << "inferAcoustic: failed to load acoustic model for " << identifier << ": "
                << QString::fromUtf8(acousticExp.error().message());
            return false;
        }
        auto activeInference = acousticExp.take();
        auto &acousticModel = activeInference.model();
        auto inferenceAcoustic = acousticModel.inference;
        if (!inferenceAcoustic) {
            qCritical() << "inferAcoustic: Acoustic inference not found for" << identifier;
            return false;
        }

        if (!acousticModel.importOptions) {
            qCritical() << "inferAcoustic: Import options not found";
            return false;
        }
        const auto importOptions = acousticModel.importOptions.as<Ac::AcousticImportOptions>();
        if (!importOptions) {
            qCritical() << "inferAcoustic: Import options not found";
            return false;
        }
        const auto &speakerMapping = importOptions->speakerMapping;
        input->words = convertInputWords(model.words, speakerName, model.speakerMix, speakerMapping, error);
        if (!error.isEmpty()) {
            qCritical() << "inferAcoustic:" << error;
            return false;
        }
        input->speakers = convertInputSpeakers(model.speakerMix, speakerMapping, error);
        if (!error.isEmpty()) {
            qCritical() << "inferAcoustic:" << error;
            return false;
        }

        srt::core::NO<Ac::AcousticResult> result;
        // Start inference
        if (isTerminateRequested()) {
            abort();
            return false;
        }
        auto exp = inferenceAcoustic->start(input);
        if (!exp) {
            qCritical().noquote().nospace()
                << "inferAcoustic: Failed to start acoustic inference for " << identifier << ": "
                << exp.error().message();
            return false;
        } else {
            result = exp.take().as<Ac::AcousticResult>();
            if (!result) {
                qCritical() << "inferAcoustic: acoustic result type mismatch or null result for"
                            << identifier;
                return false;
            }
        }

        if (!result->error.ok()) {
            qCritical().noquote().nospace()
                << "inferAcoustic: Failed to run acoustic inference for " << identifier << ": "
                << result->error.message();
            return false;
        }

        if (inferenceAcoustic->state() == srt::core::ITask::Failed) {
            qCritical().noquote().nospace()
                << "inferAcoustic: Failed to run acoustic inference for " << identifier << ": "
                << result->error.message();
            return false;
        }
        mel = result->mel;
        f0 = result->f0;
    }
    // Run vocoder
    {
        auto vocoderExp = m_activeInference.acquire(session, ds::infer::StageKind::Vocoder);
        if (!vocoderExp) {
            qCritical().noquote().nospace()
                << "inferAcoustic: failed to load vocoder model for " << identifier << ": "
                << QString::fromUtf8(vocoderExp.error().message());
            return false;
        }
        auto activeInference = vocoderExp.take();
        auto inferenceVocoder = activeInference.model().inference;
        if (!inferenceVocoder) {
            qCritical() << "inferAcoustic: Vocoder inference not found for" << identifier;
            return false;
        }
        const auto vocoderInput = srt::core::NO<Vo::VocoderStartInput>::create();
        vocoderInput->mel = mel;
        vocoderInput->f0 = f0;

        srt::core::NO<Vo::VocoderResult> result;
        // Start inference
        if (isTerminateRequested()) {
            abort();
            return false;
        }
        auto exp = inferenceVocoder->start(vocoderInput);
        if (!exp) {
            qCritical().noquote().nospace()
                << "inferAcoustic: Failed to start vocoder inference for " << identifier << ": "
                << exp.error().message();
            return false;
        } else {
            result = exp.take().as<Vo::VocoderResult>();
            if (!result) {
                qCritical() << "inferAcoustic: vocoder result type mismatch or null result for"
                            << identifier;
                return false;
            }
        }

        if (!result->error.ok()) {
            qCritical().noquote().nospace() << "inferAcoustic: Failed to run vocoder inference for "
                                            << identifier << ": " << result->error.message();
            return false;
        }

        if (inferenceVocoder->state() == srt::core::ITask::Failed) {
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
    IInferTask::terminate();
    m_activeInference.stop();
}

void InferAcousticTask::abort() {
    auto newStatus = status();
    newStatus.message = tr("Terminating: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);
    qInfo() << "Acoustic model inference task has been terminated. clipId:" << clipId()
            << "pieceId:" << pieceId() << "taskId:" << id();
}

void InferAcousticTask::buildPreviewText() {
    for (const auto &note : m_input.notes) {
        for (const auto &phoneme : note.phonemeNames)
            m_previewText.append(phoneme.name + " ");
    }
}

QString InferAcousticTask::InferAcousticInput::semanticSignature() const {
    return InferInputBase::semanticSignature(
        "acoustic", QJsonObject{
                        {"pitch",        InferInputBase::doubleArray(pitch.values)       },
                        {"breathiness",  InferInputBase::doubleArray(breathiness.values) },
                        {"tension",      InferInputBase::doubleArray(tension.values)     },
                        {"voicing",      InferInputBase::doubleArray(voicing.values)     },
                        {"energy",       InferInputBase::doubleArray(energy.values)      },
                        {"mouthOpening", InferInputBase::doubleArray(mouthOpening.values)},
                        {"gender",       InferInputBase::doubleArray(gender.values)      },
                        {"velocity",     InferInputBase::doubleArray(velocity.values)    },
                        {"toneShift",    InferInputBase::doubleArray(toneShift.values)   },
    });
}

GenericInferModel InferAcousticTask::InferAcousticInput::toEngineModel() const {
    auto words = InferTaskHelper::buildWords(*this, true);
    double totalLength = 0;
    auto interval = 0.01;
    for (const auto &word : words)
        totalLength += word.length();

    int frames = qRound(totalLength / interval);
    auto newInterval = timeline.secToTick(interval);
    InferRetake retake;
    retake.end = frames;

    InferParam param;
    param.dynamic = true;
    param.retake = retake;

    InferParam pitch = param;
    pitch.tag = "pitch";
    pitch.values = MathUtils::resample(this->pitch.values, 5 /*tick*/, newInterval);

    InferParam breathiness = param;
    breathiness.tag = "breathiness";
    breathiness.values = MathUtils::resample(this->breathiness.values, 5, newInterval);

    InferParam tension = param;
    tension.tag = "tension";
    tension.values = MathUtils::resample(this->tension.values, 5, newInterval);

    InferParam voicing = param;
    voicing.tag = "voicing";
    voicing.values = MathUtils::resample(this->voicing.values, 5, newInterval);

    InferParam energy = param;
    energy.tag = "energy";
    energy.values = MathUtils::resample(this->energy.values, 5, newInterval);

    InferParam mouthOpening = param;
    mouthOpening.tag = "mouth_opening";
    mouthOpening.values = MathUtils::resample(this->mouthOpening.values, 5, newInterval);

    InferParam gender = param;
    gender.tag = "gender";
    gender.values = MathUtils::resample(this->gender.values, 5, newInterval);

    InferParam velocity = param;
    velocity.tag = "velocity";
    velocity.values = MathUtils::resample(this->velocity.values, 5, newInterval);

    InferParam toneShift = param;
    toneShift.tag = "tone_shift";
    toneShift.values = MathUtils::resample(this->toneShift.values, 5, newInterval);

    GenericInferModel model;
    model.speaker = speaker;
    model.speakerMix =
        speakerMix.isEmpty() ? InferSpeakerMixModel::staticSpeakerMix(speaker) : speakerMix;
    model.words = words;
    model.params = {pitch,        breathiness, tension,  voicing,  energy,
                    mouthOpening, gender,      velocity, toneShift};
    model.steps = steps;
    model.depth = static_cast<float>(depth);
    model.identifier = identifier;
    return model;
}
