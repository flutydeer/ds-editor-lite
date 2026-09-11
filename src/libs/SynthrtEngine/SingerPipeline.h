#ifndef SINGERPIPELINE_H
#define SINGERPIPELINE_H

#include <memory>
#include <string_view>

#include <synthrt/SVS/SingerContrib.h>
#include <synthrt/SVS/SingerPipelineExecutive.h>
#include <synthrt/Support/Expected.h>

#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>
#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>
#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>
#include <dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>

namespace lite::synthrt {

    /// One singer's synthesis pipeline, and the stage executives it hands out.
    ///
    /// This replaces the refactor line's ModelSetHandle, and is a layer thinner than it was. There,
    /// a stage was an {inference, importOptions} pair the caller then had to assemble; here the
    /// pipeline resolves the import and returns a typed executive, so the pair has nowhere left to
    /// go wrong.
    ///
    /// Stages are created on first use rather than up front: a project that never runs variance
    /// should not pay to open a variance model, and a singer may legitimately not have one.
    /// Everything a pipeline hands out is owned by the pipeline and dies with it, so it must not
    /// outlive the package its singer came from.
    class SingerPipeline {
    public:
        /// Builds the pipeline a singer declares.
        ///
        /// Fails when the singer's provider is absent, which is what happens to a voicebank whose
        /// contract no installed interpreter serves.
        static srt::Expected<std::unique_ptr<SingerPipeline>> create(srt::SingerSpec &singer);

        ~SingerPipeline();

        /// \name The five stages
        ///
        /// Each returns the same executive every time, and an error when the singer does not
        /// import that stage or its model will not open. Duration, pitch and variance are
        /// optional; acoustic and vocoder are not, and a singer without them would not have
        /// loaded.
        /// \{
        srt::Expected<ds::Api::Duration::L1::DurationExecutive *> duration();
        srt::Expected<ds::Api::Pitch::L1::PitchExecutive *> pitch();
        srt::Expected<ds::Api::Variance::L1::VarianceExecutive *> variance();
        srt::Expected<ds::Api::Acoustic::L1::AcousticExecutive *> acoustic();
        srt::Expected<ds::Api::Vocoder::L1::VocoderExecutive *> vocoder();
        /// \}

        /// What the singer attached to one of its imports, or null when it attached nothing.
        ///
        /// A stage's executive answers what the model can do; this answers what this singer asked
        /// of it -- which of its own speaker names map to which of the model's, which variance
        /// parameters it wants predicted. Both are needed to run a stage and they come from
        /// different places, so they are asked for separately.
        ///
        /// \a role is a singer import role, such as "singer/acoustic".
        const srt::ContribImportOptions *options(std::string_view role) const;

    private:
        SingerPipeline(srt::SingerSpec &singer,
                       std::unique_ptr<srt::SingerPipelineExecutive> pipeline);

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // SINGERPIPELINE_H
