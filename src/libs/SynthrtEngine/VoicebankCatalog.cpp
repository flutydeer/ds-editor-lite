#include "VoicebankCatalog.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>

#include <synthrt/Core/ContribImportBinding.h>
#include <synthrt/Support/JSON.h>

#include <dsinfer/Api/Inferences/Acoustic/1/AcousticApiL1.h>
#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>
#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>
#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>

#include <otter/Analysis/AnalysisContrib.h>

#include <wolf/Linguist/SingerLanguages.h>

namespace lite::synthrt {

    namespace {

        /// Returns the declaration an import reaches, or null.
        ///
        /// Null for a package opened without loading, and for an import whose target was not
        /// resolved. Both are ordinary here: this runs while listing voicebanks, and a singer
        /// missing a stage is a fact about the singer rather than an error.
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

        void insertNames(std::set<std::string> &into, const std::set<ds::ParamTag> &tags) {
            for (const auto &tag : tags) {
                into.insert(tag.name());
            }
        }

        /// Builds a display text from the manifest's own shape for one.
        ///
        /// A plain string is the default text; an object is a map of locale to text where "_" is
        /// the default, which is how the specification writes a translated name.
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

        /// Reads a note name such as C4, F#3 or Bb-1 as a MIDI number.
        ///
        /// Voicebanks write tone ranges the way a musician does rather than as numbers, and
        /// nothing on this line interprets them -- the models never see a tone range, it is
        /// guidance for the person choosing a speaker. So it is read here, where the host's own
        /// reading of the singer happens, and left alone when it is not a note name.
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
            // Middle C is C4 and MIDI 60, so octave -1 is the bottom one.
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

        /// What the singer declaration says about each speaker, by identifier.
        ///
        /// Read from the raw declaration rather than from an interpreted configuration because no
        /// interpreter on this line reads it: a display name and a tone range are for the host.
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

    }

    SingerCapabilities capabilitiesOf(const srt::SingerSpec &singer) {
        namespace Ac = ds::Api::Acoustic::L1;
        namespace Pi = ds::Api::Pitch::L1;
        namespace Va = ds::Api::Variance::L1;
        namespace Ds = ds::Api::DiffSinger::L1;

        SingerCapabilities result;

        const auto *duration = targetOf(singer, "singer/duration");
        const auto *pitch = targetOf(singer, "singer/pitch");
        const auto *variance = targetOf(singer, "singer/variance");
        const auto *acoustic = targetOf(singer, "singer/acoustic");
        const auto *vocoder = targetOf(singer, "singer/vocoder");

        result.duration = duration != nullptr;
        result.pitch = pitch != nullptr;
        result.variance = variance != nullptr;
        result.acoustic = acoustic != nullptr;
        result.vocoder = vocoder != nullptr;

        if (const auto *schema = exportsOf<Ac::AcousticSchema>(acoustic)) {
            // The acoustic model is the one that sings, so its speakers are the singer's. The
            // other models have speaker lists too, and a voicebank whose lists disagree is a
            // packaging fault rather than something to reconcile here.
            const auto declared = declaredSpeakers(singer);
            result.speakers.reserve(schema->speakers.size());
            for (const auto &id : schema->speakers) {
                if (const auto it = declared.find(id); it != declared.end()) {
                    result.speakers.push_back(it->second);
                } else {
                    // The model has a speaker the singer says nothing about. Usable, and shown
                    // under the only name there is.
                    result.speakers.push_back(SpeakerCapability{id, srt::DisplayText(id), {}});
                }
            }
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

        // Reserved phonemes are the singer category's own field, read by synthrt for every
        // singer contract alike; the singer's validator has already checked its models know them.
        result.reservedPhonemes = singer.reservedPhonemes();

        // Languages come from wolf rather than from dsinfer: on this line a language is a linguist
        // contribution, and the singer's map names which of its imports leads to each one.
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
            std::error_code ec;
            if (!fs::is_directory(directory, ec)) {
                continue;
            }
            for (const auto &entry : fs::directory_iterator(directory, ec)) {
                if (ec) {
                    problems.push_back({directory, ec.message()});
                    break;
                }
                if (!entry.is_directory() || !fs::is_regular_file(entry.path() / "desc.json")) {
                    continue;
                }
                auto handle = unit.openPackage(entry.path(), srt::SynthUnit::Load);
                if (!handle) {
                    // One voicebank that will not open must not hide the rest, so this is
                    // collected and the scan goes on.
                    problems.push_back({entry.path(), handle.error().toString()});
                    continue;
                }
                auto package = handle.take();
                for (auto *spec : package.contributions("singer")) {
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

    std::vector<AnalyzerEntry> analyzersOf(const std::vector<srt::PackageHandle> &opened) {
        std::vector<AnalyzerEntry> result;
        for (const auto &package : opened) {
            for (auto *spec : package.contributions(otter::ANALYSIS_CATEGORY)) {
                result.push_back(AnalyzerEntry{
                    package.id(),
                    package.version(),
                    spec->locator().contributionId(),
                    spec->interface(),
                    spec->variant(),
                    spec->name(),
                });
            }
        }
        return result;
    }

}
