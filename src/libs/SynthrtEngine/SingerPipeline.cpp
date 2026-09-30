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

        /// Declared first, so that it is released last: the declaration and every executive
        /// below borrow from this package.
        srt::PackageHandle package;

        /// Borrowed from the package, which the handle above keeps alive.
        srt::SingerSpec *singer;

        /// Creates a stage once and remembers it, or remembers why it could not be created.
        ///
        /// The failure is remembered as well as the success: a singer with no variance model
        /// fails every time, and a synthesis that asks per phrase would otherwise re-ask the
        /// provider for something it has already said it does not have.
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
            // Creating an executive selects the model; initializing it opens the model. Both
            // happen here so that what a caller is handed is ready to run -- a stage that has to
            // be initialized separately is a stage someone will forget to initialize.
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
            // No interpreter served this singer's contract, so nothing attached a pipeline to it.
            // Every voicebank for another engine reaches here, which is why it reads as an
            // unsupported feature rather than as a broken package.
            return srt::Error(srt::Error::FeatureNotSupported,
                              "this singer declares a contract no installed provider serves");
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

    const srt::ContribImportOptions *SingerPipeline::options(std::string_view role) const {
        const auto import = _impl->singer->findImport(role);
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
