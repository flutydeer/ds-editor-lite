#include "InferAcousticTask.h"

#include "Modules/Inference/InferLogging.h"

#include <sndfile.hh>

#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>
#include <dsinfer/Core/Tensor.h>
#include <synthrt/SVS/InferenceContrib.h>

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Inference/Models/InferInputNote.h"
#include "Modules/Inference/InferEngine.h"
#include "Modules/Inference/Models/GenericInferModel.h"
#include "Modules/Inference/Utils/InferTaskHelper.h"
#include "Modules/Inference/Utils/PitchRouting.h"
#include <lite/Support/JsonUtils.h>
#include <lite/Support/StringUtils.h>

#include "InferTaskCommon.h"
#include "InferStageModel.h"
#include "Modules/Inference/Utils/InferCacheUtils.h"
#include "Modules/Inference/Utils/InferFrameLayout.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>

#include <algorithm>

namespace Ac = ds::Api::Acoustic::L1;
namespace Vo = ds::Api::Vocoder::L1;

bool InferAcousticTask::InferAcousticInput::operator==(const InferAcousticInput &other) const {
    return semanticSignature() == other.semanticSignature();
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

InferAcousticTask::InferAcousticTask(InferAcousticInput input) : m_input(std::move(input)),
      m_cacheDirectory(appOptions->inference()->cacheDirectory) {
    setPriority(1);
    buildPreviewText();
    TaskStatus status;
    status.title = tr("Infer Acoustic");
    status.message = tr("Pending infer: %1").arg(m_previewText);
    status.maximum = m_input.notes.count();
    setStatus(status);
    qCDebug(logInferTask) << "Task created"
             << "clipId:" << clipId() << "pieceId:" << pieceId() << "taskId:" << id();
}

InferAcousticTask::InferAcousticInput InferAcousticTask::input() const {
    return m_input;
}

QString InferAcousticTask::result() const {
    return m_result;
}

QStringList InferAcousticTask::cacheFileNames() const {
    if (m_inputHash.isEmpty())
        return {};
    return InferCacheUtils::cacheFileNames(QStringLiteral("acoustic"), m_inputHash).toList();
}

InferAcousticTask::AcousticCacheLookup
    InferAcousticTask::lookupCache(const InferAcousticInput &input, const QString &cacheDirectory) {
    AcousticCacheLookup lookup;
    lookup.model = input.toEngineModel();
    lookup.inputHash = lookup.model.hashData();

    const QDir cacheDir(cacheDirectory);
    if (!cacheDir.exists())
        cacheDir.mkpath(".");
    const auto names = InferCacheUtils::cacheFileNames(QStringLiteral("acoustic"), lookup.inputHash);
    lookup.inputCachePath = cacheDir.filePath(names.input);
    lookup.outputCachePath = cacheDir.filePath(names.output);

    if (!QFile::exists(lookup.outputCachePath))
        return lookup;

    // The rendered file has the sample rate of the vocoder that produced it. The cache key
    // identifies the singer and its package version, and therefore the vocoder. A file whose
    // rate differs from the rate that the catalog declares for that vocoder is therefore not a
    // result of this input.
    const auto nativeOutputPath = StringUtils::qstr_to_native(lookup.outputCachePath);
    const SndfileHandle outputFile(nativeOutputPath.c_str());
    const auto expectedRate = InferFrameLayout::vocoderSampleRate(input.identifier);
    lookup.hit = outputFile.error() == SF_ERR_NO_ERROR &&
                 (outputFile.format() & SF_FORMAT_TYPEMASK) == SF_FORMAT_WAV &&
                 outputFile.channels() == 1 && outputFile.samplerate() > 0 &&
                 (expectedRate <= 0 || outputFile.samplerate() == expectedRate) &&
                 outputFile.frames() > 0;
    return lookup;
}

void InferAcousticTask::runTask() {
    qCDebug(logInferTask) << "Running task..."
             << "pieceId:" << pieceId() << " clipId:" << clipId() << "taskId:" << id();
    auto newStatus = status();
    newStatus.message = tr("Running inference: %1").arg(m_previewText);
    newStatus.isIndetermine = true;
    setStatus(newStatus);

    const auto cache = lookupCache(m_input, m_cacheDirectory);
    m_inputHash = cache.inputHash;
    if (!QFile::exists(cache.inputCachePath))
        JsonUtils::save(cache.inputCachePath, cache.model.serialize());

    QString errorMessage;
    if (cache.hit) {
        qInfo() << "Use cached acoustic inference result:" << cache.outputCachePath;
        m_result = cache.outputCachePath;
    } else {
        qCDebug(logInferTask) << "acoustic inference cache not found. Running inference...";
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
    const auto &identifier = model.identifier;
    const auto paramWithTag = [&model](const QString &tag) -> const InferParam * {
        const auto it = std::find_if(model.params.cbegin(), model.params.cend(),
                                     [&tag](const auto &param) { return param.tag == tag; });
        return it == model.params.cend() ? nullptr : &*it;
    };
    const auto vocoderPitch = paramWithTag(QString::fromLatin1(PitchRouting::VocoderPitchTag));
    const auto acousticPitch = paramWithTag(QStringLiteral("pitch"));
    if (!vocoderPitch || vocoderPitch->values.isEmpty() || !acousticPitch) {
        error = tr("Pitch routing data is missing");
        qCritical() << "inferAcoustic:" << error;
        return false;
    }

    Ac::AcousticStartInput input;
    input.parameters = convertInputParams(model.params);
    input.depth = model.depth;
    input.steps = model.steps;

    InferDirectMLSerializationGuard dmlGuard;
    // The pipeline of the acoustic stage also supplies the vocoder, so both run on one pipeline.
    std::shared_ptr<InferEngine::SingerPipelineLease> lease;
    // Infer acoustic
    std::shared_ptr<ds::ITensor> mel;
    std::shared_ptr<ds::ITensor> acousticF0;
    double acousticFrameWidth = 0;
    {
        const auto stage = InferStageModel::acquire(
            m_activeInference, identifier, InferStage::Acoustic, "inferAcoustic", error);
        if (!stage)
            return false;
        lease = stage->lease();
        auto *inferenceAcoustic = stage->executive<Ac::AcousticExecutive>();

        const auto *acousticConfig = static_cast<const Ac::AcousticConfiguration *>(
            inferenceAcoustic->spec().configuration());
        if (!acousticConfig || acousticConfig->sampleRate <= 0 || acousticConfig->hopSize <= 0) {
            error = tr("Acoustic model frame configuration is invalid");
            qCritical() << "inferAcoustic:" << error;
            return false;
        }
        acousticFrameWidth = static_cast<double>(acousticConfig->hopSize) /
                             static_cast<double>(acousticConfig->sampleRate);

        const auto speakerMapping = stage->speakerMapping<Ac::AcousticImportOptions>();
        if (!speakerMapping) {
            qCritical() << "inferAcoustic: Import options not found";
            return false;
        }
        if (!convertStageWords(model, *speakerMapping, true, input.words, input.speakers,
                               error)) {
            qCritical() << "inferAcoustic:" << error;
            return false;
        }

        std::unique_ptr<Ac::AcousticResult> result;
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
            error = QString::fromUtf8(exp.error().toString());
            return false;
        } else {
            result = exp.take();
            if (!result) {
                qCritical() << "inferAcoustic: acoustic result type mismatch or null result for"
                            << identifier;
                return false;
            }
        }

        // A failure is already reported as an error by start(), so the result needs no further
        // error check. The state is checked instead: a stopped run returns a result like a
        // completed run, and using that result would treat a partial phrase as a complete phrase.
        if (inferenceAcoustic->state() != srt::ITask::Succeeded) {
            qCritical().noquote().nospace() << "inferAcoustic: the acoustic inference for "
                                            << identifier << " did not finish";
            return false;
        }
        mel = result->mel;
        acousticF0 = result->f0;
        if (!acousticF0 || acousticF0->dataType() != ds::ITensor::Float ||
            acousticF0->elementCount() == 0) {
            error = tr("Acoustic model returned invalid f0 data");
            qCritical() << "inferAcoustic:" << error;
            return false;
        }
    }
    // Run vocoder
    {
        const auto stage = InferStageModel::acquire(m_activeInference, lease, identifier,
                                                    InferStage::Vocoder, "inferAcoustic", error);
        if (!stage)
            return false;
        auto *inferenceVocoder = stage->executive<Vo::VocoderExecutive>();
        // The audio is written at the output sample rate declared in the vocoder configuration.
        const auto *vocoderConfig = static_cast<const Vo::VocoderConfiguration *>(
            inferenceVocoder->spec().configuration());
        if (!vocoderConfig || vocoderConfig->sampleRate <= 0) {
            error = tr("Vocoder sample rate configuration is invalid");
            qCritical() << "inferAcoustic:" << error;
            return false;
        }
        const int vocoderSampleRate = vocoderConfig->sampleRate;

        const auto originalF0Values = PitchRouting::midiPitchToF0(
            vocoderPitch->values, vocoderPitch->interval,
            static_cast<qsizetype>(acousticF0->elementCount()), acousticFrameWidth);
        if (originalF0Values.size() != static_cast<qsizetype>(acousticF0->elementCount())) {
            error = tr("Failed to align the original pitch for the vocoder");
            qCritical() << "inferAcoustic:" << error;
            return false;
        }
        auto originalF0Exp =
            ds::Tensor::create(ds::ITensor::Float, acousticF0->shape());
        if (!originalF0Exp) {
            // The label and the cause are concatenated instead of being combined into one
            // translated string. The label is translated, and the error chain is a runtime
            // message that remains in English in either case.
            error = tr("Failed to create the vocoder f0 tensor") + ": " +
                    QString::fromUtf8(originalF0Exp.error().toString());
            qCritical().noquote().nospace()
                << "inferAcoustic: " << error << ": " << originalF0Exp.error().message();
            return false;
        }
        auto originalF0 = originalF0Exp.take();
        std::copy(originalF0Values.cbegin(), originalF0Values.cend(),
                  originalF0->data<float>());

        Vo::VocoderStartInput vocoderInput;
        vocoderInput.mel = mel;
        vocoderInput.f0 = originalF0;

        std::unique_ptr<Vo::VocoderResult> result;
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
            error = QString::fromUtf8(exp.error().toString());
            return false;
        } else {
            result = exp.take();
            if (!result) {
                qCritical() << "inferAcoustic: vocoder result type mismatch or null result for"
                            << identifier;
                return false;
            }
        }

        // A failure is already reported as an error by start(), so the result needs no further
        // error check. The state is checked instead: a stopped run returns a result like a
        // completed run, and using that result would treat a partial phrase as a complete phrase.
        if (inferenceVocoder->state() != srt::ITask::Succeeded) {
            qCritical().noquote().nospace() << "inferAcoustic: the vocoder inference for "
                                            << identifier << " did not finish";
            return false;
        }
        const auto &audioRawData = result->audioData;

        const auto outputPathStr = StringUtils::qstr_to_native(outputPath);

        if (isTerminateRequested()) {
            abort();
            return false;
        }

        SndfileHandle audioFile(outputPathStr.c_str(), SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_FLOAT,
                                1, vocoderSampleRate);
        if (audioFile.error() != SF_ERR_NO_ERROR) {
            error = tr("Failed to write the rendered audio: %1")
                        .arg(QString::fromUtf8(audioFile.strError()));
            qCritical() << "Failed to write the rendered audio:" << audioFile.strError();
            return false;
        }
        const auto audioData = reinterpret_cast<const float *>(audioRawData.data());
        const auto audioSize = static_cast<sf_count_t>(audioRawData.size() / sizeof(float));
        if (audioFile.write(audioData, audioSize) != audioSize) {
            error = tr("Failed to write the rendered audio: %1")
                        .arg(QString::fromUtf8(audioFile.strError()));
            qCritical() << "Failed to write the rendered audio:" << audioFile.strError();
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
                        {"pitch",        InferInputBase::paramCurveObject(pitch)       },
                        {"breathiness",  InferInputBase::paramCurveObject(breathiness) },
                        {"tension",      InferInputBase::paramCurveObject(tension)     },
                        {"voicing",      InferInputBase::paramCurveObject(voicing)     },
                        {"energy",       InferInputBase::paramCurveObject(energy)      },
                        {"mouthOpening", InferInputBase::paramCurveObject(mouthOpening)},
                        {"gender",       InferInputBase::paramCurveObject(gender)      },
                        {"velocity",     InferInputBase::paramCurveObject(velocity)    },
                        {"toneShift",    InferInputBase::paramCurveObject(toneShift)   },
    });
}

