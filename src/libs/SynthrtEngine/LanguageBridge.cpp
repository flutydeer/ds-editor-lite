#include "LanguageBridge.h"

#include <mutex>
#include <utility>

namespace lite::synthrt {

    namespace LinguistApi = wolf::Api::Linguist::L1;

    namespace {

        wolf::SingerRef refOf(const std::string &packageId,
                                               const std::string &contributionId) {
            wolf::SingerRef singer;
            singer.locator = srt::ContribLocator(packageId, "singer", contributionId);
            // The version is left empty on purpose: the editor names a singer by the package it
            // came from, and two versions of one voicebank are not both loaded. Should that change,
            // this is the one place that has to learn about it.
            return singer;
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

        /// Turns a per word error into something an editor can put in front of a person.
        std::string describe(wolf::Api::G2P::L1::Error error) {
            using E = wolf::Api::G2P::L1::Error;
            switch (error) {
                case E::None:
                    return {};
                case E::InvalidInput:
                    return "this word is not something the language accepts";
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

        // Handed to every conversion so that one cancel reaches whatever is running. Copies share
        // one state, which is what makes that work across threads.
        std::mutex mutex;
        wolf::CancelToken token;
    };

    LanguageBridge::LanguageBridge(srt::SynthUnit &unit) : _impl(std::make_unique<Impl>(unit)) {
    }

    LanguageBridge::~LanguageBridge() = default;

    void LanguageBridge::refresh() {
        _impl->session.refresh();
    }

    std::vector<std::string> LanguageBridge::languagesOf(const std::string &packageId,
                                                         const std::string &contributionId) const {
        const auto catalog = _impl->session.catalog();
        if (!catalog) {
            return {};
        }
        const auto *entry = catalog->find(refOf(packageId, contributionId));
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

    bool LanguageBridge::canConvert(const std::string &packageId,
                                    const std::string &contributionId,
                                    const std::string &language) const {
        // probe() loads nothing and creates no executive, so this is safe to ask while drawing a
        // list. Cold means everything is decided and only the resources are still to come, which
        // for a question about whether a route exists is a yes.
        const auto status = _impl->session.probe(refOf(packageId, contributionId), language);
        return status.readiness != wolf::Readiness::Unavailable;
    }

    void LanguageBridge::setSingerPhonemes(const std::string &packageId,
                                           const std::string &contributionId,
                                           std::vector<std::string> phonemes) {
        _impl->session.setSingerPhonemes(refOf(packageId, contributionId), std::move(phonemes));
    }

    void LanguageBridge::setReservedMarkers(std::vector<std::string> markers) {
        _impl->session.setReservedMarkers(std::move(markers));
    }

    std::vector<std::string> LanguageBridge::reservedMarkers() const {
        return _impl->session.reservedMarkers();
    }

    srt::Expected<std::vector<LanguageBridge::Result>>
        LanguageBridge::convert(const std::string &packageId, const std::string &contributionId,
                                const std::string &language, const std::vector<Word> &words,
                                Depth depth) const {
        LinguistApi::LinguistConvertInput input;
        input.depth = depthOf(depth);
        input.words.reserve(words.size());
        for (const auto &word : words) {
            LinguistApi::LinguistWordInput one;
            one.lyric = word.lyric;
            one.pronunciation = word.pronunciation;
            // Phonemes and onsets travel together because they have to be the same length; the
            // contract says so with one optional holding both, and an editor that had only one of
            // them has a bug rather than a state to represent.
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
            _impl->session.convert(refOf(packageId, contributionId), language, input, token);
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
            result.push_back(std::move(one));
        }
        return result;
    }

    void LanguageBridge::cancel() {
        // Replacing the token afterwards is what keeps a cancellation from outliving the batch it
        // was aimed at: the next conversion is handed a fresh one.
        std::lock_guard guard(_impl->mutex);
        _impl->token.cancel();
        _impl->token = wolf::CancelToken();
    }

}
