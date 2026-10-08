#include "LanguageBridge.h"

#include <mutex>
#include <utility>

#include <synthrt/SVS/SingerContrib.h>

#include <wolf/Session/LinguistSession.h>

namespace lite::synthrt {

    namespace LinguistApi = wolf::Api::Linguist::L1;

    namespace {

        wolf::SingerRef refOf(const LanguageBridge::Singer &singer) {
            wolf::SingerRef reference;
            // The category is a field of the reference grammar, so it is specified explicitly and
            // not derived from the type name.
            reference.locator = srt::ContribLocator(singer.packageId, srt::SingerCategory::NAME,
                                                    singer.contributionId);
            // The version is set here because a module reference cannot carry a version. The
            // specification binds a module reference to the version that the dependency graph
            // resolved and allows two versions of a package to be loaded at the same time. An
            // empty version selects the only loaded version, which fails if two versions are
            // installed.
            reference.version = singer.version;
            return reference;
        }

        LinguistApi::Depth depthOf(LanguageBridge::Depth depth) {
            switch (depth) {
                case LanguageBridge::Depth::Pronunciation:
                    return LinguistApi::Depth::Pronunciation;
                case LanguageBridge::Depth::Phonemes:
                    return LinguistApi::Depth::Phonemes;
                case LanguageBridge::Depth::Onsets:
                    break;
            }
            return LinguistApi::Depth::Onsets;
        }

        /// Mirrors the layer the language module reports for a language, so that the caller does not
        /// read the language module's API. An unknown value maps to the deepest layer this editor
        /// asks for, the opposite default of depthOf() above, because the result decides whether a
        /// phoneme conversion is requested at all: folding an unknown value into Pronunciation would
        /// skip that conversion and report success, while requesting Onsets leaves the outcome to the
        /// conversion itself.
        LanguageBridge::Depth depthFrom(LinguistApi::Depth depth) {
            switch (depth) {
                case LinguistApi::Depth::Pronunciation:
                    return LanguageBridge::Depth::Pronunciation;
                case LinguistApi::Depth::Phonemes:
                    return LanguageBridge::Depth::Phonemes;
                case LinguistApi::Depth::Onsets:
                    break;
            }
            return LanguageBridge::Depth::Onsets;
        }

        /// Mirrors the stage the language module reported for a word, so that the result type does
        /// not expose the language module's API.
        PronunciationStage stageOf(const LinguistApi::HitStage stage) {
            switch (stage) {
                case LinguistApi::HitStage::Dict:
                    return PronunciationStage::Dict;
                case LinguistApi::HitStage::Model:
                    return PronunciationStage::Model;
                case LinguistApi::HitStage::Rule:
                    return PronunciationStage::Rule;
                case LinguistApi::HitStage::Fallback:
                    return PronunciationStage::Fallback;
                case LinguistApi::HitStage::Locked:
                    return PronunciationStage::Locked;
                case LinguistApi::HitStage::Unspecified:
                    break;
            }
            return PronunciationStage::Unspecified;
        }

        /// Returns the user-facing message for a per-word error, or an empty string for none.
        std::string describe(wolf::Api::G2P::L1::Error error) {
            using E = wolf::Api::G2P::L1::Error;
            switch (error) {
                case E::None:
                    return {};
                case E::InvalidInput:
                    return "the word is not valid input for the language";
                case E::ModelInferenceFailed:
                    return "the language model failed";
                case E::PhonemeGenerationFailed:
                    return "no phonemes could be produced";
                case E::DriverUnavailable:
                    return "no inference backend is available";
                case E::NotInitialized:
                    return "the language is not ready";
                case E::UnknownError:
                    break;
            }
            return "conversion failed";
        }

    }

    class LanguageBridge::Impl {
    public:
        explicit Impl(srt::SynthUnit &unit) : session(unit) {
        }

        wolf::LinguistSession session;

        // Handed to every conversion, with copies sharing one state, so that cancelling the token
        // would reach every conversion already running. Nothing cancels it today.
        std::mutex mutex;
        wolf::CancelToken token;
    };

    LanguageBridge::LanguageBridge(srt::SynthUnit &unit) : _impl(std::make_unique<Impl>(unit)) {
    }

    LanguageBridge::~LanguageBridge() = default;

    void LanguageBridge::refresh() {
        _impl->session.refresh();
    }

