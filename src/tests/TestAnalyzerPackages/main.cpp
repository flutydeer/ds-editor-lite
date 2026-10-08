// Tests the analyzer lookup: which packages provide analyzers, and which package root makes an
// analyzer available. The listing is the only indication of whether note transcription and pitch
// extraction are available, and both failure modes of the lookup are silent: a package outside the
// scan roots is absent, and one with a rejected declaration is likewise absent (only meaningful if
// a valid neighbour is still listed). The first mode has occurred in practice (packages in the
// dependency root, empty chooser, no log entry), which is why the fixtures cover both roots.

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

    // Returns the pitch declaration in the form shipped by the rmvpe package: one sample rate, one
    // hop, one channel, and a model path that the loader resolves without opening the model.
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

    // Returns the note declaration in the form shipped by the game package. \a alignable is the
    // only difference between the two callers: a declaration that exports supportsKnownNotes
    // requires the durationToBoundary model, and the rejection of such a declaration without that
    // model is under test.
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
        // Listing a package opens no model, but the declaration references a model file; the
        // placeholder keeps the package layout complete.
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

    // Stored form of the analyzer selection of the user. A reference saved while analyzers had a
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

    // Packages in the scan root: four package ids, one of them in two versions.
    writePackage(scanRoot, "otter-rmvpe", "otter/rmvpe", "f0", f0Declaration("RMVPE"));
    // A second version of the same package. A reference specifies no version; therefore the
    // listing and createAnalyzer() must both select the highest version, regardless of scan order.
    writePackage(scanRoot, "otter-rmvpe-next", "otter/rmvpe", "f0", f0Declaration("RMVPE2"),
                 "0.2.0.0");
    writePackage(scanRoot, "otter-game", "otter/game", "note", noteDeclaration("GAME", true));
    // A second note package that differs only in its name. It verifies that the listing is not
    // deduplicated by interface, which makes the absence of the third note package meaningful.
    writePackage(scanRoot, "example-note2", "example/note2", "note", noteDeclaration("GAME2", true));
    // Exports supportsKnownNotes without a durationToBoundary model, so the provider rejects it.
    writePackage(scanRoot, "example-broken", "example/broken", "note", noteDeclaration("BROKEN", false));
    // A package of the same kind in the dependency root, which is never scanned for packages. This
    // arrangement previously left the chooser empty.
    writePackage(dependencyRoot, "example-dep", "example/dep", "f0", f0Declaration("DEP"));
    // A directory that the scan cannot use. Two mechanisms cover the platforms: where fs::perms::none
    // is enforced, reading the declaration fails; where it is not enforced, such as on Windows, the
    // declaration is readable but the package is rejected. Either way the scan must report the entry
    // instead of throwing out of it (the scan runs on a worker thread, where an uncaught exception
    // terminates the process) or skipping it silently.
    const fs::path locked = scanRoot / "locked";
    writePackage(scanRoot, "locked", "xb/locked", "note", noteDeclaration("LOCKED", false));
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

    // Initialization as in the editor: the plugin tree and the ONNX Runtime that this build
    // deployed under the application directory. The copy in the vcpkg installed tree is unusable
    // here, because that tree has no runtime next to the driver and the driver does not load
    // without the runtime.
    const fs::path pluginRoot = SynthrtEngine::defaultPluginRoot();
    const fs::path runtimePath = SynthrtEngine::defaultRuntimePath();
    if (!fs::is_directory(runtimePath)) {
        // The post-build steps of the DsEditorLite target deploy the runtime and the plugin tree
        // next to the executables; this test does not. A tree in which only the tests were built
        // contains neither, and a CI job that builds only the tests skips this test on every run.
        // Building the application target first enables the test.
        std::cerr << "no ONNX Runtime deployed at " << runtimePath
                  << ": build the DsEditorLite target first, which deploys the plugin tree and "
                     "the runtime next to the test executables; skipping\n";
        unlock();
        fs::remove_all(fixture);
        return 77;
    }

    const QStringList voicebanks{QString::fromStdString(scanRoot.string())};
    const QStringList dependencies{QString::fromStdString(dependencyRoot.string())};
    expect(engine->initialize(voicebanks, dependencies, QStringLiteral("CPU"), -1, pluginRoot, runtimePath),
           "the engine should initialize");
    if (!engine->initialized()) {
        std::cerr << "the engine did not initialize; the remaining checks cannot run\n";
        return 1;
    }

    // A rescan that finds the same packages does not republish the catalog in either mode.
    const auto generation = engine->catalogGeneration();
    {
        const auto reused = engine->refreshVoicebanks(
            {scanRoot}, nullptr, SynthrtEngine::RescanMode::ReuseLoaded);
        expect(bool(reused), "a rescan that reuses the loaded packages should succeed");
        expect(engine->catalogGeneration() == generation,
               "a rescan of an unchanged set should keep the catalog generation");
    }

    // The unusable directory is reported and does not hide the packages beside it.
    {
        std::vector<lite::synthrt::PackageProblem> problems;
        const auto rescanned = engine->refreshVoicebanks({scanRoot}, &problems);
        expect(bool(rescanned), "a rescan over an unreadable entry should succeed");
        const bool reported =
            std::any_of(problems.begin(), problems.end(), [&locked](const auto &problem) {
                return problem.path == locked;
            });
        // Unconditional, unlike the permission-only setup this replaced: a process that may read
        // every file (such as one run as root) still cannot open a rejected package, so the entry
        // must be reported on every platform.
        expect(reported, "an entry whose declaration cannot be read should be reported as a problem");
    }
    expect(engine->catalogGeneration() == generation,
           "a reload of an unchanged set should keep the catalog generation");

    // A package that appears between two scans changes the set and republishes the catalogue.
    writePackage(scanRoot, "example-late", "example/late", "f0", f0Declaration("LATE"));
    expect(bool(engine->refreshVoicebanks({scanRoot})), "a rescan with a new package should succeed");
    expect(engine->catalogGeneration() != generation,
           "a rescan that finds a new package should change the catalog generation");
    fs::remove_all(scanRoot / "example-late");
    expect(bool(engine->refreshVoicebanks({scanRoot})), "a rescan without the new package should succeed");

    const auto pitch = referencesOf(engine->analyzers(lite::synthrt::f0Contract()));
    const auto notes = referencesOf(engine->analyzers(lite::synthrt::noteContract()));

    std::cout << "  pitch analyzers: " << join(pitch) << "\n";
    std::cout << "  note analyzers:  " << join(notes) << "\n";

    // Scanned packages are listed with the reference that the editor stores.
    expect(pitch == std::vector<std::string>{"otter/rmvpe:inference/f0"},
           "the scanned pitch package should be listed alone, got " + join(pitch));
    expect(notes == std::vector<std::string>({"example/note2:inference/note", "otter/game:inference/note"}),
           "both scanned note packages should be listed, got " + join(notes));

    // The rejection affects only the rejected package.
    expect(std::find(notes.begin(), notes.end(), "example/broken:inference/note") == notes.end(),
           "a package with a rejected declaration should not be listed");
    // Same declaration, interface and directory layout; only the package root differs.
    expect(std::find(pitch.begin(), pitch.end(), "example/dep:inference/f0") == pitch.end(),
           "a package in the dependency root should not be listed as an analyzer");
    expect(engine->analyzers().size() == 3,
           "three analyzers in total expected, got " + std::to_string(engine->analyzers().size()));

    // The reference is built from the fields shown in a chooser; these fields are checked as well.
    const auto listed = engine->analyzers(lite::synthrt::f0Contract());
    if (listed.size() == 1) {
        expect(listed.front().packageId == "otter/rmvpe", "the package id should match the declaration");
        expect(listed.front().contributionId == "f0", "the contribution id should match the declaration");
        expect(listed.front().variant == "rmvpe", "the variant should come from the declaration");
        expect(listed.front().packageVersion == stdc::VersionNumber(0, 2, 0, 0),
               "the listing should name the highest loaded version of the package");
    }
    expect(listed.size() == 1, "two versions of a package should be listed once, got " +
                                   std::to_string(listed.size()));

    // A note analyzer carries the languages declared in its exports, and the language of an
    // execution is derived from them.
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
               "a pitch analyzer should declare no languages");
    }

    // createAnalyzer() resolves the reference to the version the listing shows.
    {
        auto lease = engine->createAnalyzer(QStringLiteral("otter/rmvpe:inference/f0"));
        if (lease) {
            expect(lease->package.version() == stdc::VersionNumber(0, 2, 0, 0),
                   "the analyzer should come from the highest loaded version");
        } else {
            // The fixture model is a placeholder, so creating the executive may fail when the model
            // is opened. The error then contains the model path of the selected version.
            const auto why = lease.error().toString();
            expect(why.find("otter-rmvpe-next") != std::string::npos,
                   "the analyzer should come from the highest loaded version: " + why);
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
