// Audits a real voicebank against the complete chain in both directions.
//
// The other tests in this set use fixtures built for them. This test runs the complete chain on a
// voicebank that was not written for the test (a published package in the 2.3 format, converted)
// and reports the result in both directions: which stages open, which languages route, and which
// part of the phoneme inventory declared by each language the singer can sing.
//
// The audit runs in both directions intentionally. The chain checking the voicebank is the
// ordinary direction and detects a faulty conversion. The voicebank checking the chain detects the
// less obvious defects: a phoneme inventory that the chain routes to but that the models do not
// contain yields a route that loads without error and produces no sound.

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <synthrt/Core/PackageHandle.h>
#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Support/JSON.h>

#include <wolf/Session/LinguistSession.h>

#include "LanguageBridge.h"
#include "SingerPipeline.h"
#include "SynthrtBootstrap.h"
#include "VoicebankCatalog.h"

namespace fs = std::filesystem;
using namespace lite::synthrt;

namespace {

    int failures = 0;
    int findings = 0;

    void expect(bool condition, const std::string &what) {
        std::cout << (condition ? "  ok   " : "  FAIL ") << what << "\n";
        if (!condition) {
            ++failures;
        }
    }

    /// Reports a finding: a property of the voicebank that the chain cannot correct and must not
    /// hide.
    ///
    /// Findings are kept separate from failures intentionally. A failure indicates that the two
    /// sides disagree on something the tooling controls, and requires a code change. A finding
    /// indicates that a declaration of the voicebank does not hold. Detecting such declarations is
    /// the purpose of running the chain on a real voicebank, and failing on them would discourage
    /// running this test.
    void finding(const std::string &what) {
        std::cout << "  NOTE  " << what << "\n";
        ++findings;
    }

    /// Returns the error text of \a value, or "(no error)" if \a value holds a value. Calling
    /// error() on a successful Expected reads the wrong union member.
    template <class T>
    std::string why(const srt::Expected<T> &value) {
        return value ? std::string("(no error)") : value.error().toString();
    }

    std::string join(const std::vector<std::string> &values, std::size_t limit = 12) {
        std::string result;
        for (std::size_t i = 0; i < values.size() && i < limit; ++i) {
            result += (i ? " " : "") + values[i];
        }
        if (values.size() > limit) {
            result += " ... (" + std::to_string(values.size()) + " total)";
        }
        return result;
    }

    /// Returns the path of the phoneme table that the models of the directory \a directory use.
    ///
    /// Current conversions name a table after the model (`<model>.phonemes.json`); older
    /// conversions name it `phonemes.json`. The audit concerns the table content rather than the
    /// file name, so either name is accepted.
    fs::path phonemeTable(const fs::path &directory) {
        std::error_code ec;
        for (const auto &entry : fs::directory_iterator(directory, ec)) {
            const auto name = entry.path().filename().string();
            if (name == "phonemes.json" || name.ends_with(".phonemes.json")) {
                return entry.path();
            }
        }
        return directory / "phonemes.json";
    }

