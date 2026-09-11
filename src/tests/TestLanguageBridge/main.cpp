// The language layer, end to end, on a real voicebank bound to a real language package.
//
// The refactor line answered the editor's language questions from a LanguageService, a G2P Manager
// singleton, a LanguageRoute and an S2P resource. wolf answers all of it from one session, so the
// bridge is mostly a change of vocabulary -- but a change of vocabulary is exactly where a word can
// quietly come out meaning something else, so this converts an actual word rather than checking
// that the types line up.
//
// The language is wolf's Mandarin fixture, which converts through a dictionary and a pinyin engine
// rather than a model, so the whole path between the singer's language map and a pronunciation runs
// without the trained weights being present.

#include "LanguageBridge.h"
#include "SynthrtBootstrap.h"
#include "VoicebankCatalog.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

    int failures = 0;

    void expect(bool condition, const std::string &what) {
        if (!condition) {
            std::cerr << "FAIL: " << what << "\n";
            ++failures;
        }
    }

}

int main() {
    const fs::path packages(TEST_PACKAGE_DIR);
    if (!fs::is_directory(packages)) {
        std::cerr << "no packages at " << packages << ", skipping\n";
        return 0;
    }

    auto booted = lite::synthrt::Bootstrap::create(fs::path(TEST_PLUGIN_ROOT), {packages},
                                                  fs::path(TEST_ONNXRUNTIME_DIR),
                                                  lite::synthrt::Backend::Cpu, -1);
    expect(static_cast<bool>(booted),
           "the unit should boot: " + (booted ? std::string() : booted.error().toString()));
    if (!booted) {
        return 1;
    }
    auto bootstrap = booted.take();

    std::vector<srt::PackageHandle> opened;
    std::vector<lite::synthrt::PackageProblem> problems;
    const auto singers = lite::synthrt::scan(bootstrap->unit(), {packages}, opened, problems);
    for (const auto &problem : problems) {
        std::cerr << "  could not open " << problem.path << ": " << problem.reason << "\n";
    }
    expect(problems.empty(), "both packages should open");
    expect(singers.size() == 1, "one singer, got " + std::to_string(singers.size()));
    if (singers.empty()) {
        return 1;
    }

    // The capability read is the same one the catalog does, and now it has a language to find:
    // the singer's map names a role, the role reaches a linguist in another package, and the
    // dependency that makes that reference resolvable was written by the converter.
    const auto &entry = singers.front();
    expect(entry.capabilities.languages.size() == 1,
           "the bound language should be visible, got "
               + std::to_string(entry.capabilities.languages.size()));
    if (!entry.capabilities.languages.empty()) {
        expect(entry.capabilities.languages.front() == "cmn",
               "under the handle the singer used, got " + entry.capabilities.languages.front());
    }
    expect(entry.capabilities.defaultLanguage == "cmn", "and named as the default");

    lite::synthrt::LanguageBridge bridge(bootstrap->unit());
    bridge.refresh();

    const auto languages = bridge.languagesOf(entry.packageId, entry.contributionId);
    expect(languages.size() == 1 && languages.front() == "cmn",
           "the bridge lists the same language the catalog does");
    expect(bridge.canConvert(entry.packageId, entry.contributionId, "cmn"),
           "and says it can be converted");
    expect(!bridge.canConvert(entry.packageId, entry.contributionId, "jpn"),
           "and says a language the singer does not declare cannot");

    // The conversion itself. Passthrough hands the lyric back as its pronunciation, which is
    // enough to prove the whole path ran: the singer's map, the import, the linguist, the G2P
    // stage, and the answer coming back word by word in order.
    std::vector<lite::synthrt::LanguageBridge::Word> words;
    // Characters the fixture's dictionary covers. It holds four, so a word outside them would
    // fall through to the chain's fallback and come back unchanged -- which looks like a passing
    // conversion and is not one.
    words.push_back({"\xe4\xb8\xad", {}, {}, {}});   // zhong
    words.push_back({"\xe5\x9b\xbd", {}, {}, {}});   // guo
    // A word the user pinned must come back as the user wrote it, whatever the language thinks.
    words.push_back({"anything", std::string("pinned"), {}, {}});

    auto converted = bridge.convert(entry.packageId, entry.contributionId, "cmn", words,
                                    lite::synthrt::LanguageBridge::Depth::Pronunciation);
    expect(static_cast<bool>(converted),
           "the conversion should run: "
               + (converted ? std::string() : converted.error().toString()));
    if (converted) {
        const auto results = converted.take();
        expect(results.size() == words.size(),
               "a batch answers word for word, got " + std::to_string(results.size()));
        if (results.size() == words.size()) {
            for (std::size_t i = 0; i < results.size(); ++i) {
                expect(results[i].error.empty(),
                       "word " + std::to_string(i) + " should convert: " + results[i].error);
            }
            expect(results[0].pronunciation == "zhong",
                   "the first character converts, got " + results[0].pronunciation);
            expect(results[1].pronunciation == "guo",
                   "and the second, got " + results[1].pronunciation);
            expect(results[2].pronunciation == "pinned",
                   "and what the user pinned is not overwritten, got "
                       + results[2].pronunciation);
        }
    }

    // A language the singer does not declare is refused as a whole rather than per word: there is
    // no route, so there is nothing to answer word by word about.
    auto missing = bridge.convert(entry.packageId, entry.contributionId, "jpn", words);
    expect(!missing, "a language the singer does not declare has no route");

    for (auto &package : opened) {
        package.reset();
    }
    return failures == 0 ? 0 : 1;
}
