#ifndef INFERENCE_LOADER_H
#define INFERENCE_LOADER_H

#include <QString>
#include <QVersionNumber>

#include <synthrt/Core/PackageRef.h>
#include <synthrt/SVS/InferenceContrib.h>

#include "Utils/Expected.h"
#include "Models/SingerIdentifier.h"
#include "InferenceFlag.h"

namespace srt {
    class SingerSpec;
}

struct InferenceSpecSet {
    inline constexpr bool isComplete() const noexcept {
        return duration != nullptr && pitch != nullptr && variance != nullptr &&
               acoustic != nullptr && vocoder != nullptr;
    }

    const srt::InferenceSpec *duration = nullptr;
    const srt::InferenceSpec *pitch = nullptr;
    const srt::InferenceSpec *variance = nullptr;
    const srt::InferenceSpec *acoustic = nullptr;
    const srt::InferenceSpec *vocoder = nullptr;
};

struct InferenceImportOptionsSet {
    inline bool isComplete() const noexcept {
        return duration && pitch && variance &&
               acoustic && vocoder;
    }

    srt::NO<srt::InferenceImportOptions> duration;
    srt::NO<srt::InferenceImportOptions> pitch;
    srt::NO<srt::InferenceImportOptions> variance;
    srt::NO<srt::InferenceImportOptions> acoustic;
    srt::NO<srt::InferenceImportOptions> vocoder;
};

class InferenceLoader {
public:
    template <typename OkType>
    using Result = Expected<OkType, QString>;

    InferenceLoader();
    explicit InferenceLoader(const srt::SingerSpec *spec);
    Result<InferenceFlag::Type> loadInferenceSpecs();
    InferenceFlag::Type checkInferenceSpecs() const;
    QString singerName() const;
    QString singerId() const;
    srt::PackageRef package() const;
    QString packageId() const;
    QVersionNumber packageVersion() const;
    const srt::SingerSpec *singerSpec() const;

    bool hasDuration() const noexcept;
    bool hasPitch() const noexcept;
    bool hasVariance() const noexcept;
    bool hasAcoustic() const noexcept;
    bool hasVocoder() const noexcept;

    Result<srt::NO<srt::Inference>> createDuration() const;
    Result<srt::NO<srt::Inference>> createPitch() const;
    Result<srt::NO<srt::Inference>> createVariance() const;
    Result<srt::NO<srt::Inference>> createAcoustic() const;
    Result<srt::NO<srt::Inference>> createVocoder() const;

private:
    const srt::SingerSpec *m_singerSpec = nullptr;
    InferenceSpecSet m_specs;
    InferenceImportOptionsSet m_importOptions;
    SingerIdentifier m_identifier;
    QString m_singerName;
};



#endif // INFERENCE_LOADER_H