// Which packages an analyser comes from, and which directory makes one exist.
//
// The editor asks the engine for analysers by interface and shows what comes back; it has no other
// way to know whether the machine can transcribe notes or draw a pitch curve. That makes the
// lookup itself worth a test, because both of its failure modes are silent: a package in a
// directory the scan does not walk is simply absent from the list, and a package that refuses its
// own declaration is absent too, which is only reassuring if a healthy package beside it still
// appears.
//
// The first of those was a real morning: the analyser packages had been installed next to the
// voicebanks, which is where a dependency is looked up and not where packages are scanned, and the
// editor showed an empty chooser with nothing in the log. So the fixtures below are deliberately
// arranged around that line -- a valid package in each of the two directories, and the assertion
// that only the scanned one is listed.

#include "SynthrtEngine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <QCoreApplication>

#include <lite/Core/SingletonRegistry.h>

#include <otter/Analysis/AnalysisExecutive.h>

namespace fs = std::filesystem;

namespace {

    int failures = 0;

    void expect(bool condition, const std::string &what) {
        if (!condition) {
            std::cerr << "FAIL: " << what << "\n";
            ++failures;
        }
    }

    void writeFile(const fs::path &path, const std::string &text) {
        fs::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        out << text;
    }

    std::string descJson(const std::string &id, const std::string &contributionId,
                         const std::string &version) {
        return "{\n"
               "    \"$version\": \"1.0\",\n"
               "    \"id\": \"" +
               id +
               "\",\n"
               "    \"version\": \"" +
               version +
               "\",\n"
               "    \"compatVersion\": \"0.1.0.0\",\n"
               "    \"runtimeLevel\": 1,\n"
               "    \"contributions\": {\n"
               "        \"inference\": [\n"
               "            {\n"
               "                \"id\": \"" +
               contributionId +
               "\",\n"
               "                \"path\": \"./inferences/" +
               contributionId +
               "/inference.json\"\n"
               "            }\n"
               "        ]\n"
               "    }\n"
               "}\n";
    }

    // The pitch declaration as the rmvpe package ships it: one rate, one hop, one channel, and a
    // model path the loader only resolves.
    std::string f0Declaration(const std::string &name) {
        return "{\n"
               "    \"interface\": \"" +
               lite::synthrt::f0Contract().toStdString() +
               "\",\n"
               "    \"level\": 1,\n"
               "    \"variant\": \"rmvpe\",\n"
               "    \"name\": \"" +
               name +
               "\",\n"
               "    \"exports\": {\n"
               "        \"sampleRate\": 16000,\n"
               "        \"channelCount\": 1,\n"
               "        \"interval\": 0.01,\n"
               "        \"maxSegmentDuration\": 60\n"
               "    },\n"
               "    \"configuration\": {\n"
               "        \"model\": \"../../placeholder.onnx\"\n"
               "    }\n"
               "}\n";
    }

    // The note declaration as the game package ships it. \a alignable is the one thing the two
    // callers of this differ in: a declaration that promises note texts needs the model that turns
    // a duration into a boundary, and promising it without that model is the refusal under test.
    std::string noteDeclaration(const std::string &name, bool alignable) {
        std::string configuration = "    \"configuration\": {\n"
                                    "        \"encoder\": \"../../placeholder.onnx\",\n"
                                    "        \"segmenter\": \"../../placeholder.onnx\",\n"
                                    "        \"estimator\": \"../../placeholder.onnx\",\n"
                                    "        \"boundaryToDuration\": \"../../placeholder.onnx\",\n" +
                                    std::string(alignable ? "        \"durationToBoundary\": \"../../placeholder.onnx\",\n" : "") +
                                    "        \"timestep\": 0.01,\n"
                                    "        \"languages\": {\n"
                                    "            \"eng\": 1,\n"
                                    "            \"jpn\": 2,\n"
                                    "            \"yue\": 3,\n"
                                    "            \"cmn\": 4\n"
                                    "        }\n"
                                    "    }\n";
        return "{\n"
               "    \"interface\": \"" +
               lite::synthrt::noteContract().toStdString() +
               "\",\n"
               "    \"level\": 1,\n"
               "    \"variant\": \"game\",\n"
               "    \"name\": \"" +
               name +
               "\",\n"
               "    \"exports\": {\n"
               "        \"sampleRate\": 44100,\n"
               "        \"channelCount\": 1,\n"
               "        \"maxSegmentDuration\": 60,\n"
               "        \"languages\": [\"cmn\", \"eng\", \"jpn\", \"yue\"],\n"
               "        \"defaultLanguage\": \"cmn\",\n"
               "        \"supportsKnownNotes\": true\n"
               "    },\n" +
               configuration + "}\n";
    }

