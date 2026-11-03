#ifndef SINGERPIPELINE_H
#define SINGERPIPELINE_H

#include <memory>
#include <string_view>

#include <synthrt/Core/PackageHandle.h>
#include <synthrt/SVS/SingerContrib.h>
#include <synthrt/SVS/SingerPipelineExecutive.h>
#include <synthrt/Support/Expected.h>

#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>
#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>
#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>
#include <dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>

#include "SingerStages.h"

namespace lite::synthrt {

    /// Lazy stage executives for one singer. The enclosing SynthUnit must outlive the pipeline.
    class SingerPipeline {
    public:
        /// Retain the singer's package until all stage executives have been destroyed.
        static srt::Expected<std::unique_ptr<SingerPipeline>> create(srt::PackageHandle package,
                                                                     srt::SingerSpec &singer);

        ~SingerPipeline();

        /// Stage accessors reuse their executive and report missing stages or model-load failures.
        /// Duration, pitch, and variance are optional; acoustic and vocoder are required.
        srt::Expected<ds::Api::Duration::L1::DurationExecutive *> duration();
        srt::Expected<ds::Api::Pitch::L1::PitchExecutive *> pitch();
        srt::Expected<ds::Api::Variance::L1::VarianceExecutive *> variance();
        srt::Expected<ds::Api::Acoustic::L1::AcousticExecutive *> acoustic();
        srt::Expected<ds::Api::Vocoder::L1::VocoderExecutive *> vocoder();

        /// Options attached to the stage import, or null when absent, including singer-to-model
        /// mappings.
        const srt::ContribImportOptions *options(SingerStage stage) const;

    private:
        SingerPipeline(srt::PackageHandle package, srt::SingerSpec &singer,
                       std::unique_ptr<srt::SingerPipelineExecutive> pipeline);

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // SINGERPIPELINE_H
