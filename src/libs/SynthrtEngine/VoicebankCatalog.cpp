#include "VoicebankCatalog.h"

#include "AnalysisContracts.h"
#include "SingerStages.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <synthrt/Core/ContribImportBinding.h>
#include <synthrt/Support/JSON.h>

#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <dsinfer/Api/Inferences/Duration/1/DurationApiL1.h>
#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>
#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>
#include <dsinfer/Api/Inferences/Vocoder/1/VocoderApiL1.h>
#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>

#include <otter/Api/F0/1/F0ApiL1.h>
#include <otter/Api/Note/1/NoteApiL1.h>

#include <wolf/Linguist/SingerLanguages.h>

namespace lite::synthrt {

    namespace {

        /// Returns the target declaration of the import \a role of \a spec, or null.
        ///
        /// Returns null for a package opened without loading and for an import whose target was
        /// not resolved. Both cases are expected: this function runs while voicebanks are listed,
        /// and a missing stage is a property of the singer rather than an error.
        const srt::ContribSpec *targetOf(const srt::ContribSpec &spec, std::string_view role) {
            const auto import = spec.findImport(role);
            if (!import || !import->binding()) {
                return nullptr;
            }
            return &import->binding()->target();
        }

        template <class Schema>
        const Schema *exportsOf(const srt::ContribSpec *target) {
            if (target == nullptr || target->exports() == nullptr) {
                return nullptr;
            }
            return target->exports()->as<Schema>();
        }

        /// Returns the interpreted configuration of \a target, or null if the target is absent,
        /// has no configuration or implements another contract. The contract is compared first,
        /// because the downcast of a configuration is not checked.
        template <class Configuration>
        const Configuration *configurationOf(const srt::ContribSpec *target,
                                             std::string_view interfaceName) {
            if (target == nullptr || target->interface() != interfaceName ||
                target->configuration() == nullptr) {
                return nullptr;
            }
            return target->configuration()->as<Configuration>();
        }

        void insertNames(std::set<std::string> &into, const std::set<ds::ParamTag> &tags) {
            for (const auto &tag : tags) {
                into.insert(tag.name());
            }
        }

        /// Builds a display text from its manifest representation, or from \a fallback if the
        /// value is neither a string nor an object.
        ///
        /// A plain string is the default text. An object maps locales to texts, with "_" as the
        /// default; this is the specification's format for a translated name.
        srt::DisplayText displayTextOf(const srt::JsonValue &value, const std::string &fallback) {
            if (value.isString()) {
                return srt::DisplayText(value.toString());
            }
            if (!value.isObject()) {
                return srt::DisplayText(fallback);
            }
            std::string standard = fallback;
            std::map<std::string, std::string> translations;
            for (const auto &[locale, text] : value.toObject()) {
                if (!text.isString()) {
                    continue;
                }
                if (locale == "_") {
                    standard = text.toString();
                } else {
                    translations.emplace(locale, text.toString());
                }
            }
            return srt::DisplayText(std::move(standard), translations);
        }

        /// Converts a note name such as C4, F#3 or Bb-1 to a MIDI number.
        ///
        /// Voicebanks write tone ranges as note names rather than as numbers. No interpreter reads
        /// them, because the models never receive a tone range; a tone range is guidance for the
        /// user who selects a speaker. The note name is therefore parsed here, together with the
        /// other host-side reading of the singer.
        ///
        /// \return the MIDI number, or \c std::nullopt if \a name is not a note name or lies
        ///         outside the MIDI range.
        std::optional<int> midiOf(const std::string &name) {
            static constexpr int semitones[] = {9, 11, 0, 2, 4, 5, 7}; // A B C D E F G
            if (name.empty()) {
                return std::nullopt;
            }
            const auto letter = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            if (letter < 'A' || letter > 'G') {
                return std::nullopt;
            }
            int value = semitones[letter - 'A'];
            std::size_t at = 1;
            for (; at < name.size() && (name[at] == '#' || name[at] == 'b'); ++at) {
                value += name[at] == '#' ? 1 : -1;
            }
            if (at >= name.size()) {
                return std::nullopt;
            }
            int octave = 0;
            const auto text = name.substr(at);
            try {
                std::size_t used = 0;
                octave = std::stoi(text, &used);
                if (used != text.size()) {
                    return std::nullopt;
                }
            } catch (const std::exception &) {
                return std::nullopt;
            }
            // Middle C is C4 and MIDI 60; therefore octave -1 is the lowest octave.
            const auto midi = value + (octave + 1) * 12;
            if (midi < 0 || midi > 127) {
                return std::nullopt;
            }
            return midi;
        }

