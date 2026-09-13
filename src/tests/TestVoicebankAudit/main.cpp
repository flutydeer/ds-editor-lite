// A real voicebank and the whole chain, each checking the other.
//
// Every other test in this set uses a fixture built to pass it. This one points the finished chain
// at a voicebank nobody wrote for it -- a published 2.3 package, converted -- and reports what the
// two say about each other: which stages open, which languages route, and how much of what each
// language declares the singer can actually sing.
//
// The audit runs in both directions on purpose. The chain checking the voicebank is the ordinary
// direction and catches a bad conversion. The voicebank checking the chain is the one that finds
// the interesting faults: a phoneme inventory the chain routes to but the models have never heard
// of is a route that loads cleanly and then sings nothing.

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

    /// Something true of this voicebank that the chain cannot fix and must not hide.
    ///
    /// Kept apart from a failure on purpose. A failure means the two sides disagree about
    /// something the tooling decides, and someone has to change code. A finding means the
    /// voicebank says something about itself that does not hold -- surfacing that is the whole
    /// reason for pointing the chain at a real voicebank, and failing on it would only teach
    /// people to stop running this.
    void finding(const std::string &what) {
        std::cout << "  NOTE  " << what << "\n";
        ++findings;
    }

    /// Why an Expected failed, safely: error() on a successful one reads the wrong union member.
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

    /// The phonemes the singer's acoustic model knows, read the way the editor has to read them.
    ///
    /// wolf does not open voicebank formats, so this is the host's job, and the host is the only
    /// side that knows the table is keyed `<language>/<phoneme>` with the markers left bare. What
    /// comes back is one list per singer rather than one per language, which is what the session
    /// takes: a phoneme is either in the model or it is not, whichever language asked for it.
    std::vector<std::string> singerPhonemes(const fs::path &table) {
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
        std::set<std::string> distinct;
        for (const auto &[key, value] : parsed.toObject()) {
            const auto slash = key.find('/');
            distinct.insert(slash == std::string::npos ? key : key.substr(slash + 1));
        }
        return {distinct.begin(), distinct.end()};
    }

    /// Mirrors a package, copying what is small and linking what is not.
    ///
    /// The models are hundreds of megabytes and are not what is under test; the declarations are
    /// bytes and are. Linking the first and copying the second gives a package that is real in
    /// every way that matters here, in the time it takes to walk a directory.
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
            } else {
                fs::create_symlink(entry.path(), target, ec);
            }
            if (ec) {
                return false;
            }
        }
        return !ec;
    }

    /// Replaces the first occurrence of \a what in a file. Returns false when it was not there,
    /// which matters: a doctoring that silently changed nothing would make the test pass by
    /// loading a package that was never broken.
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
    // A published voicebank is hundreds of megabytes and cannot live in the tree, so this test is
    // told where one is and skips when it is not. Skipping rather than failing is the right
    // answer: nothing here is broken when the machine has no voicebank on it.
    const fs::path voicebank(argc > 1 ? argv[1] : TEST_VOICEBANK_DIR);
    const fs::path languagePackages(argc > 2 ? argv[2] : TEST_LANGUAGE_PACKAGES);
    const fs::path pluginRoot(argc > 3 ? argv[3] : TEST_PLUGIN_ROOT);
    const fs::path runtimePath(argc > 4 ? argv[4] : TEST_ONNXRUNTIME_DIR);

    if (voicebank.empty() || !fs::is_regular_file(voicebank / "desc.json")) {
        std::cerr << "no converted voicebank at " << voicebank << ", skipping. Point "
                  << "LITE_AUDIT_VOICEBANK at one converted by scripts/convert-voicebank.py\n";
        return 0;
    }
    if (!fs::is_directory(languagePackages)) {
        std::cerr << "no language packages at " << languagePackages << ", skipping\n";
        return 0;
    }

    std::cout << "voicebank: " << voicebank << "\n";

    // Dependencies resolve through the package paths, which are not the directories a scan walks:
    // the voicebank's own parent is here too, because a package is looked up by id from here.
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
    expect(capabilities.complete(), "it can synthesise");
    expect(capabilities.duration && capabilities.pitch && capabilities.variance,
           "and imports all three optional stages");
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
    // Read from the singer rather than agreed out of band, which is the whole point of the key:
    // the loader has already refused the package if its models lacked any of these.
    const auto &markers = capabilities.reservedPhonemes;
    std::cout << "  reserved   " << join(markers, 20) << "\n";
    expect(!markers.empty(), "the singer declares the phonemes a lyric may name directly");
    expect(capabilities.speakers.size() == 5, "five speakers, as the singer declares");
    expect(capabilities.languages.size() == 3, "three languages reach a linguist");

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
    const auto phonemes = singerPhonemes(singer.packagePath / "inferences/acoustic/phonemes.json");
    expect(!phonemes.empty(), "the acoustic phoneme table is readable");
    std::cout << "  the models know " << phonemes.size() << " distinct phoneme(s)\n";

    wolf::LinguistSession session(unit->unit());
    session.refresh();
    const wolf::SingerRef reference{
        srt::ContribLocator(singer.packageId, "singer", singer.contributionId),
        singer.packageVersion,
    };
    session.setSingerPhonemes(reference, phonemes);
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
                std::cout << "         the singer cannot sing: " << join(status.missingPhonemes)
                          << "\n";
            }
            expect(status.readiness != wolf::Readiness::Unavailable,
                   language.handle + " has a usable route");
            // The domain contract keeps reserved markers out of the declared inventory: a marker
            // is answered before grapheme-to-phoneme and is never a phoneme a host must have.
            // Leaving one in makes every host measuring its singer report a gap where there is
            // none, which is what this voicebank's `um` did until the converter learned the rule.
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
            // A phoneme the language declares and no model has loads, routes and sings nothing,
            // with no layer in between able to notice. With the reserved ones out of the way,
            // anything left is the voicebank saying it can produce something it cannot sing.
            if (!status.missingPhonemes.empty()) {
                finding(language.handle + " declares " + join(status.missingPhonemes)
                        + ", which none of this singer's models has: the chain would produce it "
                          "and the models would sing nothing");
            }
        }
    }

    std::cout << "\n== conversions, end to end ==\n";
    LanguageBridge bridge(unit->unit());
    bridge.refresh();
    bridge.setSingerPhonemes({singer.packageId, singer.contributionId,
                              singer.packageVersion},
                             phonemes);
    bridge.setReservedMarkers(markers);

    const std::vector<Probe> probes = {
        {"cmn", {{"\xe4\xbd\xa0", {}, {}, {}}, {"\xe5\xa5\xbd", {}, {}, {}}}},
        {"eng", {{"hello", {}, {}, {}}, {"world", {}, {}, {}}}},
        {"jpn", {{"\xe3\x81\x93", {}, {}, {}}, {"\xe3\x82\x93", {}, {}, {}}}},
    };
    const std::set<std::string> known(phonemes.begin(), phonemes.end());
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
        expect(results.size() == probe.words.size(), probe.language + " answers every word");
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
            // The last link. A phoneme the chain produced that the models do not have would be
            // sung as nothing, and no earlier stage would have complained.
            std::vector<std::string> unsingable;
            for (const auto &phoneme : result.phonemes) {
                if (known.find(phoneme) == known.end()) {
                    unsingable.push_back(phoneme);
                }
            }
            expect(unsingable.empty(),
                   probe.language + " word " + std::to_string(i)
                       + " produced only phonemes these models have"
                       + (unsingable.empty() ? "" : ": " + join(unsingable)));
        }
    }

    // A marker is answered before grapheme-to-phoneme, so it comes back as itself whatever the
    // language is and whether or not any dictionary holds it. Checked through the bridge rather
    // than the session because it is the bridge the editor will call, and the interception only
    // happens for markers the host named -- which is the whole reason the setter exists.
    std::cout << "\n== reserved markers, which never reach grapheme-to-phoneme ==\n";
    for (const auto &marker : markers) {
        auto answered = bridge.convert({singer.packageId, singer.contributionId, singer.packageVersion},
                                    "cmn",
                                       {{marker, {}, {}, {}}},
                                       LanguageBridge::Depth::Onsets);
        expect(static_cast<bool>(answered), marker + " is answered: " + why(answered));
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
        expect(copied, marker + " comes back as itself, one phoneme, one onset");
    }

    // The other half of declaring them: a singer that reserves a phoneme its models do not have
    // must not load. Checked on this voicebank rather than a fixture, because what is being shown
    // is that the check reaches real models -- four separate tables, hundreds of entries each.
    //
    // The package is mirrored with its models linked rather than copied, then doctored. It gets a
    // different id so the unit is not asked to hold two packages claiming to be the same one.
    std::cout << "\n== a reserved phoneme the models lack refuses the package ==\n";
    {
        const auto staging = fs::temp_directory_path() / "lite-audit-doctored";
        std::error_code ec;
        fs::remove_all(staging, ec);
        const auto doctored = staging / voicebank.filename();
        bool ready = mirror(voicebank, doctored);
        ready = ready && doctor(doctored / "desc.json", "\"id\": \"", "\"id\": \"doctored-");
        // A token no phoneme table could hold, so the refusal cannot be a coincidence of spelling.
        ready = ready && doctor(doctored / "characters" / "yousa" / "config.json",
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
                       "and must say which token, said: " + reason);
                for (const auto *model : {"duration", "pitch", "variance", "acoustic"}) {
                    expect(reason.find(model) != std::string::npos,
                           std::string("and that ") + model + " lacks it, said: " + reason);
                }
            } else {
                // Opened despite the doctoring, so let go of it before the unit does.
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

    // The session and the bridge hold executives that point into the packages, so they have to go
    // first. They are declared after `opened` for exactly that reason -- getting the order wrong
    // is a crash somewhere else entirely, which is the rule the facade exists to own.
    return failures == 0 ? 0 : 1;
}