    void LanguageBridge::release(const Singer &singer) {
        _impl->session.release(refOf(singer));
    }

    std::vector<std::string> LanguageBridge::languagesOf(const Singer &singer) const {
        const auto catalog = _impl->session.catalog();
        if (!catalog) {
            return {};
        }
        const auto *entry = catalog->find(refOf(singer));
        if (entry == nullptr) {
            return {};
        }
        std::vector<std::string> result;
        result.reserve(entry->languages.size());
        for (const auto &language : entry->languages) {
            result.push_back(language.handle);
        }
        return result;
    }

    bool LanguageBridge::canConvert(const Singer &singer, const std::string &language) const {
        // probe() loads nothing and creates no executive, so this call is safe during list
        // rendering. Cold readiness indicates that the route is resolved and only the resources
        // remain to be loaded, so a cold route counts as existing.
        const auto status = _impl->session.probe(refOf(singer), language);
        return status.readiness != wolf::Readiness::Unavailable;
    }

    std::string LanguageBridge::unavailableReason(const Singer &singer,
                                                  const std::string &language) const {
        // The same call as canConvert(): probe() loads no resource and creates no executive, so it
        // is safe to call during list rendering. Only a definite Unavailable carries a reason; an
        // empty reason yields an empty string, and the fallback text is provided by the UI, because
        // this layer invents no words.
        const auto status = _impl->session.probe(refOf(singer), language);
        if (status.readiness != wolf::Readiness::Unavailable) {
            return {};
        }
        return status.reason;
    }

    std::optional<LanguageBridge::Depth> LanguageBridge::maxDepth(const Singer &singer,
                                                                 const std::string &language) const {
        // The same call as canConvert() / unavailableReason(): probe() loads no resource and
        // creates no executive, so the depth limit of the combination can be read even for a
        // language that was never converted.
        const auto status = _impl->session.probe(refOf(singer), language);
        if (status.readiness == wolf::Readiness::Unavailable) {
            // An unavailable route reaches no layer, and no Depth value expresses that, so an empty
            // value is returned and the caller applies the semantics of canConvert()
            // (unavailableReason() gives the reason). It is not folded into Pronunciation, which
            // would make the caller read "the route is unavailable" as "this combination has no
            // onset layer".
            return std::nullopt;
        }
        return depthFrom(status.maxDepth);
    }

    void LanguageBridge::setSingerPhonemes(const Singer &singer,
                                           std::vector<std::string> phonemes) {
        _impl->session.setSingerPhonemes(refOf(singer), std::move(phonemes));
    }

    void LanguageBridge::setReservedMarkers(std::vector<std::string> markers) {
        _impl->session.setReservedMarkers(std::move(markers));
    }

    std::vector<std::string> LanguageBridge::reservedMarkers() const {
        return _impl->session.reservedMarkers();
    }

    srt::Expected<std::vector<LanguageBridge::Result>>
        LanguageBridge::convert(const Singer &singer, const std::string &language,
                            const std::vector<Word> &words, Depth depth) const {
        LinguistApi::LinguistConvertInput input;
        input.depth = depthOf(depth);
        input.words.reserve(words.size());
        for (const auto &word : words) {
            LinguistApi::LinguistWordInput one;
            one.lyric = word.lyric;
            one.pronunciation = word.pronunciation;
            // Phonemes and onsets must have the same length. The contract therefore holds both in
            // one optional, and an editor state with only one of them is a bug, not a valid state.
            if (word.phonemes && word.onsets) {
                LinguistApi::LockedPhonemes locked;
                locked.phonemes = *word.phonemes;
                locked.onsets = *word.onsets;
                one.locked = std::move(locked);
            }
            input.words.push_back(std::move(one));
        }

        wolf::CancelToken token;
        {
            std::lock_guard guard(_impl->mutex);
            token = _impl->token;
        }

        auto produced =
            _impl->session.convert(refOf(singer), language, input, token);
        if (!produced) {
            return produced.takeError();
        }
        const auto converted = produced.take();

        std::vector<Result> result;
        result.reserve(converted->words.size());
        for (const auto &word : converted->words) {
            Result one;
            one.pronunciation = word.pronunciation;
            one.candidates = word.candidates;
            one.phonemes = word.phonemes;
            one.onsets = word.onsets;
            one.error = describe(word.error);
            one.stage = stageOf(word.hitStage);
            result.push_back(std::move(one));
        }
        return result;
    }

}