        std::optional<std::pair<int, int>> toneRangeOf(const srt::JsonObject &speaker) {
            const auto it = speaker.find("toneRanges");
            if (it == speaker.end() || !it->second.isObject()) {
                return std::nullopt;
            }
            const auto &range = it->second.toObject();
            const auto read = [&range](const char *key) -> std::optional<int> {
                const auto value = range.find(key);
                if (value == range.end()) {
                    return std::nullopt;
                }
                if (value->second.isInt()) {
                    return static_cast<int>(value->second.toInt());
                }
                return value->second.isString() ? midiOf(value->second.toString()) : std::nullopt;
            };
            const auto low = read("min");
            const auto high = read("max");
            if (!low || !high) {
                return std::nullopt;
            }
            return std::make_pair(*low, *high);
        }

        /// Returns the speakers that the singer declaration lists, keyed by identifier.
        ///
        /// Read from the raw declaration rather than from an interpreted configuration because no
        /// interpreter reads this data: a display name and a tone range are host-only data.
        std::map<std::string, SpeakerCapability> declaredSpeakers(const srt::SingerSpec &singer) {
            std::map<std::string, SpeakerCapability> result;
            const auto &configuration = singer.manifestConfiguration();
            if (!configuration.isObject()) {
                return result;
            }
            const auto it = configuration.toObject().find("speakers");
            if (it == configuration.toObject().end() || !it->second.isArray()) {
                return result;
            }
            for (const auto &item : it->second.toArray()) {
                if (!item.isObject()) {
                    continue;
                }
                const auto &speaker = item.toObject();
                const auto id = speaker.find("id");
                if (id == speaker.end() || !id->second.isString()) {
                    continue;
                }
                SpeakerCapability entry;
                entry.id = id->second.toString();
                if (const auto name = speaker.find("name"); name != speaker.end()) {
                    entry.name = displayTextOf(name->second, entry.id);
                } else {
                    entry.name = srt::DisplayText(entry.id);
                }
                entry.toneRange = toneRangeOf(speaker);
                result.emplace(entry.id, std::move(entry));
            }
            return result;
        }

        /// Returns the speaker mapping of the acoustic import, which maps host identifiers to
        /// speakers of the acoustic model, or an empty map if the import declares none.
        ///
        /// Read from the import rather than from the declaration because the mapping belongs to
        /// the import. The inference tasks read the mapping from the same place, so the host
        /// identifiers offered to a host are exactly those that the tasks resolve later.
        std::map<std::string, std::string> acousticSpeakerMappingOf(const srt::SingerSpec &singer) {
            namespace Ac = ds::Api::Acoustic::L1;
            const auto import = singer.findImport(roleOf(SingerStage::Acoustic));
            if (!import || !import->options()) {
                return {};
            }
            const auto *options = import->options()->as<Ac::AcousticImportOptions>();
            return options ? options->speakerMapping : std::map<std::string, std::string>{};
        }

        /// Returns the speakers to offer a host, identified by host identifier.
        ///
        /// The singer declaration supplies the host identifiers and their display data. The
        /// speakers exported by the acoustic model are the targets of the translation, and only a
        /// speaker that resolves to one of them can be synthesized. An identifier that resolves to
        /// no model speaker is omitted and reported in \a unaddressable instead, because offering
        /// it would list a speaker that fails on use.
        ///
        /// If the import carries a mapping, the mapping defines the valid host identifiers: a
        /// synthesis looks an identifier up in the mapping and rejects an identifier that is not a
        /// key. The keys are therefore valid host identifiers, and the model's speaker names are
        /// not.
        ///
        /// If the singer declares no speakers, the host identifiers come from the remaining table:
        /// the mapping keys if a mapping exists, and the model's speaker names otherwise.
        std::vector<SpeakerCapability>
            hostSpeakersOf(const srt::SingerSpec &singer,
                           const std::vector<std::string> &modelSpeakers,
                           std::vector<std::string> &unaddressable) {
            const auto declared = declaredSpeakers(singer);
            const auto mapping = acousticSpeakerMappingOf(singer);
            std::vector<SpeakerCapability> result;
            const auto add = [&](const std::string &id, const SpeakerCapability *what) {
                const auto it = mapping.find(id);
                // If a mapping exists, a synthesis consults only the mapping, so an identifier
                // absent from it is not offered regardless of the model's speaker names.
                if (it == mapping.end() && !mapping.empty()) {
                    unaddressable.push_back(id);
                    return;
                }
                const auto &reaches = it == mapping.end() ? id : it->second;
                if (std::find(modelSpeakers.begin(), modelSpeakers.end(), reaches) ==
                    modelSpeakers.end()) {
                    unaddressable.push_back(id);
                    return;
                }
                result.push_back(what != nullptr
                                     ? *what
                                     : SpeakerCapability{id, srt::DisplayText(id), {}});
            };
            if (declared.empty()) {
                // The singer declares no speakers, so the identifiers come from the remaining
                // table. With a mapping, only its keys can be synthesized, because a synthesis
                // looks up that table. Without a mapping, the model's speaker names are used
                // unchanged.
                if (mapping.empty()) {
                    for (const auto &id : modelSpeakers) {
                        add(id, nullptr);
                    }
                } else {
                    for (const auto &entry : mapping) {
                        add(entry.first, nullptr);
                    }
                }
                return result;
            }
            for (const auto &[id, speaker] : declared) {
                add(id, &speaker);
            }
            return result;
        }

    }