    void writePackage(const fs::path &root, const std::string &directory, const std::string &id,
                      const std::string &contributionId, const std::string &declaration,
                      const std::string &version = "0.1.0.0") {
        const fs::path package = root / directory;
        writeFile(package / "desc.json", descJson(id, contributionId, version));
        writeFile(package / "inferences" / contributionId / "inference.json", declaration);
        // Nothing opens a model while a package is being listed, but the declaration names one, so
        // the shape on disk is complete.
        writeFile(package / "placeholder.onnx", "not a model\n");
    }

    std::vector<std::string> referencesOf(const std::vector<lite::synthrt::AnalyzerEntry> &entries) {
        std::vector<std::string> references;
        for (const auto &entry : entries) {
            references.push_back(entry.reference());
        }
        std::sort(references.begin(), references.end());
        return references;
    }

    std::string join(const std::vector<std::string> &values) {
        std::string joined;
        for (const auto &value : values) {
            joined += joined.empty() ? value : ", " + value;
        }
        return joined.empty() ? "(none)" : joined;
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    // The stored form of the user's analyser selection. A reference saved while analysers had a
    // dedicated category still identifies the same package and contribution.
    {
        using lite::synthrt::AnalyzerReference;
        const auto parsed = AnalyzerReference::parse("otter/game:inference/note");
        expect(parsed && parsed->packageId == "otter/game" && parsed->contributionId == "note",
               "a reference should split into its package and contribution");
        expect(AnalyzerReference{"otter/game", "note"}.toString() == "otter/game:inference/note",
               "a reference should format to the form that it parses from");
        expect(!AnalyzerReference::parse("otter/game:analysis/note"),
               "the older form should not parse as a current reference");
        expect(AnalyzerReference::upgrade("otter/game:analysis/note") ==
                   "otter/game:inference/note",
               "the older form should be upgraded to the same package and contribution");
        expect(AnalyzerReference::upgrade("otter/game:inference/note") ==
                   "otter/game:inference/note",
               "a current reference should be returned unchanged");
        expect(AnalyzerReference::upgrade("") == "", "an empty setting should remain empty");
        expect(!AnalyzerReference::parse("otter/game:inference/") &&
                   !AnalyzerReference::parse(":inference/x"),
               "a reference without a package or a contribution should be rejected");
    }

    const fs::path fixture =
        fs::temp_directory_path() / ("dsh-analyzer-fixture-" + std::to_string(QCoreApplication::applicationPid()));
    const fs::path scanRoot = fixture / "packages";
    const fs::path dependencyRoot = fixture / "dependencies";
    fs::remove_all(fixture);

    // Four packages where packages are looked for.
    writePackage(scanRoot, "otter-rmvpe", "otter/rmvpe", "f0", f0Declaration("RMVPE"));
    // A second version of the same package. A reference names no version, so the listing and
    // createAnalyzer() must both settle on the highest one, whatever order the scan finds them in.
    writePackage(scanRoot, "otter-rmvpe-next", "otter/rmvpe", "f0", f0Declaration("RMVPE2"),
                 "0.2.0.0");
    writePackage(scanRoot, "otter-game", "otter/game", "note", noteDeclaration("GAME", true));
    // A second note package that differs only in its name: it proves the listing is not deduplicated
    // by interface, which is what makes the third one's absence mean something.
    writePackage(scanRoot, "example-note2", "example/note2", "note", noteDeclaration("GAME2", true));
    // Promises note texts and names no model that can align them, so the provider refuses it.
    writePackage(scanRoot, "example-broken", "example/broken", "note", noteDeclaration("BROKEN", false));
    // One package of the same kind where a dependency is looked up, and never where packages are
    // scanned: this is the arrangement that once left the chooser empty.
    writePackage(dependencyRoot, "example-dep", "example/dep", "f0", f0Declaration("DEP"));
    // A directory the scan may not look into. A filesystem query that throws would end the
    // process on the initialization thread, so this must become a reported problem instead.
    const fs::path locked = scanRoot / "locked";
    fs::create_directories(locked);
    fs::permissions(locked, fs::perms::none);
    const auto unlock = [&locked] {
        std::error_code ignored;
        fs::permissions(locked, fs::perms::owner_all, ignored);
    };

    auto *engine = SingletonRegistry::create<SynthrtEngine>(nullptr);
    expect(engine != nullptr, "the engine should be registered");
    if (engine == nullptr) {
        return 1;
    }

    // Booted the way the editor boots: the plugin tree and the ONNX Runtime this build deployed
    // under the application directory. Naming the copy under vcpkg's installed tree instead does
    // not work here, because that one has no runtime beside the driver and the driver does not load
    // without it.
    const fs::path pluginRoot = SynthrtEngine::defaultPluginRoot();
    const fs::path runtimePath = SynthrtEngine::defaultRuntimePath();
    if (!fs::is_directory(runtimePath)) {
        std::cerr << "no ONNX Runtime deployed at " << runtimePath << ", skipping\n";
        unlock();
        fs::remove_all(fixture);
        return 77;
    }

    const QStringList voicebanks{QString::fromStdString(scanRoot.string())};
    const QStringList dependencies{QString::fromStdString(dependencyRoot.string())};
    expect(engine->initialize(voicebanks, dependencies, QStringLiteral("CPU"), -1, pluginRoot, runtimePath),
           "the engine should initialize");
    if (!engine->initialized()) {
        std::cerr << "the engine did not initialize, nothing below would mean anything\n";
        return 1;
    }

    // A rescan that finds the same packages does not republish the catalogue, in either mode.
    const auto generation = engine->catalogGeneration();
    {
        const auto reused = engine->refreshVoicebanks(
            {scanRoot}, nullptr, SynthrtEngine::RescanMode::ReuseLoaded);
        expect(bool(reused), "a rescan that reuses the loaded packages should succeed");
        expect(engine->catalogGeneration() == generation,
               "a rescan of an unchanged set should keep the catalogue generation");
    }

    // The unreadable directory is reported and does not hide the packages beside it.
    {
        std::vector<lite::synthrt::PackageProblem> problems;
        const auto rescanned = engine->refreshVoicebanks({scanRoot}, &problems);
        expect(bool(rescanned), "a rescan over an unreadable entry should succeed");
        const bool reported =
            std::any_of(problems.begin(), problems.end(), [&locked](const auto &problem) {
                return problem.path == locked;
            });
        // A process with the permission to read everything, such as one run as root, cannot
        // reproduce the condition, so only a denied read is required to be reported.
        std::error_code probe;
        (void) fs::is_regular_file(locked / "desc.json", probe);
        expect(!probe || reported, "an unreadable entry should be reported as a problem");
    }
    expect(engine->catalogGeneration() == generation,
           "a reload of an unchanged set should keep the catalogue generation");

    // A package that appears between two scans changes the set and republishes the catalogue.
    writePackage(scanRoot, "example-late", "example/late", "f0", f0Declaration("LATE"));
    expect(bool(engine->refreshVoicebanks({scanRoot})), "a rescan with a new package should succeed");
    expect(engine->catalogGeneration() != generation,
           "a rescan that finds a new package should change the catalogue generation");
    fs::remove_all(scanRoot / "example-late");
    expect(bool(engine->refreshVoicebanks({scanRoot})), "a rescan without the new package should succeed");

    const auto pitch = referencesOf(engine->analyzers(lite::synthrt::f0Contract()));
    const auto notes = referencesOf(engine->analyzers(lite::synthrt::noteContract()));

    std::cout << "  pitch analysers: " << join(pitch) << "\n";
    std::cout << "  note analysers:  " << join(notes) << "\n";

    // Scanned and listed, with the reference the editor stores.
    expect(pitch == std::vector<std::string>{"otter/rmvpe:inference/f0"},
           "the scanned pitch package should be listed alone, got " + join(pitch));
    expect(notes == std::vector<std::string>({"example/note2:inference/note", "otter/game:inference/note"}),
           "both scanned note packages should be listed, got " + join(notes));

    // The refusal is one package's, and one package's only.
    expect(std::find(notes.begin(), notes.end(), "example/broken:inference/note") == notes.end(),
           "a package that refuses its declaration should not be listed");
    // Same declaration, same interface, same directory -- only the other root.
    expect(std::find(pitch.begin(), pitch.end(), "example/dep:inference/f0") == pitch.end(),
           "a package in the dependency root should not be listed as an analyser");
    expect(engine->analyzers().size() == 3,
           "three analysers in total, got " + std::to_string(engine->analyzers().size()));

    // The reference is built from the parts a chooser shows, so they are checked as well.
    const auto listed = engine->analyzers(lite::synthrt::f0Contract());
    if (listed.size() == 1) {
        expect(listed.front().packageId == "otter/rmvpe", "the package id should be the declaration's own");
        expect(listed.front().contributionId == "f0", "the contribution id should be the declaration's own");
        expect(listed.front().variant == "rmvpe", "the variant should come from the declaration");
        expect(listed.front().packageVersion == stdc::VersionNumber(0, 2, 0, 0),
               "the listing should name the highest loaded version of the package");
    }
    expect(listed.size() == 1, "two versions of one package should be listed once, got " +
                                   std::to_string(listed.size()));

    // A note analyser carries the languages its exports declare, and the language an execution
    // uses follows from them.
    for (const auto &entry : engine->analyzers(lite::synthrt::noteContract())) {
        if (entry.packageId != "otter/game") {
            continue;
        }
        expect(entry.languages == std::vector<std::string>({"cmn", "eng", "jpn", "yue"}),
               "the note analyser should list the languages of its exports");
        expect(entry.defaultLanguage == "cmn",
               "the note analyser should carry the default language of its exports");
        expect(entry.effectiveLanguage("") == "cmn",
               "a request without a language should use the declared default");
        expect(entry.effectiveLanguage("jpn") == "jpn", "a listed language should be used");
        expect(entry.effectiveLanguage("fra") == "cmn",
               "an unlisted language should be replaced by the declared default");
    }
    if (!listed.empty()) {
        expect(listed.front().languages.empty() && listed.front().effectiveLanguage("fra") == "fra",
               "a pitch analyser should distinguish no languages");
    }

    // createAnalyzer() resolves the reference to the version the listing shows.
    {
        auto lease = engine->createAnalyzer(QStringLiteral("otter/rmvpe:inference/f0"));
        if (lease) {
            expect(lease->package.version() == stdc::VersionNumber(0, 2, 0, 0),
                   "the analyser should come from the highest loaded version");
        } else {
            // The fixture model is a placeholder, so creating the executive may fail when it opens
            // the model. The error then names the model of the version that was chosen.
            const auto why = lease.error().toString();
            expect(why.find("otter-rmvpe-next") != std::string::npos,
                   "the analyser should come from the highest loaded version: " + why);
        }
    }

    unlock();
    if (failures == 0) {
        fs::remove_all(fixture);
    } else {
        std::cerr << "fixtures left in " << fixture << "\n";
    }
    std::cout << (failures == 0 ? "ok\n" : "failures\n");
    return failures == 0 ? 0 : 1;
}
