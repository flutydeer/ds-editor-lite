#include "SingerPipeline.h"

#include <mutex>
#include <utility>

#include <synthrt/Core/ContribSpecExtension.h>

#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>

namespace lite::synthrt {

    namespace Ds = ds::Api::DiffSinger::L1;

    class SingerPipeline::Impl {
    public:
        Impl(srt::PackageHandle package, srt::SingerSpec &singer,
             std::unique_ptr<srt::SingerPipelineExecutive> pipeline)
            : package(std::move(package)), singer(&singer), pipeline(std::move(pipeline)) {
        }

        /// Declared first so that it is released last, because the declaration and every
        /// executive below borrow from this package.
        srt::PackageHandle package;

        /// Borrowed from the package, which the handle above keeps alive.
        srt::SingerSpec *singer;

        /// Creates a stage once and caches the executive, or caches the reason for the failure.
        ///
        /// Failures are cached as well as successes. A singer without a variance model fails on
        /// every attempt, and a synthesis that requests the stage per phrase would otherwise query
        /// the provider repeatedly for a stage that it has already reported as absent.
        template <class Executive, class Options, class InitArgs, class Make>
        srt::Expected<Executive *> stage(Executive *&slot, bool &tried, std::string &why,
                                         Make make) {
            std::lock_guard guard(mutex);
            if (slot != nullptr) {
                return slot;
            }
            if (tried) {
                return srt::Error(srt::Error::FeatureNotSupported, why);
            }
            tried = true;
            Options options;
            auto created = make(options);
            if (!created) {
                why = created.error().toString();
                return created.takeError();
            }
            auto *executive = created.take();
            // Creating an executive selects the model, and initializing it opens the model. Both
            // steps happen here so that the returned executive is ready to run, because a separate
            // initialization step is easily omitted by a caller.
            if (auto started = executive->initialize(InitArgs{}); !started) {
                why = started.error().toString();
                return started.takeError();
            }
            slot = executive;
            return slot;
        }

        std::mutex mutex;
        std::unique_ptr<srt::SingerPipelineExecutive> pipeline;

        ds::Api::Duration::L1::DurationExecutive *durationStage = nullptr;
        ds::Api::Pitch::L1::PitchExecutive *pitchStage = nullptr;
        ds::Api::Variance::L1::VarianceExecutive *varianceStage = nullptr;
        ds::Api::Acoustic::L1::AcousticExecutive *acousticStage = nullptr;
        ds::Api::Vocoder::L1::VocoderExecutive *vocoderStage = nullptr;

        bool durationTried = false;
        bool pitchTried = false;
        bool varianceTried = false;
        bool acousticTried = false;
        bool vocoderTried = false;

        std::string durationWhy;
        std::string pitchWhy;
        std::string varianceWhy;
        std::string acousticWhy;
        std::string vocoderWhy;
    };

    srt::Expected<std::unique_ptr<SingerPipeline>>
        SingerPipeline::create(srt::PackageHandle package, srt::SingerSpec &singer) {
        auto *extension =
            srt::ContribSpecExtension::findFromSpec<Ds::DiffSingerPipelineExecutive>(singer);
        if (extension == nullptr) {
            // No interpreter implements the contract of this singer, so no pipeline extension is
            // attached. Every voicebank for another engine reaches this branch, so the error is
            // reported as an unsupported feature rather than as a broken package.
            return srt::Error(srt::Error::FeatureNotSupported,
                              "no installed provider implements the contract of this singer");
        }

        Ds::DiffSingerPipelineRuntimeOptions options;
        auto created = extension->as<srt::SingerPipelineExtension>()->createPipeline(options);
        if (!created) {
            return created.takeError().withContext("cannot build this singer's pipeline");
        }
        return std::unique_ptr<SingerPipeline>(
            new SingerPipeline(std::move(package), singer, created.take()));
    }

    SingerPipeline::SingerPipeline(srt::PackageHandle package, srt::SingerSpec &singer,
                                   std::unique_ptr<srt::SingerPipelineExecutive> pipeline)
        : _impl(std::make_unique<Impl>(std::move(package), singer, std::move(pipeline))) {
    }

    const srt::ContribImportOptions *SingerPipeline::options(SingerStage stage) const {
        const auto import = _impl->singer->findImport(roleOf(stage));
        return import ? import->options() : nullptr;
    }

    SingerPipeline::~SingerPipeline() = default;

    srt::Expected<ds::Api::Duration::L1::DurationExecutive *> SingerPipeline::duration() {
        auto *typed = _impl->pipeline->as<Ds::DiffSingerPipelineExecutive>();
        return _impl->stage<ds::Api::Duration::L1::DurationExecutive,
                            ds::Api::Duration::L1::DurationRuntimeOptions,
                            ds::Api::Duration::L1::DurationInitArgs>(
            _impl->durationStage, _impl->durationTried, _impl->durationWhy,
            [typed](const auto &options) { return typed->createDuration(options); });
    }

    srt::Expected<ds::Api::Pitch::L1::PitchExecutive *> SingerPipeline::pitch() {
        auto *typed = _impl->pipeline->as<Ds::DiffSingerPipelineExecutive>();
        return _impl->stage<ds::Api::Pitch::L1::PitchExecutive,
                            ds::Api::Pitch::L1::PitchRuntimeOptions,
                            ds::Api::Pitch::L1::PitchInitArgs>(
            _impl->pitchStage, _impl->pitchTried, _impl->pitchWhy,
            [typed](const auto &options) { return typed->createPitch(options); });
    }

    srt::Expected<ds::Api::Variance::L1::VarianceExecutive *> SingerPipeline::variance() {
        auto *typed = _impl->pipeline->as<Ds::DiffSingerPipelineExecutive>();
        return _impl->stage<ds::Api::Variance::L1::VarianceExecutive,
                            ds::Api::Variance::L1::VarianceRuntimeOptions,
                            ds::Api::Variance::L1::VarianceInitArgs>(
            _impl->varianceStage, _impl->varianceTried, _impl->varianceWhy,
            [typed](const auto &options) { return typed->createVariance(options); });
    }

    srt::Expected<ds::Api::Acoustic::L1::AcousticExecutive *> SingerPipeline::acoustic() {
        auto *typed = _impl->pipeline->as<Ds::DiffSingerPipelineExecutive>();
        return _impl->stage<ds::Api::Acoustic::L1::AcousticExecutive,
                            ds::Api::Acoustic::L1::AcousticRuntimeOptions,
                            ds::Api::Acoustic::L1::AcousticInitArgs>(
            _impl->acousticStage, _impl->acousticTried, _impl->acousticWhy,
            [typed](const auto &options) { return typed->createAcoustic(options); });
    }

    srt::Expected<ds::Api::Vocoder::L1::VocoderExecutive *> SingerPipeline::vocoder() {
        auto *typed = _impl->pipeline->as<Ds::DiffSingerPipelineExecutive>();
        return _impl->stage<ds::Api::Vocoder::L1::VocoderExecutive,
                            ds::Api::Vocoder::L1::VocoderRuntimeOptions,
                            ds::Api::Vocoder::L1::VocoderInitArgs>(
            _impl->vocoderStage, _impl->vocoderTried, _impl->vocoderWhy,
            [typed](const auto &options) { return typed->createVocoder(options); });
    }

}