    /// Returns the keys of the phoneme table of a model exactly as the file writes them, language
    /// prefixes included.
    ///
    /// The audit needs the written form for one purpose: to take a keyed entry of the package and
    /// check that the catalog derived its bare token. Every other use of the table goes through
    /// singerPhonemes(), which removes the prefix.
    std::vector<std::string> tableKeys(const fs::path &table) {
        std::ifstream file(table);
        if (!file.is_open()) {
            return {};
        }
        const std::string text((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        const auto parsed = srt::JsonValue::fromJson(text, true);
        if (!parsed.isObject()) {
            return {};
        }
        std::vector<std::string> result;
        for (const auto &[key, value] : parsed.toObject()) {
            result.push_back(key);
        }
        return result;
    }

    /// Returns \a key with the language prefix removed.
    ///
    /// A table of a model is keyed `<language>/<phoneme>`, and only the first prefix is removed,
    /// because the rest of the key is the phoneme and may contain a slash of its own. A key without
    /// a prefix is a language independent token such as a breath and is returned unchanged.
    std::string barePhoneme(const std::string &key) {
        const auto slash = key.find('/');
        return slash == std::string::npos ? key : key.substr(slash + 1);
    }

    /// Returns the phonemes contained in one model of the singer, as the editor reads them.
    ///
    /// wolf does not open voicebank formats, so reading the table is the responsibility of the
    /// host. Only the host has the information that the table is keyed `<language>/<phoneme>` with
    /// bare markers. The result is one list per table instead of one list per language, which is
    /// the form the session accepts: a phoneme is either in the model or not, regardless of the
    /// requesting language.
    std::vector<std::string> singerPhonemes(const fs::path &table) {
        std::set<std::string> distinct;
        for (const auto &key : tableKeys(table)) {
            distinct.insert(barePhoneme(key));
        }
        return {distinct.begin(), distinct.end()};
    }

    /// Returns the phonemes that every model of \a package contains, read from the package.
    ///
    /// This is the independent side of the cross-check of the table the catalog derives: the same
    /// files, read a second way, without the catalog. Every frame model is read and the result is
    /// their intersection, because the derivation supplies the intersection, and a comparison
    /// against one model alone would hold only for a voicebank whose models agree.
    std::vector<std::string> modelPhonemes(const fs::path &package) {
        std::set<std::string> result;
        bool started = false;
        for (const auto *stage : {"duration", "pitch", "variance", "acoustic"}) {
            const auto table = singerPhonemes(phonemeTable(package / "inferences" / stage));
            if (table.empty()) {
                // A model that declares no table constrains nothing, which is the rule the
                // derivation applies as well.
                continue;
            }
            const std::set<std::string> one(table.begin(), table.end());
            if (!started) {
                result = one;
                started = true;
                continue;
            }
            for (auto it = result.begin(); it != result.end();) {
                if (one.count(*it) != 0) {
                    ++it;
                } else {
                    it = result.erase(it);
                }
            }
        }
        return {result.begin(), result.end()};
    }

    /// Returns the path of the first character declaration in the package, or an empty path if
    /// none exists.
    ///
    /// The doctoring below requires one declaration to edit, and the character that the
    /// declaration belongs to does not affect the result: a reserved phoneme missing from the
    /// models must cause the package to be rejected, regardless of which character declares it.
    fs::path anyCharacterConfig(const fs::path &package) {
        std::error_code ec;
        for (const auto &entry : fs::directory_iterator(package / "characters", ec)) {
            const auto config = entry.path() / "config.json";
            if (fs::is_regular_file(config, ec)) {
                return config;
            }
        }
        return {};
    }

    /// Reads a JSON file. Returns a null value if the file cannot be opened.
    srt::JsonValue readJson(const fs::path &path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return srt::JsonValue::fromJson("null", true);
        }
        const std::string text((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        return srt::JsonValue::fromJson(text, true);
    }

    /// Speaker declaration of the singer.
    ///
    /// Read directly from the character manifest instead of through the catalog, because the
    /// comparison is only meaningful if both sides are read independently. Otherwise a catalog
    /// that named speakers after the model instead of the singer would match itself and mismatch
    /// the package.
    struct SpeakerDeclaration {
        /// Speaker identifiers held by a host, in declaration order.
        std::vector<std::string> ids;

        /// Mapping that the acoustic import applies to the identifiers. Empty if no mapping is
        /// declared; the engine then treats each identifier as a speaker name of the model.
        std::map<std::string, std::string> mapping;
    };

    SpeakerDeclaration readSpeakerDeclaration(const fs::path &config) {
        SpeakerDeclaration result;
        const auto parsed = readJson(config);
        if (!parsed.isObject()) {
            return result;
        }
        const auto &manifest = parsed.toObject();
        const auto configuration = manifest.find("configuration");
        if (configuration != manifest.end() && configuration->second.isObject()) {
            const auto &object = configuration->second.toObject();
            const auto speakers = object.find("speakers");
            if (speakers != object.end() && speakers->second.isArray()) {
                for (const auto &item : speakers->second.toArray()) {
                    if (!item.isObject()) {
                        continue;
                    }
                    const auto id = item.toObject().find("id");
                    if (id != item.toObject().end() && id->second.isString()) {
                        result.ids.push_back(id->second.toString());
                    }
                }
            }
        }
        const auto imports = manifest.find("imports");
        if (imports == manifest.end() || !imports->second.isArray()) {
            return result;
        }
        for (const auto &item : imports->second.toArray()) {
            if (!item.isObject()) {
                continue;
            }
            const auto &import = item.toObject();
            const auto role = import.find("role");
            if (role == import.end() || !role->second.isString() ||
                role->second.toString() != "singer/acoustic") {
                continue;
            }
            const auto options = import.find("options");
            if (options == import.end() || !options->second.isObject()) {
                continue;
            }
            const auto mapping = options->second.toObject().find("speakerMapping");
            if (mapping == options->second.toObject().end() || !mapping->second.isObject()) {
                continue;
            }
            for (const auto &[from, to] : mapping->second.toObject()) {
                if (to.isString()) {
                    result.mapping.emplace(from, to.toString());
                }
            }
        }
        return result;
    }

    /// Returns the speaker names accepted by the acoustic model, which are the targets of the
    /// speaker mapping of the singer.
    std::vector<std::string> acousticModelSpeakers(const fs::path &config) {
        std::vector<std::string> result;
        const auto parsed = readJson(config);
        if (!parsed.isObject()) {
            return result;
        }
        const auto &manifest = parsed.toObject();
        const auto exports = manifest.find("exports");
        if (exports == manifest.end() || !exports->second.isObject()) {
            return result;
        }
        const auto speakers = exports->second.toObject().find("speakers");
        if (speakers == exports->second.toObject().end() || !speakers->second.isArray()) {
            return result;
        }
        for (const auto &item : speakers->second.toArray()) {
            if (item.isString()) {
                result.push_back(item.toString());
            }
        }
        return result;
    }

    /// Links \a target to \a source by symbolic link if the platform permits it, otherwise by
    /// hard link. Returns whether either link was created.
    bool link(const fs::path &source, const fs::path &target, std::error_code &ec) {
        ec.clear();
        fs::create_symlink(source, target, ec);
        if (!ec) {
            return true;
        }
        ec.clear();
        fs::create_hard_link(source, target, ec);
        if (!ec) {
            return true;
        }
        ec.clear();
        return false;
    }

    /// Mirrors a package, copying the JSON declarations and linking all other files. Returns
    /// whether the mirror is complete.
    ///
    /// The models are hundreds of megabytes and are not under test; the declarations are small and
    /// are under test. Linking the models and copying the declarations produces a package that is
    /// equivalent for this test, in the time needed to traverse the directory.
    bool mirror(const fs::path &from, const fs::path &to) {
        std::error_code ec;
        fs::create_directories(to, ec);
        for (const auto &entry : fs::recursive_directory_iterator(from, ec)) {
            const auto relative = fs::relative(entry.path(), from, ec);
            const auto target = to / relative;
            if (entry.is_directory()) {
                fs::create_directories(target, ec);
            } else if (entry.path().extension() == ".json") {
                fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
            } else if (!link(entry.path(), target, ec)) {
                // Copying is the fallback if neither link is available: Windows grants the
                // symlink privilege only to administrators, and a hard link requires both paths
                // on one volume, which is not guaranteed for the voicebank and the staging
                // directory.
                fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
            }
            if (ec) {
                return false;
            }
        }
        return !ec;
    }

    /// Replaces the first occurrence of \a what in a file with \a with (the doctoring). Returns
    /// false if \a what does not occur or the file cannot be read or written, because a doctoring
    /// that silently changed nothing would pass the test by loading an unmodified package.
    bool doctor(const fs::path &path, const std::string &what, const std::string &with) {
        std::ifstream in(path);
        if (!in.is_open()) {
            return false;
        }
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        const auto at = text.find(what);
        if (at == std::string::npos) {
            return false;
        }
        text.replace(at, what.size(), with);
        std::ofstream out(path, std::ios::trunc);
        out << text;
        return out.good();
    }

    const char *readinessName(wolf::Readiness value) {
        switch (value) {
            case wolf::Readiness::Ready:
                return "Ready";
            case wolf::Readiness::Cold:
                return "Cold";
            case wolf::Readiness::Unavailable:
                return "Unavailable";
        }
        return "?";
    }

    const char *coverageName(wolf::LanguageStatus::CoverageKind value) {
        switch (value) {
            case wolf::LanguageStatus::CoverageKind::Unknown:
                return "unknown";
            case wolf::LanguageStatus::CoverageKind::Exact:
                return "exact";
            case wolf::LanguageStatus::CoverageKind::Lower:
                return "at least";
        }
        return "?";
    }

    struct Probe {
        std::string language;
        std::vector<LanguageBridge::Word> words;
    };


}

int main(int argc, char *argv[]) {
    // A published voicebank is hundreds of megabytes and cannot be stored in the tree, so the test
    // takes the voicebank path from the command line or the build configuration and is skipped if
    // no voicebank exists there. Skipping instead of failing is correct, because nothing is broken
    // if the machine has no voicebank.
    const fs::path voicebank(argc > 1 ? argv[1] : TEST_VOICEBANK_DIR);
    const fs::path languagePackages(argc > 2 ? argv[2] : TEST_LANGUAGE_PACKAGES);
    const fs::path pluginRoot(argc > 3 ? argv[3] : TEST_PLUGIN_ROOT);
    const fs::path runtimePath(argc > 4 ? argv[4] : TEST_ONNXRUNTIME_DIR);

    if (voicebank.empty() || !fs::is_regular_file(voicebank / "desc.json")) {
        std::cerr << "no converted voicebank at " << voicebank << "; skipping. Set "
                  << "LITE_AUDIT_VOICEBANK to a voicebank converted by "
                     "scripts/convert-voicebank.py\n";
        return 77;
    }
    if (!fs::is_directory(languagePackages)) {
        std::cerr << "no language packages at " << languagePackages << "; skipping\n";
        return 77;
    }

    std::cout << "voicebank: " << voicebank << "\n";

    // Dependencies are resolved through the package paths, which differ from the scan roots. The
    // parent directory of the voicebank is included because packages are looked up by id in these
    // paths.
    auto bootstrap = Bootstrap::create(pluginRoot,
                                       {languagePackages, voicebank.parent_path()},
                                       runtimePath, Backend::Cpu, -1);
    expect(static_cast<bool>(bootstrap), "the unit bootstraps: " + why(bootstrap));
    if (!bootstrap) {
        return 1;
    }
    auto unit = bootstrap.take();
    expect(unit->hasDriver(), "an ONNX driver is loaded");

    std::cout << "\n== the chain reading the voicebank ==\n";

    std::vector<srt::PackageHandle> opened;
    std::vector<PackageProblem> problems;
    auto singers = scan(unit->unit(), {voicebank.parent_path()}, opened, problems);
    for (const auto &problem : problems) {
        std::cout << "  note  " << problem.path << ": " << problem.reason << "\n";
    }
    expect(!singers.empty(), "the converted package loads and contributes a singer");
    if (singers.empty()) {
        return 1;
    }

    const auto &singer = singers.front();
    std::cout << "  singer " << singer.packageId << ":singer/" << singer.contributionId << " v"
              << singer.packageVersion.toString() << "\n";
    const auto &capabilities = singer.capabilities;
    expect(capabilities.complete(), "the singer has every stage required for synthesis");
    expect(capabilities.duration && capabilities.pitch && capabilities.variance,
           "the singer imports all three optional stages");
    std::vector<std::string> speakerNames;
    for (const auto &speaker : capabilities.speakers) {
        speakerNames.push_back(speaker.id);
    }
    std::cout << "  speakers   " << join(speakerNames) << "\n";
    std::cout << "  predicts   " << join({capabilities.variancePredictions.begin(),
                                          capabilities.variancePredictions.end()}) << "\n";
    std::cout << "  controls   " << join({capabilities.varianceControls.begin(),
                                          capabilities.varianceControls.end()}) << "\n";
    std::cout << "  transition " << join({capabilities.transitionControls.begin(),
                                          capabilities.transitionControls.end()}) << "\n";
    std::cout << "  languages  " << join(capabilities.languages) << " (default "
              << capabilities.defaultLanguage << ")\n";
    // The frame grids and the audio rate the inference tasks use, read from the configurations.
    std::cout << "  frames     duration " << capabilities.durationFrameWidth << " s, pitch "
              << capabilities.pitchFrameWidth << " s, variance " << capabilities.varianceFrameWidth
              << " s, acoustic " << capabilities.acousticFrameWidth << " s; vocoder "
              << capabilities.vocoderSampleRate << " Hz\n";
    expect(capabilities.durationFrameWidth > 0 && capabilities.pitchFrameWidth > 0 &&
               capabilities.varianceFrameWidth > 0 && capabilities.acousticFrameWidth > 0,
           "every stage declares its frame width");
    expect(capabilities.vocoderSampleRate > 0, "the vocoder declares its sample rate");
    // Read from the singer declaration instead of agreed out of band, which is the purpose of the
    // key: the loader has already rejected the package if its models lack any of these phonemes.
    const auto &markers = capabilities.reservedPhonemes;
    std::cout << "  reserved   " << join(markers, 20) << "\n";
    expect(!markers.empty(),
           "the singer declaration lists the phonemes that a lyric may specify directly");
    // Counted from the package: the languages available to a singer are the language packages that
    // its manifest depends on, so a voicebank converted with a different language set still passes
    // the audit.
    {
        std::ifstream manifest(voicebank / "desc.json");
        const std::string text((std::istreambuf_iterator<char>(manifest)),
                               std::istreambuf_iterator<char>());
        std::size_t declaredLanguages = 0;
        for (auto at = text.find("wolf/lang-"); at != std::string::npos;
             at = text.find("wolf/lang-", at + 1)) {
            ++declaredLanguages;
        }
        expect(capabilities.languages.size() == declaredLanguages,
               "every language the package depends on reaches a linguist: declared "
                   + std::to_string(declaredLanguages) + ", reached "
                   + std::to_string(capabilities.languages.size()));
    }

    // The identifiers held by a host are the speaker identifiers of the singer: the identifiers in
    // its declaration, which are the keys of its import mapping. The speaker names of the acoustic
    // model are the targets of that mapping. With the mapping `base -> xuanyi-base` of the club
    // voicebank, a catalog that offered the model names placed a speaker in the menu that every
    // synthesis then failed to look up, because the lookup table is keyed by the singer
    // identifiers.
    const auto declaration = readSpeakerDeclaration(singer.packagePath / "characters" /
                                                    singer.contributionId / "config.json");
    const auto modelSpeakers =
        acousticModelSpeakers(singer.packagePath / "inferences/acoustic/config.json");
    auto declared = declaration.ids;
    auto offered = speakerNames;
    std::sort(declared.begin(), declared.end());
    std::sort(offered.begin(), offered.end());
    std::cout << "  declares   " << join(declared) << "\n";
    std::cout << "  model has  " << join(modelSpeakers) << "\n";
    for (const auto &[from, to] : declaration.mapping) {
        std::cout << "  maps       " << from << " -> " << to << "\n";
    }
    expect(!declared.empty(), "the singer declaration lists its speakers");
    expect(offered == declared,
           "the speakers offered to a host are the declared speakers of the singer"
               + (offered == declared ? std::string() : ": offered " + join(offered)));
    for (const auto &speaker : capabilities.speakers) {
        // The same lookup that a synthesis performs with this identifier (see above): an identifier
        // that is not a key of the import mapping, such as a model speaker name, fails it.
        expect(declaration.mapping.empty() || declaration.mapping.count(speaker.id) != 0,
               "speaker " + speaker.id +
                   " is a key of the acoustic mapping, which synthesis uses for the lookup");
    }
    expect(capabilities.unaddressableSpeakers.empty(),
           "every speaker offered reaches the acoustic model"
               + (capabilities.unaddressableSpeakers.empty()
                      ? std::string()
                      : ": " + join(capabilities.unaddressableSpeakers)));

    std::cout << "\n== the models opening ==\n";
    srt::SingerSpec *spec = nullptr;
    for (auto *contribution : opened.front().contributions("singer")) {
        if (auto *found = contribution->as<srt::SingerSpec>()) {
            spec = found;
        }
    }
    expect(spec != nullptr, "the singer declaration is reachable");
    if (spec == nullptr) {
        return 1;
    }
    auto pipeline = SingerPipeline::create(opened.front(), *spec);
    expect(static_cast<bool>(pipeline), "the pipeline builds: " + why(pipeline));
    if (pipeline) {
        auto built = pipeline.take();
        auto duration = built->duration();
        expect(static_cast<bool>(duration), "duration opens: " + why(duration));
        auto pitch = built->pitch();
        expect(static_cast<bool>(pitch), "pitch opens: " + why(pitch));
        auto variance = built->variance();
        expect(static_cast<bool>(variance), "variance opens: " + why(variance));
        auto acoustic = built->acoustic();
        expect(static_cast<bool>(acoustic), "acoustic opens: " + why(acoustic));
        auto vocoder = built->vocoder();
        expect(static_cast<bool>(vocoder), "vocoder opens: " + why(vocoder));
    }

    std::cout << "\n== the voicebank reading the chain ==\n";
    // The independent side of the cross-check below: the package's four tables, read by this file.
    const auto fromFiles = modelPhonemes(singer.packagePath);
    expect(!fromFiles.empty(), "the phoneme tables of the models are readable");
    std::cout << "  the models contain " << fromFiles.size() << " distinct phoneme(s)\n";

    // The table the chain measures a coverage with comes from the catalog, and the audit supplies it
    // through the production entry point rather than reading a file of its own. Everything asserted
    // below therefore describes what the editor runs.
    const auto &derived = capabilities.singablePhonemes;
    expect(!derived.empty(),
           "the catalog derives a phoneme table for the singer, which is what a measured coverage "
           "requires");
    expect(derived == fromFiles,
           "the derived table is the intersection an independent read of the models yields"
               + (derived == fromFiles
                      ? std::string()
                      : ": derived " + std::to_string(derived.size()) + ", read "
                            + std::to_string(fromFiles.size())));
    // Necessary condition that does not reimplement the intersection: a symbol absent from any
    // model must be absent from the derived table, because the chain would then sing it without a
    // frame from that model.
    for (const auto *stage : {"duration", "pitch", "variance", "acoustic"}) {
        const auto one = singerPhonemes(phonemeTable(singer.packagePath / "inferences" / stage));
        if (one.empty()) {
            continue; // a model that declares no table constrains nothing
        }
        std::vector<std::string> outside;
        for (const auto &phoneme : derived) {
            if (std::find(one.begin(), one.end(), phoneme) == one.end()) {
                outside.push_back(phoneme);
            }
        }
        expect(outside.empty(), std::string("the derived table is a subset of the ") + stage
                                    + " model, whose table is not the source of these: "
                                    + join(outside));
    }
    // Rule pin: a table key is `<language>/<phoneme>`, and the prefix is not part of a phoneme. A
    // keyed token matches no phoneme of any language and would measure a coverage of zero, so the
    // pin takes a keyed entry of this package and requires its bare token in the derived table.
    {
        const auto keys = tableKeys(phonemeTable(singer.packagePath / "inferences/acoustic"));
        const auto keyed = std::find_if(keys.begin(), keys.end(), [](const std::string &key) {
            return key.find('/') != std::string::npos;
        });
        expect(keyed != keys.end(), "the phoneme table of a model is keyed by language");
        std::vector<std::string> keyedTokens;
        for (const auto &phoneme : derived) {
            if (phoneme.find('/') != std::string::npos) {
                keyedTokens.push_back(phoneme);
            }
        }
        expect(keyedTokens.empty(), "the derived table holds no keyed token"
                                        + (keyedTokens.empty()
                                               ? std::string()
                                               : ": " + join(keyedTokens)));
        if (keyed != keys.end()) {
            const auto bare = barePhoneme(*keyed);
            expect(std::find(derived.begin(), derived.end(), bare) != derived.end(),
                   "the derived table holds " + bare + ", the token of the key " + *keyed);
        }
    }

    wolf::LinguistSession session(unit->unit());
    session.refresh();
    const wolf::SingerRef reference{
        srt::ContribLocator(singer.packageId, "singer", singer.contributionId),
        singer.packageVersion,
    };
    // The catalog supplies the session, exactly as the engine does after every refresh. A key that
    // the session does not hold is discarded with a warning, and the coverage then stays unknown.
    feedSingerPhonemes(session, singers);
    session.setReservedMarkers(markers);

    const auto *entry = session.catalog()->find(reference);
    expect(entry != nullptr, "the session finds the singer");
    if (entry != nullptr) {
        for (const auto &language : entry->languages) {
            const auto status = session.probe(reference, language.handle);
            std::cout << "  " << language.handle << "/" << language.binding.scheme << "  "
                      << readinessName(status.readiness);
            if (!status.reason.empty()) {
                std::cout << " (" << status.reason << ")";
            }
            std::cout << ", declares " << language.phonemes.size() << " phoneme(s)"
                      << (language.openSet ? " and an open set" : "") << ", coverage "
                      << coverageName(status.coverageKind) << " "
                      << static_cast<int>(status.coverage * 100 + 0.5) << "%\n";
            if (!status.missingPhonemes.empty()) {
                std::cout << "         unsingable phonemes: " << join(status.missingPhonemes)
                          << "\n";
            }
            expect(status.readiness != wolf::Readiness::Unavailable,
                   language.handle + " has a usable route");
            // The domain contract excludes reserved markers from the declared inventory: a marker
            // is resolved before grapheme-to-phoneme conversion and is never a phoneme that a host
            // must support. A marker left in the inventory makes every host that measures singer
            // coverage report a nonexistent gap; the `um` entry of the club voicebank caused such
            // a gap before the converter implemented this rule.
            std::vector<std::string> declaredMarkers;
            for (const auto &marker : markers) {
                if (std::find(language.phonemes.begin(), language.phonemes.end(), marker)
                    != language.phonemes.end()) {
                    declaredMarkers.push_back(marker);
                }
            }
            expect(declaredMarkers.empty(),
                   language.handle + " declares no reserved marker as a phoneme"
                       + (declaredMarkers.empty() ? "" : ", found " + join(declaredMarkers)));
            // The silence of such a phoneme is described in the file header. After the exclusion of
            // reserved phonemes, every remaining missing phoneme is a declaration of the voicebank
            // that the models cannot sing.
            if (!status.missingPhonemes.empty()) {
                finding(language.handle + " declares " + join(status.missingPhonemes)
                        + ", which no model of this singer contains: the chain can produce these "
                          "phonemes, and the models produce no sound for them");
            }
        }
    }

    std::cout << "\n== conversions, end to end ==\n";
    LanguageBridge bridge(unit->unit());
    bridge.refresh();
    // The catalog supplies the bridge through the production entry point, exactly as the engine does
    // after every refresh: the bridge wraps the same session, and a table that the session does not
    // hold is discarded with a warning instead of being measured.
    feedSingerPhonemes(bridge, singers);
    bridge.setReservedMarkers(markers);

    const std::vector<Probe> probes = {
        {"cmn", {{"\xe4\xbd\xa0", {}, {}, {}}, {"\xe5\xa5\xbd", {}, {}, {}}}},
        {"eng", {{"hello", {}, {}, {}}, {"world", {}, {}, {}}}},
        {"jpn", {{"\xe3\x81\x93", {}, {}, {}}, {"\xe3\x82\x93", {}, {}, {}}}},
    };

    // The layer limit of every probed language, read from the language module before a conversion,
    // which is the order the editor uses now (GetPhonemeNameTask). A composition without an onset
    // member stops at Phonemes, and a request for a deeper layer returns the shallower result
    // without an error, so the audit reports the limit beside the conversions below: the limit is
    // what decides how far those conversions can go.
    const auto depthName = [](LanguageBridge::Depth depth) -> const char * {
        switch (depth) {
            case LanguageBridge::Depth::Pronunciation:
                return "Pronunciation";
            case LanguageBridge::Depth::Phonemes:
                return "Phonemes";
            case LanguageBridge::Depth::Onsets:
                return "Onsets";
        }
        return "?";
    };
    std::cout << "\n== layer limits, read before converting ==\n";
    std::map<std::string, LanguageBridge::Depth> limits;
    for (const auto &probe : probes) {
        const auto limit = bridge.maxDepth(
            {singer.packageId, singer.contributionId, singer.packageVersion}, probe.language);
        std::cout << "  " << probe.language << " -> "
                  << (limit ? depthName(*limit) : std::string("(route not usable)")) << "\n";
        if (limit) {
            limits.emplace(probe.language, *limit);
        }
    }

    // The three depths on the same words (U-05). The wolf contract defines one cut depth instead of
    // a set of switches, and a composition may legitimately return a shallower result than
    // requested. The assertions below therefore check the shape guaranteed by each depth instead of
    // the shape of a switch: Pronunciation includes no phonemes, Phonemes includes no onsets, and
    // onsets, if present, align with the phonemes.
    struct DepthCase {
        LanguageBridge::Depth depth;
        const char *name;
    };
    const DepthCase depthCases[] = {
        {LanguageBridge::Depth::Pronunciation, "Pronunciation"},
        {LanguageBridge::Depth::Phonemes, "Phonemes"},
        {LanguageBridge::Depth::Onsets, "Onsets"},
    };
    std::cout << "\n== conversion depths ==\n";
    std::size_t counted = 0;
    std::size_t staged = 0;
    for (const auto &probe : probes) {
        std::vector<std::vector<LanguageBridge::Result>> perDepth(3);
        std::size_t depthIndex = 0;
        for (const auto &depthCase : depthCases) {
            auto converted = bridge.convert(
                {singer.packageId, singer.contributionId, singer.packageVersion}, probe.language,
                probe.words, depthCase.depth);
            expect(static_cast<bool>(converted), probe.language + " converts at " + depthCase.name
                                                     + ": " + why(converted));
            if (converted) {
                auto results = converted.take();
                expect(results.size() == probe.words.size(),
                       std::string(depthCase.name) + " returns a result for every word of "
                           + probe.language);
                for (std::size_t i = 0; i < results.size(); ++i) {
                    const auto &result = results[i];
                    const std::string where =
                        probe.language + " " + probe.words[i].lyric + " at " + depthCase.name;
                    expect(result.error.empty(), where + " converted");
                    expect(!result.pronunciation.empty(), where + " has a pronunciation");
                    if (depthCase.depth == LanguageBridge::Depth::Pronunciation) {
                        expect(result.phonemes.empty(), where + " stops before phonemes");
                        expect(result.onsets.empty(), where + " stops before onsets");
                    } else if (depthCase.depth == LanguageBridge::Depth::Phonemes) {
                        expect(result.onsets.empty(), where + " stops before onsets");
                    } else {
                        // The contract permits a shallower result but not an unaligned result.
                        expect(result.onsets.empty()
                                   || result.onsets.size() == result.phonemes.size(),
                               where + " has one onset per phoneme or no onsets");
                        // A composition that reaches no onset layer marks no onset, but it does
                        // return one flag per phoneme with all of them false, so the presence of the
                        // list says nothing: only a set flag contradicts the limit read above. A
                        // language whose limit is unknown, because its route is not usable, is left
                        // unchecked rather than treated as a composition that reaches no onset.
                        const auto reached = limits.find(probe.language);
                        if (reached != limits.end()
                            && reached->second == LanguageBridge::Depth::Phonemes
                            && std::any_of(result.onsets.begin(), result.onsets.end(),
                                           [](bool onset) { return onset; })) {
                            finding(probe.language
                                    + " marks an onset although its composition reaches no onset"
                                      " layer");
                        }
                    }
                    if (depthCase.depth == LanguageBridge::Depth::Pronunciation) {
                        ++counted;
                        if (result.stage != PronunciationStage::Unspecified) {
                            ++staged;
                        }
                    }
                }
                perDepth[depthIndex] = results;
            }
            ++depthIndex;
        }
        // A deeper conversion must not change the result of a shallower depth. The contract does
        // not guarantee this, so a change is reported as a finding instead of a failure.
        if (perDepth[0].size() == perDepth[1].size() && !perDepth[0].empty()) {
            for (std::size_t i = 0; i < perDepth[0].size(); ++i) {
                if (perDepth[0][i].pronunciation != perDepth[1][i].pronunciation) {
                    finding(probe.language + " word " + std::to_string(i)
                            + " changes pronunciation between depths");
                }
                if (perDepth[1][i].phonemes != perDepth[2][i].phonemes) {
                    finding(probe.language + " word " + std::to_string(i)
                            + " changes phonemes between Phonemes and Onsets");
                }
            }
        }
    }

    // The stage says how the language module produced a reading: a dictionary entry, a model, a rule
    // or the last resort. It is diagnostic, so only its propagation is asserted: the bridge carries
    // it out of the conversion instead of discarding it, and a module that reports nothing stays
    // Unspecified.
    expect(staged > 0,
           "the conversion reports the stage that produced its reading: " + std::to_string(staged) +
               " of " + std::to_string(counted) + " words");

    const std::set<std::string> known(derived.begin(), derived.end());
    for (const auto &probe : probes) {
        expect(bridge.canConvert({singer.packageId, singer.contributionId, singer.packageVersion},
                                 probe.language),
               probe.language + " is convertible");
        auto converted = bridge.convert({singer.packageId, singer.contributionId, singer.packageVersion},
                                    probe.language,
                                        probe.words, LanguageBridge::Depth::Onsets);
        expect(static_cast<bool>(converted),
               probe.language + " converts: " + why(converted));
        if (!converted) {
            continue;
        }
        const auto results = converted.take();
        expect(results.size() == probe.words.size(),
               probe.language + " returns a result for every word");
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto &result = results[i];
            std::cout << "  " << probe.language << " " << probe.words[i].lyric << " -> "
                      << (result.error.empty() ? result.pronunciation : "!" + result.error)
                      << " [" << join(result.phonemes) << "]";
            if (!result.onsets.empty()) {
                std::cout << " onsets";
                for (std::size_t j = 0; j < result.onsets.size(); ++j) {
                    std::cout << (result.onsets[j] ? " ^" : " .");
                }
            }
            std::cout << "\n";
            expect(result.error.empty(),
                   probe.language + " word " + std::to_string(i) + " converted");
            expect(!result.phonemes.empty(),
                   probe.language + " word " + std::to_string(i) + " reached phonemes");
            expect(result.onsets.size() == result.phonemes.size(),
                   probe.language + " word " + std::to_string(i) + " has an onset per phoneme");
            // Final link: every phoneme the chain produces must be one the models contain (see the
            // file header).
            std::vector<std::string> unsingable;
            for (const auto &phoneme : result.phonemes) {
                if (known.find(phoneme) == known.end()) {
                    unsingable.push_back(phoneme);
                }
            }
            expect(unsingable.empty(),
                   probe.language + " word " + std::to_string(i)
                       + " produced only phonemes contained in the models"
                       + (unsingable.empty() ? "" : ": " + join(unsingable)));
        }
    }

    // A marker bypasses grapheme-to-phoneme conversion (the rule above), so it is returned
    // unchanged for every language, regardless of whether a dictionary contains it. The check uses
    // the bridge instead of the session because the editor calls the bridge. The interception
    // applies only to markers registered by the host through setReservedMarkers(), which is the
    // reason the setter exists.
    std::cout << "\n== reserved markers (bypass grapheme-to-phoneme) ==\n";
    for (const auto &marker : markers) {
        auto answered = bridge.convert({singer.packageId, singer.contributionId, singer.packageVersion},
                                    "cmn",
                                       {{marker, {}, {}, {}}},
                                       LanguageBridge::Depth::Onsets);
        expect(static_cast<bool>(answered), marker + " is converted: " + why(answered));
        if (!answered) {
            continue;
        }
        const auto results = answered.take();
        const bool copied = results.size() == 1 && results.front().error.empty()
                            && results.front().pronunciation == marker
                            && results.front().phonemes == std::vector<std::string>{marker}
                            && results.front().onsets == std::vector<bool>{true};
        std::cout << "  " << marker << " -> "
                  << (results.empty() ? std::string("(nothing)") : results.front().pronunciation)
                  << (copied ? "  (copied through)" : "  (NOT copied)") << "\n";
        expect(copied, marker + " is returned unchanged as one phoneme with one onset");
    }

    // Counterpart of the reserved phoneme declaration: a singer that reserves a phoneme missing
    // from its models must not load. The check uses this voicebank instead of a fixture, because
    // it verifies that the loader check covers real models: four separate tables with hundreds of
    // entries each.
    //
    // The package is mirrored with its models linked instead of copied, and then doctored. The
    // copy receives a different id, so the unit does not hold two packages with the same id.
    std::cout << "\n== rejection of a reserved phoneme missing from the models ==\n";
    {
        const auto staging = fs::temp_directory_path() / "lite-audit-doctored";
        std::error_code ec;
        fs::remove_all(staging, ec);
        const auto doctored = staging / voicebank.filename();
        bool ready = mirror(voicebank, doctored);
        ready = ready && doctor(doctored / "desc.json", "\"id\": \"", "\"id\": \"doctored-");
        // A token that no phoneme table can contain, so the rejection cannot result from a
        // coincidental match.
        ready = ready && doctor(anyCharacterConfig(doctored),
                                "\"reservedPhonemes\": [",
                                "\"reservedPhonemes\": [\n      \"zz-not-a-phoneme\",");
        expect(ready, "the doctored copy is prepared");
        if (ready) {
            auto refused = unit->unit().openPackage(doctored, srt::SynthUnit::Load);
            expect(!refused, "the package must not load");
            if (!refused) {
                const auto reason = refused.error().toString();
                std::cout << "  refused: " << reason << "\n";
                expect(reason.find("zz-not-a-phoneme") != std::string::npos,
                       "the rejection must name the token: " + reason);
                for (const auto *model : {"duration", "pitch", "variance", "acoustic"}) {
                    expect(reason.find(model) != std::string::npos,
                           std::string("the rejection must name ") + model
                               + " as lacking the token: " + reason);
                }
            } else {
                // The package opened despite the doctoring; release it before the unit.
                refused.take().reset();
            }
        }
        fs::remove_all(staging, ec);
    }

    std::cout << "\n"
              << (failures == 0 ? "audit clean" : std::to_string(failures) + " failure(s)");
    if (findings != 0) {
        std::cout << ", " << findings << " finding(s) about the voicebank itself";
    }
    std::cout << "\n";

    // The session and the bridge hold executives that reference the packages, so they must be
    // destroyed first. They are declared after `opened` for this reason. The wrong order causes a
    // crash in unrelated code; the facade exists to enforce this order.
    return failures == 0 ? 0 : 1;
}