    SingerCapabilities capabilitiesOf(const srt::SingerSpec &singer) {
        namespace Ac = ds::Api::Acoustic::L1;
        namespace Pi = ds::Api::Pitch::L1;
        namespace Va = ds::Api::Variance::L1;
        namespace Ds = ds::Api::DiffSinger::L1;

        SingerCapabilities result;

        const auto *duration = targetOf(singer, roleOf(SingerStage::Duration));
        const auto *pitch = targetOf(singer, roleOf(SingerStage::Pitch));
        const auto *variance = targetOf(singer, roleOf(SingerStage::Variance));
        const auto *acoustic = targetOf(singer, roleOf(SingerStage::Acoustic));
        const auto *vocoder = targetOf(singer, roleOf(SingerStage::Vocoder));

        result.duration = duration != nullptr;
        result.pitch = pitch != nullptr;
        result.variance = variance != nullptr;
        result.acoustic = acoustic != nullptr;
        result.vocoder = vocoder != nullptr;

        if (const auto *schema = exportsOf<Ac::AcousticSchema>(acoustic)) {
            // The acoustic model produces the singing voice, so its speakers are the singer's
            // speakers; the singer declaration and the import mapping supply the host identifiers.
            // The other models also list speakers. Disagreeing lists are a packaging fault of the
            // voicebank and are not reconciled here.
            result.speakers =
                hostSpeakersOf(singer, schema->speakers, result.unaddressableSpeakers);
            insertNames(result.varianceControls, schema->varianceControls);
            insertNames(result.transitionControls, schema->transitionControls);
        }
        if (const auto *schema = exportsOf<Pi::PitchSchema>(pitch)) {
            result.allowsExpressiveness = schema->allowExpressiveness;
        }
        if (const auto *schema = exportsOf<Va::VarianceSchema>(variance)) {
            for (const auto &tag : schema->predictions) {
                result.variancePredictions.insert(tag.name());
            }
        }

        // The frame widths and the vocoder rate come from the model configurations, which the
        // loader interpreted when the package opened, so reading them loads no model.
        namespace Du = ds::Api::Duration::L1;
        namespace Vo = ds::Api::Vocoder::L1;
        if (const auto *config =
                configurationOf<Du::DurationConfiguration>(duration, Du::API_INTERFACE)) {
            result.durationFrameWidth = config->frameWidth;
        }
        if (const auto *config =
                configurationOf<Pi::PitchConfiguration>(pitch, Pi::API_INTERFACE)) {
            result.pitchFrameWidth = config->frameWidth;
        }
        if (const auto *config =
                configurationOf<Va::VarianceConfiguration>(variance, Va::API_INTERFACE)) {
            result.varianceFrameWidth = config->frameWidth;
        }
        if (const auto *config =
                configurationOf<Ac::AcousticConfiguration>(acoustic, Ac::API_INTERFACE);
            config && config->sampleRate > 0 && config->hopSize > 0) {
            result.acousticFrameWidth =
                static_cast<double>(config->hopSize) / static_cast<double>(config->sampleRate);
        }
        if (const auto *config =
                configurationOf<Vo::VocoderConfiguration>(vocoder, Vo::API_INTERFACE)) {
            result.vocoderSampleRate = config->sampleRate;
        }

        // Reserved phonemes are a field of the singer category, which synthrt reads for every
        // singer contract. The singer validator has already verified that the models support them.
        result.reservedPhonemes = singer.reservedPhonemes();

        // Languages are read through wolf rather than dsinfer, because a language is a linguist
        // contribution, and the language map of the singer specifies the import for each language.
        if (auto languages = wolf::readSingerLanguages(singer)) {
            const auto value = languages.take();
            result.languages.reserve(value.entries.size());
            for (const auto &entry : value.entries) {
                result.languages.push_back(entry.language);
            }
            result.defaultLanguage = value.defaultLanguage;
        }

        return result;
    }

