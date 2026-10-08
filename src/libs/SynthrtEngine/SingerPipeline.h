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

    /// Synthesis pipeline of one singer, with the executives of its stages.
    ///
    /// This class replaces the ModelSetHandle of the refactor branch, in which a stage was an
    /// {inference, importOptions} pair that the caller assembled. The pipeline resolves the import
    /// and returns a typed executive, which removes that assembly step and its failure modes.
    ///
    /// Stages are created on first use rather than in advance, because a project that never runs
    /// variance must not incur the cost of opening a variance model, and a singer may lack that
    /// model. Every executive returned by a pipeline is owned by the pipeline and destroyed with
    /// it, so the pipeline must not outlive the package that contains its singer.
    class SingerPipeline {
    public:
        /// Builds the pipeline that \a singer declares.
        ///
        /// \a package is the handle of the package that contains \a singer. The pipeline retains
        /// the handle, so the declaration and the executives that borrow from it remain valid for
        /// the lifetime of the pipeline, independently of any rescan. The executives are destroyed
        /// before the handle is released, which is the order that synthrt requires.
        ///
        /// Fails if the provider of the singer is absent, which is the case for a voicebank whose
        /// contract no installed interpreter implements.
        static srt::Expected<std::unique_ptr<SingerPipeline>> create(srt::PackageHandle package,
                                                                     srt::SingerSpec &singer);

        ~SingerPipeline();

        /// \name Stage executives
        ///
        /// Each function returns the same executive on every call, or an error if the singer does
        /// not import the stage or its model cannot be opened. Duration, pitch and variance are
        /// optional. Acoustic and vocoder are required, and a singer without them fails to load.
        /// \{
        srt::Expected<ds::Api::Duration::L1::DurationExecutive *> duration();
        srt::Expected<ds::Api::Pitch::L1::PitchExecutive *> pitch();
        srt::Expected<ds::Api::Variance::L1::VarianceExecutive *> variance();
        srt::Expected<ds::Api::Acoustic::L1::AcousticExecutive *> acoustic();
        srt::Expected<ds::Api::Vocoder::L1::VocoderExecutive *> vocoder();
        /// \}

        /// Returns the options that the singer attaches to one of its imports, or null if the
        /// import has no options.
        ///
        /// The executive of a stage reports the capabilities of the model. The import options
        /// record the singer's configuration of the model, such as the mapping from the singer's
        /// speaker names to the model's speaker names and the variance parameters to predict.
        /// Running a stage requires both, and they have different sources, so they are queried
        /// separately.
        ///
        /// \a stage selects the import by its role; see roleOf().
        const srt::ContribImportOptions *options(SingerStage stage) const;

    private:
        SingerPipeline(srt::PackageHandle package, srt::SingerSpec &singer,
                       std::unique_ptr<srt::SingerPipelineExecutive> pipeline);

        class Impl;
        std::unique_ptr<Impl> _impl;
    };

}

#endif // SINGERPIPELINE_H
