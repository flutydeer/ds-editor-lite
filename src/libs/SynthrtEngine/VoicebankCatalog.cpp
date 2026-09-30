#include "VoicebankCatalog.h"

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
#include <dsinfer/Api/Inferences/Pitch/1/PitchApiL1.h>
#include <dsinfer/Api/Inferences/Variance/1/VarianceApiL1.h>
#include <dsinfer/Api/Singers/DiffSinger/1/DiffSingerApiL1.h>

#include <otter/Api/F0/1/F0ApiL1.h>
#include <otter/Api/Note/1/NoteApiL1.h>

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

        /// How the singer's own speaker identifiers reach the acoustic model, as the import that
        /// will sing them declares it.
        ///
        /// Read from the import rather than from the declaration because that is where the
        /// translation belongs, and read from the same place the inference tasks read it so the
        /// identifiers a host is offered are exactly the ones that can be looked up later.
        std::map<std::string, std::string> acousticSpeakerMappingOf(const srt::SingerSpec &singer) {
            namespace Ac = ds::Api::Acoustic::L1;
            const auto import = singer.findImport("singer/acoustic");
            if (!import || !import->options()) {
                return {};
            }
            const auto *options = import->options()->as<Ac::AcousticImportOptions>();
            return options ? options->speakerMapping : std::map<std::string, std::string>{};
        }

        /// The speakers to offer a host, in the identifiers the host names them by.
        ///
        /// The singer's declaration is the source of those identifiers and of what to show beside
        /// them; the acoustic model's exports are the other end of the translation, and only a
        /// speaker that reaches one of them can be sung. An identifier that reaches nothing is
        /// left out and reported in \a unaddressable instead: offering it would put a speaker in a
        /// menu that fails when it is used.
        ///
        /// When the import carries a mapping, that same mapping decides what an identifier may be:
        /// a synthesis looks a name up in it and refuses one that is not a key, so the keys are
        /// what a host may hold, and the model's own names are not.
        ///
        /// A singer that declares no speakers has nothing of its own to name them with, so the
        /// identifiers come from whichever table is left: the mapping's keys when there is a
        /// mapping, and the model's names when there is not.
        std::vector<SpeakerCapability>
            hostSpeakersOf(const srt::SingerSpec &singer,
                           const std::vector<std::string> &modelSpeakers,
                           std::vector<std::string> &unaddressable) {
            const auto declared = declaredSpeakers(singer);
            const auto mapping = acousticSpeakerMappingOf(singer);
            std::vector<SpeakerCapability> result;
            const auto add = [&](const std::string &id, const SpeakerCapability *what) {
                const auto it = mapping.find(id);
                // A mapping that is there at all is the only table a synthesis consults, so an
                // identifier missing from it is not offered however the model names its speakers.
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
                // Nothing of the singer's own names its speakers, so the identifiers come from
                // whichever table is left: with a mapping only its keys can be sung, because that
                // is the table a synthesis looks up; with none, the model's names go through as
                // they are.
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
            // The acoustic model is the one that sings, so its speakers are the singer's; the
            // singer's declaration and its import mapping are what a host names them by. The other
            // models have speaker lists too, and a voicebank whose lists disagree is a packaging
            // fault rather than something to reconcile here.
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

    bool isAnalysisContract(std::string_view interfaceName) {
        return interfaceName == otter::Api::F0::L1::API_INTERFACE ||
               interfaceName == otter::Api::Note::L1::API_INTERFACE;
    }

    std::vector<AnalyzerEntry> analyzersOf(const std::vector<srt::PackageHandle> &opened) {
        std::vector<AnalyzerEntry> result;
        for (const auto &package : opened) {
            for (auto *spec : package.contributions(srt::InferenceCategory::NAME)) {
                if (!isAnalysisContract(spec->interface())) {
                    continue;
                }
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
