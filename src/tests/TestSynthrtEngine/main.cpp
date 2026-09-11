// The engine facade, on the parts it now delegates to.
//
// Those parts are each checked on their own elsewhere. What is checked here is the facade's own
// job, which is the part that used to go wrong: the order things are built and torn down in, the
// lifetime of what it hands out, and the fact that a caller asking through the editor's own
// vocabulary -- a SingerIdentifier -- reaches the same answers as a caller using the parts
// directly.

#include "SynthrtEngine.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <QCoreApplication>

#include <lite/Core/SingletonRegistry.h>

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

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const fs::path packages(TEST_PACKAGE_DIR);
    if (!fs::is_directory(packages)) {
        std::cerr << "no packages at " << packages << ", skipping\n";
        return 0;
    }

    auto *engine = SingletonRegistry::create<SynthrtEngine>(nullptr);
    expect(engine != nullptr, "the engine should be registered");
    if (engine == nullptr) {
        return 1;
    }

    const QStringList voicebanks{QString::fromStdString(packages.string())};
    // The plugin root is named rather than derived from where this binary happens to sit.
    expect(engine->initialize(voicebanks, {}, QStringLiteral("CPU"), -1,
                              fs::path(TEST_PLUGIN_ROOT), fs::path(TEST_ONNXRUNTIME_DIR)),
           "the engine should initialize");
    expect(engine->initialized(), "and say so");
    expect(engine->initializationDone(), "and record that it was attempted");
    expect(engine->waitForInitialization(1000), "and not make a later caller wait");
    expect(engine->hasInferenceBackend(), "with an inference backend");

    const auto singers = engine->singers();
    expect(singers.size() == 1, "one singer, got " + std::to_string(singers.size()));
    if (singers.empty()) {
        return 1;
    }

    // The editor names a singer by its contribution id alone in a few places, so the facade has to
    // be able to turn that back into a full identifier.
    auto found = engine->findSinger(QStringLiteral("fixture"));
    expect(static_cast<bool>(found), "a singer should be findable by name");
    if (!found) {
        return 1;
    }
    const auto identifier = found.take();
    expect(identifier.packageId == QStringLiteral("ci-fixture"),
           "with its package, got " + identifier.packageId.toStdString());
    expect(!engine->packageDirectory(identifier).empty(), "and a directory on disk");

    auto described = engine->singer(identifier);
    expect(static_cast<bool>(described), "and be describable");
    if (described) {
        const auto entry = described.take();
        expect(entry.capabilities.complete(), "the fixture can synthesise");
        expect(entry.capabilities.speakers.size() == 2, "with two speakers");
    }

    // A pipeline is the engine's, not the caller's, and asking twice must not build it twice --
    // each build opens five models.
    auto first = engine->pipelineFor(identifier);
    expect(static_cast<bool>(first),
           "the pipeline should build: " + (first ? std::string() : first.error().toString()));
    auto second = engine->pipelineFor(identifier);
    expect(second && first && second.take() == first.take(),
           "and be the same one the second time");
    if (first) {
        auto acoustic = first.take()->acoustic();
        expect(static_cast<bool>(acoustic),
               "its acoustic stage should open: "
                   + (acoustic ? std::string() : acoustic.error().toString()));
    }

    // The language questions, asked the way the editor asks them.
    const auto languages = engine->languagesOf(identifier);
    expect(languages.size() == 1 && languages.first() == QStringLiteral("cmn"),
           "the singer's language should be listed");
    expect(engine->canConvert(identifier, QStringLiteral("cmn")), "and be convertible");
    expect(!engine->canConvert(identifier, QStringLiteral("jpn")),
           "and one it does not declare should not be");

    std::vector<lite::synthrt::LanguageBridge::Word> words;
    words.push_back({"\xe4\xb8\xad", {}, {}, {}}); // zhong
    auto converted = engine->convert(identifier, QStringLiteral("cmn"), words,
                                     lite::synthrt::LanguageBridge::Depth::Pronunciation);
    expect(static_cast<bool>(converted),
           "a conversion should run through the facade: "
               + (converted ? std::string() : converted.error().toString()));
    if (converted) {
        const auto results = converted.take();
        expect(results.size() == 1 && results.front().pronunciation == "zhong",
               "and give the same answer the bridge does");
    }

    // A refresh releases the packages a pipeline borrows from, so it has to release the pipelines
    // first. Getting that order wrong destroys a package while an executive still points into it,
    // which is the kind of thing that only shows up as a crash somewhere else entirely.
    auto rescanned = engine->refreshVoicebanks({packages});
    expect(static_cast<bool>(rescanned), "a refresh should work");
    if (rescanned) {
        expect(rescanned.take().size() == 1, "and find the singer again");
    }
    auto rebuilt = engine->pipelineFor(identifier);
    expect(static_cast<bool>(rebuilt), "and a pipeline can be built again afterwards");

    // Shutting down while everything is live is the ordering this facade exists to get right.
    engine->shutdown();
    expect(!engine->initialized(), "a shut down engine is not initialized");
    expect(engine->isAboutToQuit(), "and says it is going away");
    expect(engine->singers().empty(), "and holds nothing");

    SingletonRegistry::destroy(engine);
    return failures == 0 ? 0 : 1;
}