    std::vector<SingerEntry> scan(srt::SynthUnit &unit, const std::vector<fs::path> &paths,
                                  std::vector<srt::PackageHandle> &opened,
                                  std::vector<PackageProblem> &problems) {
        std::vector<SingerEntry> result;
        for (const auto &directory : paths) {
            // Every filesystem query below uses the overload that reports errors through an error
            // code. The contents of a search path are arbitrary, and an entry that cannot be read
            // must be reported like a package that fails to open instead of throwing out of the
            // scan. The scan runs on a worker thread, where an uncaught exception terminates the
            // process.
            std::error_code ec;
            if (!fs::is_directory(directory, ec)) {
                if (ec && ec != std::errc::no_such_file_or_directory) {
                    problems.push_back({directory, ec.message()});
                }
                continue;
            }
            fs::directory_iterator it(directory, ec);
            if (ec) {
                problems.push_back({directory, ec.message()});
                continue;
            }
            // The increment reports a failure through ec and may leave the iterator at the end,
            // so the error is checked before the end test rather than after it.
            const fs::directory_iterator end;
            for (bool first = true;; first = false) {
                if (!first) {
                    it.increment(ec);
                    if (ec) {
                        problems.push_back({directory, ec.message()});
                        break;
                    }
                }
                if (it == end) {
                    break;
                }
                const auto &entry = *it;
                std::error_code entryError;
                if (!entry.is_directory(entryError)) {
                    if (entryError) {
                        problems.push_back({entry.path(), entryError.message()});
                    }
                    continue;
                }
                if (!fs::is_regular_file(entry.path() / "desc.json", entryError)) {
                    if (entryError && entryError != std::errc::no_such_file_or_directory) {
                        problems.push_back({entry.path(), entryError.message()});
                    }
                    continue;
                }
                auto handle = unit.openPackage(entry.path(), srt::SynthUnit::Load);
                if (!handle) {
                    // A voicebank that fails to open must not hide the others, so the failure is
                    // recorded and the scan continues.
                    problems.push_back({entry.path(), handle.error().toString()});
                    continue;
                }
                auto package = handle.take();
                for (auto *spec : package.contributions(srt::SingerCategory::NAME)) {
                    auto *singer = spec->as<srt::SingerSpec>();
                    if (singer == nullptr) {
                        continue;
                    }
                    result.push_back(SingerEntry{
                        package.id(),
                        package.version(),
                        spec->locator().contributionId(),
                        spec->name(),
                        package.path(),
                        capabilitiesOf(*singer),
                        package.name(),
                        package.vendor(),
                        package.description(),
                        package.copyright(),
                        package.url(),
                    });
                }
                opened.push_back(std::move(package));
            }
        }
        return result;
    }

    const AnalysisContract *findAnalysisContract(std::string_view interfaceName) {
        // Each contract creates its own executive type through its own factory function, so the
        // table pairs the identifier with the function. The Align contract is absent because the
        // editor does not run an aligner.
        static const AnalysisContract contracts[] = {
            {otter::Api::F0::L1::API_INTERFACE,
             [](::srt::InferenceSpec &spec)
                 -> ::srt::Expected<std::unique_ptr<otter::AnalysisExecutive>> {
                 auto created = otter::Api::F0::L1::createAnalyzer(spec);
                 if (!created) {
                     return created.takeError();
                 }
                 return std::unique_ptr<otter::AnalysisExecutive>(created.take());
             }},
            {otter::Api::Note::L1::API_INTERFACE,
             [](::srt::InferenceSpec &spec)
                 -> ::srt::Expected<std::unique_ptr<otter::AnalysisExecutive>> {
                 auto created = otter::Api::Note::L1::createAnalyzer(spec);
                 if (!created) {
                     return created.takeError();
                 }
                 return std::unique_ptr<otter::AnalysisExecutive>(created.take());
             }},
        };
        for (const auto &contract : contracts) {
            if (contract.interfaceName == interfaceName) {
                return &contract;
            }
        }
        return nullptr;
    }

    bool isAnalysisContract(std::string_view interfaceName) {
        return findAnalysisContract(interfaceName) != nullptr;
    }

    std::vector<AnalyzerEntry> analyzersOf(const std::vector<srt::PackageHandle> &opened) {
        std::vector<AnalyzerEntry> result;
        for (const auto &package : opened) {
            for (auto *spec : package.contributions(srt::InferenceCategory::NAME)) {
                if (!isAnalysisContract(spec->interface())) {
                    continue;
                }
                AnalyzerEntry entry{
                    package.id(),
                    package.version(),
                    spec->locator().contributionId(),
                    spec->interface(),
                    spec->variant(),
                    spec->name(),
                };
                // A note analyzer declares its languages in its exports, where the loader has
                // already checked that the default is one of them. The contract is compared
                // first, because the downcast of the exports is not checked.
                if (spec->interface() == otter::Api::Note::L1::API_INTERFACE) {
                    if (const auto *schema = exportsOf<otter::Api::Note::L1::NoteSchema>(spec)) {
                        entry.languages = schema->languages;
                        entry.defaultLanguage = schema->defaultLanguage;
                    }
                }
                result.push_back(std::move(entry));
            }
        }
        return result;
    }

}
