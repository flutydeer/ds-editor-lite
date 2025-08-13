#include "InferenceLoader.h"

#include <QDebug>
#include <QLocale>
#include <QString>

#include <synthrt/SVS/SingerContrib.h>
#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>
#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>
#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>
#include <dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>

namespace Co = ds::Api::Common::L1;
namespace Ac = ds::Api::Acoustic::L1;
namespace Dur = ds::Api::Duration::L1;
namespace Pit = ds::Api::Pitch::L1;
namespace Var = ds::Api::Variance::L1;
namespace Vo = ds::Api::Vocoder::L1;

static std::string s_locale = QLocale::system().name().toStdString();

static inline QString getSingerDisplayString(const QString &singerName, const QString &singerId) {
    return singerName + " (" + singerId + ")";
}

InferenceLoader::InferenceLoader() : m_singerSpec(nullptr) {
}

InferenceLoader::InferenceLoader(const srt::SingerSpec *spec) : m_singerSpec(spec) {
}

auto InferenceLoader::loadInferenceSpecs() -> Result<InferenceFlag::Type> {
    if (!m_singerSpec) {
        QString errMsg = "SingerSpec is nullptr";
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    }
    if (const auto &category = m_singerSpec->category(); category != "singer") {
        QString errMsg = "SingerSpec invalid category: " + QString::fromUtf8(category);
        return errMsg;
    }

    m_singerId = QString::fromUtf8(m_singerSpec->id());
    m_singerName = QString::fromUtf8(m_singerSpec->name().text(s_locale));

    const auto singerDisplay = getSingerDisplayString(m_singerName, m_singerId);

    qDebug().noquote().nospace() << "Loading inference specs for singer " << singerDisplay;

    for (const auto &imp : m_singerSpec->imports()) {
        const auto &cls = imp.inference()->className();
        bool found = false;
        if (cls == Dur::API_CLASS) {
            m_importOptions.duration = imp.options();
            m_specs.duration = imp.inference();
            found = true;
        } else if (cls == Pit::API_CLASS) {
            m_importOptions.pitch = imp.options();
            m_specs.pitch = imp.inference();
            found = true;
        } else if (cls == Var::API_CLASS) {
            m_importOptions.variance = imp.options();
            m_specs.variance = imp.inference();
            found = true;
        } else if (cls == Ac::API_CLASS) {
            m_importOptions.acoustic = imp.options();
            m_specs.acoustic = imp.inference();
            found = true;
        } else if (cls == Vo::API_CLASS) {
            m_importOptions.vocoder = imp.options();
            m_specs.vocoder = imp.inference();
            found = true;
        }
        if (found) {
            qDebug().noquote().nospace()
                << "Found inference for singer " << singerDisplay << ": " << cls;
        } else {
            qWarning().noquote().nospace()
                << "Invalid inference for singer " << singerDisplay << ": " << cls;
        }
    }
    return checkInferenceSpecs();
}

auto InferenceLoader::checkInferenceSpecs() const -> InferenceFlag::Type {
    InferenceFlag::Type flags = InferenceFlag::None;
    if (hasDuration()) {
        flags.set(InferenceFlag::Duration);
    }
    if (hasPitch()) {
        flags.set(InferenceFlag::Pitch);
    }
    if (hasVariance()) {
        flags.set(InferenceFlag::Variance);
    }
    if (hasAcoustic()) {
        flags.set(InferenceFlag::Acoustic);
    }
    if (hasVocoder()) {
        flags.set(InferenceFlag::Vocoder);
    }
    return flags;
}

QString InferenceLoader::singerId() const {
    return m_singerId;
}

QString InferenceLoader::singerName() const {
    return m_singerName;
}

bool InferenceLoader::hasDuration() const noexcept {
    return m_specs.duration && m_importOptions.duration;
}

bool InferenceLoader::hasPitch() const noexcept {
    return m_specs.pitch && m_importOptions.pitch;
}

bool InferenceLoader::hasVariance() const noexcept {
    return m_specs.variance && m_importOptions.variance;
}

bool InferenceLoader::hasAcoustic() const noexcept {
    return m_specs.acoustic && m_importOptions.acoustic;
}

bool InferenceLoader::hasVocoder() const noexcept {
    return m_specs.vocoder && m_importOptions.vocoder;
}

