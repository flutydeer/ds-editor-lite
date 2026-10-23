// Validate a configured spec 2.4 voicebank's declarations, language routes, and model coverage.

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
#include "../TestSupport/VoicebankFixture.h"

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

    /// Record voicebank-specific findings separately from failed host-contract assertions.
    void finding(const std::string &what) {
        std::cout << "  NOTE  " << what << "\n";
        ++findings;
    }

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

    /// Accept phonemes.json and model-specific *.phonemes.json tables.
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

    /// Read raw table keys, retaining language prefixes for normalization checks.
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

    /// Remove the first language/ prefix only.
    std::string barePhoneme(const std::string &key) {
        const auto slash = key.find('/');
        return slash == std::string::npos ? key : key.substr(slash + 1);
    }

    /// Read a model's phoneme inventory without language prefixes.
    std::vector<std::string> singerPhonemes(const fs::path &table) {
        std::set<std::string> distinct;
        for (const auto &key : tableKeys(table)) {
            distinct.insert(barePhoneme(key));
        }
        return {distinct.begin(), distinct.end()};
    }

    /// Independently read model-table intersections for comparison with the production catalog.
    std::vector<std::string> modelPhonemes(const fs::path &package) {
        std::set<std::string> result;
        bool started = false;
        for (const auto *stage : {"duration", "pitch", "variance", "acoustic"}) {
            const auto table = singerPhonemes(phonemeTable(package / "inferences" / stage));
            if (table.empty()) {
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

    /// Find a character declaration to modify in the package copy.
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

    srt::JsonValue readJson(const fs::path &path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cout << "  unreadable: " << path.string() << " (cannot open)\n";
            return srt::JsonValue::fromJson("null", true);
        }
        const std::string text((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
        auto value = srt::JsonValue::fromJson(text, true);
        if (!value.isObject() && !value.isArray()) {
            std::cout << "  unparsed: " << path.string() << "\n";
        }
        return value;
    }

    /// Read speaker metadata independently of the production catalog.
    struct SpeakerDeclaration {
        std::vector<std::string> ids;

        /// Absent acoustic speaker mappings use model names directly.
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

    /// Copy editable JSON declarations and link model files to avoid duplicating large weights.
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
                // Fall back to copying when symlink permissions or cross-volume paths prevent
                // linking.
                fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing, ec);
            }
            if (ec) {
                return false;
            }
        }
        return !ec;
    }

    /// Fail if the replacement anchor is absent, so an unchanged package cannot satisfy the test.
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

int runVoicebankAudit() {
    failures = 0;
    findings = 0;
    // Optional external voicebank configured through LITE_AUDIT_VOICEBANK.
    const fs::path voicebank(TEST_VOICEBANK_DIR);
    const fs::path languagePackages(TEST_LANGUAGE_PACKAGES);
    const fs::path pluginRoot(TEST_PLUGIN_ROOT);
    const fs::path runtimePath(TEST_ONNXRUNTIME_DIR);

    if (voicebank.empty() || !fs::is_regular_file(voicebank / "desc.json")) {
        std::cerr << "no spec 2.4 voicebank at " << voicebank << "; skipping. Set "
                  << "LITE_AUDIT_VOICEBANK to a published spec 2.4 voicebank\n";
        return 77;
    }
    if (!fs::is_directory(languagePackages)) {
        std::cerr << "no language packages at " << languagePackages << "; skipping\n";
        return 77;
    }

    std::cout << "voicebank: " << voicebank << "\n";

    // Include the voicebank's parent when resolving package dependencies.
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
    expect(capabilities.supportsEditorPipeline(),
           "the singer has every stage required by the editor");
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
    std::cout << "  frames     duration " << capabilities.durationFrameWidth << " s, pitch "
              << capabilities.pitchFrameWidth << " s, variance " << capabilities.varianceFrameWidth
              << " s, acoustic " << capabilities.acousticFrameWidth << " s; vocoder "
              << capabilities.vocoderSampleRate << " Hz\n";
    expect(capabilities.durationFrameWidth > 0 && capabilities.pitchFrameWidth > 0 &&
               capabilities.varianceFrameWidth > 0 && capabilities.acousticFrameWidth > 0,
           "every stage declares its frame width");
    expect(capabilities.vocoderSampleRate > 0, "the vocoder declares its sample rate");
    const auto &markers = capabilities.reservedPhonemes;
    std::cout << "  reserved   " << join(markers, 20) << "\n";
    expect(!markers.empty(),
           "the singer declaration lists the phonemes that a lyric may specify directly");
    {
        std::ifstream manifest(voicebank / "desc.json");
        const std::string text((std::istreambuf_iterator<char>(manifest)),
                               std::istreambuf_iterator<char>());
        std::size_t declaredLanguages = 0;
        for (auto at = text.find("wolf/lang-"); at != std::string::npos;
             at = text.find("wolf/lang-", at + 1)) {
            ++declaredLanguages;
        }
        // Required language dependencies must resolve; self-contained linguists may add languages.
        expect(capabilities.languages.size() >= declaredLanguages,
               "every language the package depends on reaches a linguist: declared "
                   + std::to_string(declaredLanguages) + ", reached "
                   + std::to_string(capabilities.languages.size()));
        if (capabilities.languages.size() > declaredLanguages) {
            std::cout << "  note   " << (capabilities.languages.size() - declaredLanguages)
                      << " of the reached language(s) come(s) from a linguist that carries its own "
                         "modules, the manifest names no wolf package for it\n";
        }
    }

    // Validate singer-side speaker IDs against the acoustic import mapping.
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
    if (declared.empty()) {
        std::cout << "  note   the declaration names no speaker, so the package is single-speaker "
                     "and the host is offered none\n";
        expect(offered.empty(),
               "a package that names no speaker is offered none either"
                   + (offered.empty() ? std::string() : ": offered " + join(offered)));
    } else {
        expect(offered == declared,
               "the speakers offered to a host are the declared speakers of the singer"
                   + (offered == declared ? std::string() : ": offered " + join(offered)));
    }
    for (const auto &speaker : capabilities.speakers) {
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
    const auto fromFiles = modelPhonemes(singer.packagePath);
    expect(!fromFiles.empty(), "the phoneme tables of the models are readable");
    std::cout << "  the models contain " << fromFiles.size() << " distinct phoneme(s)\n";

    expect(capabilities.singablePhonemes.has_value(), "the singer declares a phoneme table");
    const auto derived = capabilities.singablePhonemes.value_or(std::vector<std::string>{});
    expect(!derived.empty(),
           "the catalog derives a phoneme table for the singer, which is what a measured coverage "
           "requires");
    expect(derived == fromFiles,
           "the derived table is the intersection an independent read of the models yields"
               + (derived == fromFiles
                      ? std::string()
                      : ": derived " + std::to_string(derived.size()) + ", read "
                            + std::to_string(fromFiles.size())));
    // A phoneme missing from any model must be absent from the shared inventory.
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
    // Verify that catalog inventory tokens omit the language prefix.
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
            // Reserved markers must not appear in ordinary language phoneme inventories.
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
    feedSingerPhonemes(bridge, singers);
    bridge.setReservedMarkers(markers);

    const std::vector<Probe> probes = {
        {"cmn", {{"\xe4\xbd\xa0", {}, {}, {}}, {"\xe5\xa5\xbd", {}, {}, {}}}},
        {"eng", {{"hello", {}, {}, {}}, {"world", {}, {}, {}}}},
        {"jpn", {{"\xe3\x81\x93", {}, {}, {}}, {"\xe3\x82\x93", {}, {}, {}}}},
    };

    // Probe supported depth before testing conversion results.
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

    // Check result shape at each requested depth, allowing shallower results.
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
                        expect(result.onsets.empty()
                                   || result.onsets.size() == result.phonemes.size(),
                               where + " has one onset per phoneme or no onsets");
                        // An absent onset layer may return an aligned all-false mask, but no true
                        // onset flags.
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
        // Cross-depth differences are findings because the contract does not guarantee equality.
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

    // Verify propagation of diagnostic pronunciation stages, including Unspecified.
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

    // Reserved markers must pass through language conversion unchanged.
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

    // Copy and rename the package before injecting an unsupported reserved phoneme.
    std::cout << "\n== rejection of a reserved phoneme missing from the models ==\n";
    {
        const auto staging = fs::temp_directory_path() / "lite-audit-doctored";
        std::error_code ec;
        fs::remove_all(staging, ec);
        const auto doctored = staging / voicebank.filename();
        bool ready = mirror(voicebank, doctored);
        // Match the root package ID, not an indented dependency ID.
        ready = ready && doctor(doctored / "desc.json", "\n  \"id\": \"",
                                "\n  \"id\": \"doctored-");
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

    // Destroy language executives before releasing their package handles.
    return failures == 0 ? 0 : 1;
}

// Verify declared and absent default-lyric metadata without modifying the source package.
int runDefaultLyricDeclaration() {
    failures = 0;
    findings = 0;
    const fs::path voicebank(TestSupport::voicebankRoot().toStdString());
    const fs::path pluginRoot(TEST_PLUGIN_ROOT);
    const fs::path runtimePath(TEST_ONNXRUNTIME_DIR);

    if (!fs::is_regular_file(voicebank / "desc.json")) {
        std::cerr << "no voicebank fixture at " << voicebank << "\n";
        return 1;
    }

    // Include deployed language packages when resolving fixture dependencies.
    fs::path languagePackages = pluginRoot / "wolf" / "packages";
    if (!fs::is_directory(languagePackages)) {
        languagePackages = pluginRoot.parent_path() / "wolf" / "packages";
    }

    auto bootstrap = Bootstrap::create(pluginRoot, {languagePackages, voicebank.parent_path()},
                                       runtimePath, Backend::Cpu, -1);
    expect(static_cast<bool>(bootstrap), "the unit bootstraps: " + why(bootstrap));
    if (!bootstrap) {
        return 1;
    }
    auto unit = bootstrap.take();

    std::cout << "\n== default lyric declared by a linguist module ==\n";
    std::error_code ec;
    const auto staging = fs::temp_directory_path() / "lite-default-lyric";
    fs::remove_all(staging, ec);
    const auto doctored = staging / voicebank.filename();
    bool ready = fs::create_directories(staging, ec) || fs::is_directory(staging);
    ready = ready && mirror(voicebank, doctored);
    ready = ready && doctor(doctored / "desc.json", "\n  \"id\": \"", "\n  \"id\": \"default-lyric-");
    std::vector<fs::path> declarations;
    for (const auto &entry : fs::recursive_directory_iterator(doctored, ec)) {
        if (entry.path().filename() == "linguist.json") {
            declarations.push_back(entry.path());
        }
    }
    expect(!declarations.empty(), "the fixture declares at least one linguist module");
    for (const auto &declaration : declarations) {
        ready = ready && doctor(declaration, "\"language\": \"",
                                "\"defaultLyric\": \"la\",\n  \"language\": \"");
    }
    expect(ready, "the doctored copy is prepared");

    if (ready) {
        std::vector<srt::PackageHandle> opened;
        std::vector<PackageProblem> problems;
        const auto singers = scan(unit->unit(), {staging}, opened, problems);
        for (const auto &problem : problems) {
            std::cout << "  note  " << problem.path << ": " << problem.reason << "\n";
        }
        expect(!singers.empty(), "the doctored package loads and contributes a singer");
        if (!singers.empty()) {
            const auto &capabilities = singers.front().capabilities;
            expect(!capabilities.languages.empty(), "the doctored singer declares languages");
            for (const auto &language : capabilities.languages) {
                const auto lyric = capabilities.defaultLyrics.find(language);
                const bool reported =
                    lyric != capabilities.defaultLyrics.end() && lyric->second == "la";
                std::cout << "  " << language << " -> " << (reported ? "la" : "(nothing)") << "\n";
                expect(reported,
                       "language " + language +
                           " reports the default lyric its own module declares");
            }
        }
        // The handles reference the packages, so they are released before the unit.
        opened.clear();
    }
    fs::remove_all(staging, ec);

    {
        std::vector<srt::PackageHandle> opened;
        std::vector<PackageProblem> problems;
        const auto singers = scan(unit->unit(), {voicebank.parent_path()}, opened, problems);
        for (const auto &problem : problems) {
            std::cout << "  note  " << problem.path << ": " << problem.reason << "\n";
        }
        expect(!singers.empty(), "the fixture itself contributes a singer");
        if (!singers.empty()) {
            expect(singers.front().capabilities.defaultLyrics.empty(),
                   "a voicebank that declares no default lyric reports none");
        }
    }

    return failures == 0 ? 0 : 1;
}