GenericInferModel InferAcousticTask::InferAcousticInput::toEngineModel() const {
    auto words = InferTaskHelper::buildWords(*this, true);
    double totalLength = 0;
    // The curves are sampled on the frames of the model of this stage.
    const auto interval = InferFrameLayout::inputInterval(identifier, InferStage::Acoustic);
    for (const auto &word : words)
        totalLength += word.length();

    int frames = qRound(totalLength / interval);
    InferRetake retake;
    retake.end = frames;

    InferParam param;
    param.dynamic = true;
    param.interval = interval;
    param.retake = retake;

    InferParam pitch = param;
    pitch.tag = "pitch";
    const auto originalPitch = resamplePitchToFrames(this->pitch, frames, interval);
    pitch.values = PitchRouting::applyToneShift(
        originalPitch, resampleCurveToFrames(this->toneShift, frames, interval));

    InferParam vocoderPitch = param;
    vocoderPitch.tag = QString::fromLatin1(PitchRouting::VocoderPitchTag);
    vocoderPitch.values = originalPitch;

    InferParam breathiness = param;
    breathiness.tag = "breathiness";
    breathiness.values = resampleCurveToFrames(this->breathiness, frames, interval);

    InferParam tension = param;
    tension.tag = "tension";
    tension.values = resampleCurveToFrames(this->tension, frames, interval);

    InferParam voicing = param;
    voicing.tag = "voicing";
    voicing.values = resampleCurveToFrames(this->voicing, frames, interval);

    InferParam energy = param;
    energy.tag = "energy";
    energy.values = resampleCurveToFrames(this->energy, frames, interval);

    InferParam mouthOpening = param;
    mouthOpening.tag = "mouth_opening";
    mouthOpening.values = resampleCurveToFrames(this->mouthOpening, frames, interval);

    InferParam gender = param;
    gender.tag = "gender";
    gender.values = resampleCurveToFrames(this->gender, frames, interval);

    InferParam velocity = param;
    velocity.tag = "velocity";
    velocity.values = resampleCurveToFrames(this->velocity, frames, interval);

    GenericInferModel model;
    model.speaker = speaker;
    const auto effectiveMix =
        speakerMix.isEmpty() ? InferSpeakerMixModel::staticSpeakerMix(speaker) : speakerMix;
    model.speakerMix = InferSpeakerMixModel::fitToFrames(effectiveMix, frames, interval);
    model.words = words;
    model.params = {pitch,  vocoderPitch, breathiness, tension, voicing,
                    energy, mouthOpening, gender,      velocity};
    model.steps = steps;
    model.depth = static_cast<float>(depth);
    model.identifier = identifier;
    return model;
}
