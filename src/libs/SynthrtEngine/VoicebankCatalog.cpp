#include "VoicebankCatalog.h"

#include "AnalysisContracts.h"
#include "LanguageBridge.h"
#include "SingerStages.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
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
#include <wolf/Session/LinguistSession.h>

namespace lite::synthrt {

    namespace {

        /// Return the resolved import target, or null for an absent or unresolved import.
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

        /// Validate the contract before the unchecked configuration downcast.
        template <class Configuration>
        const Configuration *configurationOf(const srt::ContribSpec *target,
                                             std::string_view interfaceName) {
            if (target == nullptr || target->interfaceId() != interfaceName ||
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

        /// Remove only the first language/ prefix from model phoneme keys.
        std::set<std::string> barePhonemeKeys(const std::map<std::string, int> &table) {
            std::set<std::string> result;
            for (const auto &entry : table) {
                const auto slash = entry.first.find('/');
                result.insert(slash == std::string::npos ? entry.first : entry.first.substr(slash + 1));
            }
            return result;
        }

        /// Intersect nonempty model tables. No value means unknown coverage; an empty vector means
        /// no overlap.
        std::optional<std::vector<std::string>>
            commonPhonemes(const std::vector<std::set<std::string>> &tables) {
            std::set<std::string> result;
            bool started = false;
            for (const auto &table : tables) {
                if (table.empty()) {
                    continue;
                }
                if (!started) {
                    result = table;
                    started = true;
                    continue;
                }
                for (auto it = result.begin(); it != result.end();) {
                    if (table.count(*it) != 0) {
                        ++it;
                    } else {
                        it = result.erase(it);
                    }
                }
            }
            if (!started)
                return std::nullopt;
            return std::vector<std::string>(result.begin(), result.end());
        }

        template <class Supply>
        void feedSingers(const std::vector<SingerEntry> &catalog, Supply &&supply) {
            for (const auto &entry : catalog) {
                // Keep unknown coverage distinct from the known empty intersection of tables.
                if (!entry.capabilities.singablePhonemes) {
                    continue;
                }
                // Singers without languages have no language-catalog entry.
                if (entry.capabilities.languages.empty()) {
                    continue;
                }
                supply(entry);
            }
        }

        /// Parse a string or locale map, using '_' as the default locale key.
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

        /// Parse note names such as C4 or Bb-1, rejecting values outside MIDI 0..127.
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
            // C4 is MIDI 60.
            const auto midi = value + (static_cast<long long>(octave) + 1) * 12;
            if (midi < 0 || midi > 127) {
                return std::nullopt;
            }
            return static_cast<int>(midi);
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
                    const auto midi = value->second.toInt();
                    if (midi < 0 || midi > 127)
                        return std::nullopt;
                    return static_cast<int>(midi);
                }
                return value->second.isString() ? midiOf(value->second.toString()) : std::nullopt;
            };
            const auto low = read("min");
            const auto high = read("max");
            if (!low || !high || *low > *high) {
                return std::nullopt;
            }
            return std::make_pair(*low, *high);
        }

