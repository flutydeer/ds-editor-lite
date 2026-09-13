// The voicebank layer on the main line, against a real package.
//
// This is the piece the migration assessment was least sure of: the refactor line handed the
// editor a capability report, and the main line has no such thing, so what a singer can do has to
// be worked out from its imports and the exports of what they reach. Getting it wrong is quiet --
// a voicebank would simply appear to be missing a stage -- so it is written apart from the engine
// and checked here, against the package the converter produces from the editor's own fixture.
//
// It deliberately does not touch the rest of the editor, which does not build against the main
// line yet.

#include "SingerPipeline.h"
#include "SynthrtBootstrap.h"
#include "VoicebankCatalog.h"

#include <cstdlib>
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

    fs::path pluginRoot() {
        return fs::path(TEST_PLUGIN_ROOT);
    }

}

int main() {
    const fs::path packages(TEST_PACKAGE_DIR);
    // The 2.3 fixture this is converted from is produced during a CI build and is not in the
    // tree, so the directory exists and is empty on a developer machine. Both cases skip: an
    // empty directory is "no fixture here", not "the fixture contributes nothing".
    if (!fs::is_directory(packages) || fs::is_empty(packages)) {
        std::cerr << "no converted package at " << packages << ", skipping\n";
        return 77;
    }

    // Everything a unit needs before a package can be opened, in one call.
    auto booted = lite::synthrt::Bootstrap::create(pluginRoot(), {packages},
                                                   fs::path(TEST_ONNXRUNTIME_DIR),
                                                   lite::synthrt::Backend::Cpu, -1);
    expect(static_cast<bool>(booted),
           "the unit should boot: " + (booted ? std::string() : booted.error().toString()));
    if (!booted) {
        return 1;
    }
    auto bootstrap = booted.take();
    auto &unit = bootstrap->unit();
    const bool driverReady = bootstrap->hasDriver();
    expect(driverReady, "the ONNX driver should have been found and registered");

    // Every category the editor uses must exist on the unit, and two of them exist only because
    // something linked the library that registers them. This is a real check rather than a
    // formality: dropping --no-as-needed makes the linker discard otter, whose symbols nothing
    // here names, and the analysis category goes with it. Linking through otter::otter carries
    // that option, which is why the target exists rather than a bare library name.
    for (const auto *category : {"inference", "singer", "linguist", "analysis"}) {
        expect(unit.category(category) != nullptr,
               std::string("the unit must carry the ") + category + " category");
    }

    std::vector<srt::PackageHandle> opened;
    std::vector<lite::synthrt::PackageProblem> problems;
    const auto singers = lite::synthrt::scan(unit, {packages}, opened, problems);

    for (const auto &problem : problems) {
        std::cerr << "  could not open " << problem.path << ": " << problem.reason << "\n";
    }
    expect(problems.empty(), "the converted package should open");
    expect(singers.size() == 1, "the fixture contributes one singer, got "
                                    + std::to_string(singers.size()));
    if (singers.empty()) {
        return 1;
    }

    const auto &entry = singers.front();
    expect(entry.packageId == "ci-fixture", "the package id, got " + entry.packageId);
    expect(entry.contributionId == "fixture",
           "the contribution id, got " + entry.contributionId);
    expect(!entry.packagePath.empty(), "a singer knows where its package is");

    // What the fixture's models actually export. None of this is stated by the singer: it is read
    // through the singer's imports, which is the whole point.
    const auto &can = entry.capabilities;
    expect(can.duration, "the fixture imports a duration model");
    expect(can.pitch, "and a pitch model");
    expect(can.variance, "and a variance model");
    expect(can.acoustic, "and an acoustic model");
    expect(can.vocoder, "and a vocoder");
    expect(can.complete(), "so it can synthesise");

    expect(can.speakers.size() == 2, "two speakers, got " + std::to_string(can.speakers.size()));
    if (can.speakers.size() == 2) {
        expect(can.speakers[0].id == "clear" && can.speakers[1].id == "soft",
               "named clear and soft, got " + can.speakers[0].id + " and " + can.speakers[1].id);
    }

    // The acoustic model consumes these; the variance model predicts them. A voicebank whose
    // variance model predicted nothing would show up here as an empty set rather than as a
    // failure, which is why the two are read separately.
    for (const auto &wanted : {"breathiness", "tension", "voicing"}) {
        expect(can.varianceControls.count(wanted) == 1,
               std::string("the acoustic model consumes ") + wanted);
        expect(can.variancePredictions.count(wanted) == 1,
               std::string("the variance model predicts ") + wanted);
    }
    for (const auto &wanted : {"gender", "velocity"}) {
        expect(can.transitionControls.count(wanted) == 1,
               std::string("the acoustic model accepts ") + wanted);
    }

    // The converter was not told which linguist any language maps to, so the singer declares
    // none. That is the honest answer, and it has to read as "no languages" rather than as a
    // failure to parse.
    expect(can.languages.empty(), "an unbound voicebank declares no languages");

    // The pipeline: what used to be a ModelSetHandle. Creating a stage opens its models, so this
    // is the first point that needs the driver and the first that touches the weights.
    if (driverReady) {
        auto *spec = opened.front().contribution("singer", "fixture");
        expect(spec != nullptr, "the singer declaration is reachable from its package");
        if (spec != nullptr) {
            auto built =
                lite::synthrt::SingerPipeline::create(opened.front(), *spec->as<srt::SingerSpec>());
            expect(static_cast<bool>(built),
                   "the pipeline should build: "
                       + (built ? std::string() : built.error().toString()));
            if (built) {
                auto pipeline = built.take();
                const auto stage = [&](const char *what, auto &&get) {
                    auto made = get();
                    expect(static_cast<bool>(made),
                           std::string(what) + " should be created: "
                               + (made ? std::string() : made.error().toString()));
                    // Asking twice must give the same executive back, or every phrase of a
                    // synthesis would open the models again.
                    if (made) {
                        auto again = get();
                        expect(again && again.take() == made.take(),
                               std::string(what) + " is created once and kept");
                    }
                };
                stage("duration", [&] { return pipeline->duration(); });
                stage("pitch", [&] { return pipeline->pitch(); });
                stage("variance", [&] { return pipeline->variance(); });
                stage("acoustic", [&] { return pipeline->acoustic(); });
                stage("vocoder", [&] { return pipeline->vocoder(); });
            }
        }
    } else {
        std::cerr << "no ONNX driver, skipping the pipeline\n";
    }

    for (auto &package : opened) {
        package.reset();
    }
    return failures == 0 ? 0 : 1;
}