auto InferenceLoader::createDuration() const -> Result<srt::NO<srt::Inference>> {
    const auto singerDisplay = getSingerDisplayString(m_singerName, m_singerId);

    srt::NO<srt::Inference> inferenceDuration;
    const auto runtimeOptions = srt::NO<Dur::DurationRuntimeOptions>::create();
    if (auto exp = m_specs.duration->createInference(m_importOptions.duration, runtimeOptions);
        !exp) {
        QString errMsg = "Failed to create duration inference for singer " + singerDisplay + ": " +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    } else {
        inferenceDuration = exp.take();
    }
    const auto initArgs = srt::NO<Dur::DurationInitArgs>::create();
    if (auto exp = inferenceDuration->initialize(initArgs); !exp) {
        QString errMsg = "Failed to initialize duration inference for singer " + singerDisplay +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    }
    return inferenceDuration;
}

auto InferenceLoader::createPitch() const -> Result<srt::NO<srt::Inference>> {
    const auto singerDisplay = getSingerDisplayString(m_singerName, m_singerId);

    srt::NO<srt::Inference> inferencePitch;
    const auto runtimeOptions = srt::NO<Pit::PitchRuntimeOptions>::create();
    if (auto exp = m_specs.pitch->createInference(m_importOptions.pitch, runtimeOptions); !exp) {
        QString errMsg = "Failed to create pitch inference for singer " + singerDisplay + ": " +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    } else {
        inferencePitch = exp.take();
    }
    const auto initArgs = srt::NO<Pit::PitchInitArgs>::create();
    if (auto exp = inferencePitch->initialize(initArgs); !exp) {
        QString errMsg = "Failed to initialize pitch inference for singer " + singerDisplay +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    }
    return inferencePitch;
}

auto InferenceLoader::createVariance() const -> Result<srt::NO<srt::Inference>> {
    const auto singerDisplay = getSingerDisplayString(m_singerName, m_singerId);

    srt::NO<srt::Inference> inferenceVariance;
    const auto runtimeOptions = srt::NO<Var::VarianceRuntimeOptions>::create();
    if (auto exp = m_specs.variance->createInference(m_importOptions.variance, runtimeOptions); !exp) {
        QString errMsg = "Failed to create variance inference for singer " + singerDisplay + ": " +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    } else {
        inferenceVariance = exp.take();
    }
    const auto initArgs = srt::NO<Var::VarianceInitArgs>::create();
    if (auto exp = inferenceVariance->initialize(initArgs); !exp) {
        QString errMsg = "Failed to initialize variance inference for singer " + singerDisplay +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    }
    return inferenceVariance;
}

auto InferenceLoader::createAcoustic() const -> Result<srt::NO<srt::Inference>> {
    const auto singerDisplay = getSingerDisplayString(m_singerName, m_singerId);

    srt::NO<srt::Inference> inferenceAcoustic;
    const auto runtimeOptions = srt::NO<Ac::AcousticRuntimeOptions>::create();
    if (auto exp = m_specs.acoustic->createInference(m_importOptions.acoustic, runtimeOptions); !exp) {
        QString errMsg = "Failed to create acoustic inference for singer " + singerDisplay + ": " +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    } else {
        inferenceAcoustic = exp.take();
    }
    const auto initArgs = srt::NO<Ac::AcousticInitArgs>::create();
    if (auto exp = inferenceAcoustic->initialize(initArgs); !exp) {
        QString errMsg = "Failed to initialize acoustic inference for singer " + singerDisplay +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    }
    return inferenceAcoustic;
}

auto InferenceLoader::createVocoder() const -> Result<srt::NO<srt::Inference>> {
    const auto singerDisplay = getSingerDisplayString(m_singerName, m_singerId);

    srt::NO<srt::Inference> inferenceVocoder;
    const auto runtimeOptions = srt::NO<Vo::VocoderRuntimeOptions>::create();
    if (auto exp = m_specs.vocoder->createInference(m_importOptions.vocoder, runtimeOptions); !exp) {
        QString errMsg = "Failed to create vocoder inference for singer " + singerDisplay + ": " +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    } else {
        inferenceVocoder = exp.take();
    }
    const auto initArgs = srt::NO<Vo::VocoderInitArgs>::create();
    if (auto exp = inferenceVocoder->initialize(initArgs); !exp) {
        QString errMsg = "Failed to initialize vocoder inference for singer " + singerDisplay +
                         QString::fromUtf8(exp.error().message());
        qCritical().noquote().nospace() << errMsg;
        return errMsg;
    }
    return inferenceVocoder;
}