        /// Preserve declaration order because it determines the default speaker.
        std::vector<SpeakerCapability> declaredSpeakers(const srt::SingerSpec &singer) {
            std::vector<SpeakerCapability> result;
            std::set<std::string> seen;
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
                if (seen.insert(entry.id).second)
                    result.push_back(std::move(entry));
            }
            return result;
        }

        /// Map singer-side IDs to acoustic model speaker names.
        std::map<std::string, std::string> acousticSpeakerMappingOf(const srt::SingerSpec &singer) {
            namespace Ac = ds::Api::Acoustic::L1;
            const auto import = singer.findImport(roleOf(SingerStage::Acoustic));
            if (!import || !import->options()) {
                return {};
            }
            const auto *options = import->options()->as<Ac::AcousticImportOptions>();
            return options ? options->speakerMapping : std::map<std::string, std::string>{};
        }

        /// Keep host speaker IDs that resolve through the acoustic mapping; report the others.
        /// Without declared speakers, use mapping keys or, if unmapped, model speaker names.
        std::vector<SpeakerCapability>
            hostSpeakersOf(const srt::SingerSpec &singer,
                           const std::vector<std::string> &modelSpeakers,
                           std::vector<std::string> &unaddressable) {
            const auto declared = declaredSpeakers(singer);
            const auto mapping = acousticSpeakerMappingOf(singer);
            std::vector<SpeakerCapability> result;
            const auto add = [&](const std::string &id, const SpeakerCapability *what) {
                const auto it = mapping.find(id);
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
            for (const auto &speaker : declared) {
                add(speaker.id, &speaker);
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
            // Use the acoustic model and its import mapping to derive host speaker IDs.
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

        std::vector<std::set<std::string>> tables;
        if (const auto *config =
                configurationOf<Du::DurationConfiguration>(duration, Du::API_INTERFACE)) {
            tables.push_back(barePhonemeKeys(config->phonemes));
        }
        if (const auto *config =
                configurationOf<Pi::PitchConfiguration>(pitch, Pi::API_INTERFACE)) {
            tables.push_back(barePhonemeKeys(config->phonemes));
        }
        if (const auto *config =
                configurationOf<Va::VarianceConfiguration>(variance, Va::API_INTERFACE)) {
            tables.push_back(barePhonemeKeys(config->phonemes));
        }
        if (const auto *config =
                configurationOf<Ac::AcousticConfiguration>(acoustic, Ac::API_INTERFACE)) {
            tables.push_back(barePhonemeKeys(config->phonemes));
        }
        result.singablePhonemes = commonPhonemes(tables);

        // Reserved markers are validated by the singer loader.
        result.reservedPhonemes = singer.reservedPhonemes();

        if (auto languages = wolf::readSingerLanguages(singer)) {
            const auto value = languages.take();
            result.languages.reserve(value.entries.size());
            for (const auto &entry : value.entries) {
                result.languages.push_back(entry.language);
                if (const auto *linguist = targetOf(singer, entry.role)) {
                    result.languageNames.emplace(entry.language, linguist->name());
                    // ContribSpec::as() requires a category check; LinguistSpec is not tied to an
                    // API level.
                    if (linguist->locator().category() == wolf::LINGUIST_CATEGORY) {
                        const auto *spec = linguist->as<wolf::LinguistSpec>();
                        if (!spec->defaultLyric().empty())
                            result.defaultLyrics.emplace(entry.language, spec->defaultLyric());
                    }
                }
            }
            result.defaultLanguage = value.defaultLanguage;
        }

        return result;
    }

    void feedSingerPhonemes(wolf::LinguistSession &session,
                            const std::vector<SingerEntry> &catalog) {
        feedSingers(catalog, [&session](const SingerEntry &entry) {
            session.setSingerPhonemes(
                wolf::SingerRef{
                    srt::ContribLocator(entry.packageId, srt::SingerCategory::NAME,
                                        entry.contributionId),
                    entry.packageVersion,
                },
                *entry.capabilities.singablePhonemes);
        });
    }

    void feedSingerPhonemes(LanguageBridge &language, const std::vector<SingerEntry> &catalog) {
        feedSingers(catalog, [&language](const SingerEntry &entry) {
            language.setSingerPhonemes(
                {entry.packageId, entry.contributionId, entry.packageVersion},
                *entry.capabilities.singablePhonemes);
        });
    }

    std::vector<SingerEntry> scan(srt::SynthUnit &unit, const std::vector<fs::path> &paths,
                                  std::vector<srt::PackageHandle> &opened,
                                  std::vector<PackageProblem> &problems) {
        std::vector<SingerEntry> result;
        const auto loadPackage = [&](const fs::path &path) {
            auto handle = unit.openPackage(path, srt::SynthUnit::Load);
            if (!handle) {
                problems.push_back({path, handle.error().toString()});
                return;
            }
            auto package = handle.take();
            if (std::any_of(opened.begin(), opened.end(), [&](const auto &existing) {
                    return existing.id() == package.id() && existing.version() == package.version();
                }))
                return;
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
                    package.license(),
                    package.url(),
                });
            }
            opened.push_back(std::move(package));
        };
        for (const auto &directory : paths) {
            // Report filesystem errors per path so an unreadable entry does not abort the worker
            // scan.
            std::error_code ec;
            if (!fs::is_directory(directory, ec)) {
                if (ec && ec != std::errc::no_such_file_or_directory) {
                    problems.push_back({directory, ec.message()});
                }
                continue;
            }
            if (fs::is_regular_file(directory / "desc.json", ec)) {
                loadPackage(directory);
                continue;
            }
            if (ec && ec != std::errc::no_such_file_or_directory) {
                problems.push_back({directory, ec.message()});
                continue;
            }
            ec.clear();
            fs::directory_iterator it(directory, ec);
            if (ec) {
                problems.push_back({directory, ec.message()});
                continue;
            }
            // Check increment errors before the end test, since a failed increment may reach end.
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
                loadPackage(entry.path());
            }
        }
        return result;
    }

    const AnalysisContract *findAnalysisContract(std::string_view interfaceName) {
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
                if (!isAnalysisContract(spec->interfaceId())) {
                    continue;
                }
                AnalyzerEntry entry{
                    package.id(),
                    package.version(),
                    spec->locator().contributionId(),
                    spec->interfaceId(),
                    spec->variant(),
                    spec->name(),
                };
                // Check the contract before downcasting analyzer exports.
                if (spec->interfaceId() == otter::Api::Note::L1::API_INTERFACE) {
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
