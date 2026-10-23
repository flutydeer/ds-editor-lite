#include <QScopeGuard>
// Test analyzer discovery, package-root selection, and rejected declarations.

#include "SynthrtEngine.h"
#include "SynthrtBootstrap.h"

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

    // Pitch declaration with a placeholder model; listing must not open its weights.
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

    // supportsKnownNotes requires durationToBoundary; omit that model to test rejection.
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
        // Keep the declared model path present without supplying executable weights.
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

int runAnalyzerPackages() {

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

    writePackage(scanRoot, "otter-rmvpe", "otter/rmvpe", "f0", f0Declaration("RMVPE"));
    // Versionless references must select the highest loaded version regardless of scan order.
    writePackage(scanRoot, "otter-rmvpe-next", "otter/rmvpe", "f0", f0Declaration("RMVPE2"),
                 "0.2.0.0");
    writePackage(scanRoot, "otter-game", "otter/game", "note", noteDeclaration("GAME", true));
    // Distinct packages with the same interface must both be listed.
    writePackage(scanRoot, "example-note2", "example/note2", "note", noteDeclaration("GAME2", true));
    // Exports supportsKnownNotes without a durationToBoundary model, so the provider rejects it.
    writePackage(scanRoot, "example-broken", "example/broken", "note", noteDeclaration("BROKEN", false));
    // Dependency roots are not implicit scan roots.
    writePackage(dependencyRoot, "example-dep", "example/dep", "f0", f0Declaration("DEP"));
    // Permission failure or invalid metadata must produce a package problem on every platform.
    const fs::path locked = scanRoot / "locked";
    writePackage(scanRoot, "locked", "xb/locked", "note", noteDeclaration("LOCKED", false));
    fs::permissions(locked, fs::perms::none);
    const auto unlock = [&locked] {
        std::error_code ignored;
        fs::permissions(locked, fs::perms::owner_all, ignored);
    };

    auto *engine = SingletonRegistry::create<SynthrtEngine>(nullptr);
    const auto engineCleanup = qScopeGuard([&] { SingletonRegistry::destroy(engine); });
    expect(engine != nullptr, "the engine should be registered");
    if (engine == nullptr) {
        return 1;
    }

    // Use the deployed plugin tree and ONNX Runtime.
    const fs::path pluginRoot = SynthrtEngine::defaultPluginRoot();
    const fs::path runtimePath = SynthrtEngine::defaultRuntimePath();
    if (!fs::is_directory(runtimePath)) {
        // Building DsEditorLite supplies the runtime and plugins required by this test.
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

    // Reuse preserves unchanged identities; explicit reload always republishes.
    const auto generation = engine->catalogGeneration();
    {
        const auto reused = engine->refreshVoicebanks(
            {scanRoot}, nullptr, SynthrtEngine::RescanMode::ReuseLoaded);
        expect(bool(reused), "a rescan that reuses the loaded packages should succeed");
        expect(engine->catalogGeneration() == generation,
               "a rescan of an unchanged set should keep the catalog generation");
    }

    {
        std::vector<lite::synthrt::PackageProblem> problems;
        const auto rescanned = engine->refreshVoicebanks({scanRoot}, &problems);
        expect(bool(rescanned), "a rescan over an unreadable entry should succeed");
        const bool reported =
            std::any_of(problems.begin(), problems.end(), [&locked](const auto &problem) {
                return problem.path == locked;
            });
        // Invalid metadata also fails under root or on platforms that ignore permissions.
        expect(reported, "an entry whose declaration cannot be read should be reported as a problem");
    }
    expect(engine->catalogGeneration() > generation,
           "a reload must invalidate pipelines even when package identities are unchanged");

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

    expect(pitch == std::vector<std::string>{"otter/rmvpe:inference/f0"},
           "the scanned pitch package should be listed alone, got " + join(pitch));
    expect(notes == std::vector<std::string>({"example/note2:inference/note", "otter/game:inference/note"}),
           "both scanned note packages should be listed, got " + join(notes));

    expect(std::find(notes.begin(), notes.end(), "example/broken:inference/note") == notes.end(),
           "a package with a rejected declaration should not be listed");
    expect(std::find(pitch.begin(), pitch.end(), "example/dep:inference/f0") == pitch.end(),
           "a package in the dependency root should not be listed as an analyzer");
    expect(engine->analyzers().size() == 3,
           "three analyzers in total expected, got " + std::to_string(engine->analyzers().size()));

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

    {
        auto lease = engine->createAnalyzer(QStringLiteral("otter/rmvpe:inference/f0"));
        if (lease) {
            expect(lease->package.version() == stdc::VersionNumber(0, 2, 0, 0),
                   "the analyzer should come from the highest loaded version");
        } else {
            // Placeholder weights may fail to open; the error path must identify the selected
            // version.
            const auto why = lease.error().toString();
            expect(why.find("otter-rmvpe-next") != std::string::npos,
                   "the analyzer should come from the highest loaded version: " + why);
        }
    }

    // Validation must read a candidate with an already-loaded identity, without replacing the
    // installed package or trusting its previously loaded declarations.
    const auto installed = scanRoot / "otter-rmvpe-next";
    expect(bool(engine->validatePackage(installed)), "the installed package should validate");
    writePackage(fixture, "candidate", "otter/rmvpe", "f0", "invalid JSON", "0.2.0.0");
    expect(!engine->validatePackage(fixture / "candidate"),
           "a broken same-identity candidate should fail validation");
    const auto candidateRoot = fixture / "outside-live-search-paths";
    writePackage(candidateRoot, "dependent", "example/dependent", "f0", f0Declaration("CANDIDATE"));
    auto dependentManifest = descJson("example/dependent", "f0", "0.1.0.0");
    dependentManifest.insert(dependentManifest.find("\"contributions\""),
                             "\"dependencies\": [{\"id\": \"example/sibling\", "
                             "\"version\": \"0.1.0.0\"}],\n    ");
    writeFile(candidateRoot / "dependent/desc.json", dependentManifest);
    expect(!engine->validatePackage(candidateRoot / "dependent"),
           "a candidate with a missing sibling dependency should fail validation");
    writePackage(candidateRoot, "sibling", "example/sibling", "f0", f0Declaration("SIBLING"));
    expect(bool(engine->validatePackage(candidateRoot / "dependent")),
           "validation should resolve dependencies beside an unconfigured candidate");
    writeFile(installed / "inferences/f0/inference.json", "invalid JSON");
    expect(!engine->validatePackage(installed), "validation should reread changed installed files");
    expect(referencesOf(engine->analyzers(lite::synthrt::f0Contract())) == pitch,
           "validation should leave the live catalog unchanged");

    // Changing contract at the highest version hides the obsolete lower-version F0 entry.
    writeFile(installed / "inferences/f0/inference.json", noteDeclaration("NOTE3", true));
    expect(bool(engine->refreshVoicebanks({scanRoot})), "the changed contract should reload");
    expect(engine->analyzers(lite::synthrt::f0Contract()).empty(),
           "filtering must not expose the older version's contract");
    const auto changedNotes = referencesOf(engine->analyzers(lite::synthrt::noteContract()));
    expect(std::find(changedNotes.begin(), changedNotes.end(), "otter/rmvpe:inference/f0") !=
               changedNotes.end(), "the latest contract should be offered");

    expect(engine->inferenceReady(), "the deployed ONNX driver should be ready");
    const auto metadataPlugins = fixture / "metadata-plugins";
    for (const auto &category : lite::synthrt::pluginCategories(pluginRoot)) {
        for (const auto &directory : category.directories) {
            if (!fs::is_directory(directory))
                continue;
            const auto destination = metadataPlugins / directory.lexically_relative(pluginRoot);
            fs::create_directories(destination.parent_path());
#ifdef Q_OS_WIN
            fs::copy(directory, destination, fs::copy_options::recursive);
#else
            // Preserve the deployed dylib dependency paths and manifest resource symlinks.
            fs::create_directory_symlink(directory, destination);
#endif
        }
    }
    SingletonRegistry::destroy(engine);
    engine = SingletonRegistry::create<SynthrtEngine>(nullptr);
    expect(engine->initialize(voicebanks, dependencies, QStringLiteral("CPU"), -1,
                              metadataPlugins, runtimePath),
           "metadata should initialize without the driver plugin");
    expect(engine->initialized() && !engine->inferenceReady(),
           "a metadata-only engine must not report inference readiness");
    expect(engine->analyzers().size() == 3, "missing driver should not hide valid packages");
    // Windows cannot remove the copied plugin DLLs while this unit still holds them loaded.
    engine->shutdown();

    unlock();
    if (failures == 0) {
        fs::remove_all(fixture);
    } else {
        std::cerr << "fixtures left in " << fixture << "\n";
    }
    std::cout << (failures == 0 ? "ok\n" : "failures\n");
    return failures == 0 ? 0 : 1;
}
