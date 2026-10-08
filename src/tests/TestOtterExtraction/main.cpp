// Tests the two extraction tasks against real otter executives.
//
// The otter fixtures are synthetic models with deterministic output: fixture-rmvpe returns a pitch
// for every 10 ms frame, and fixture-note transcribes one note every 0.2 s with keys ascending from
// 60. This output makes the placement of the result on the project timeline checkable, which the
// unit tests cannot cover. The audio of a clip starts at the material origin, the visible region
// may be trimmed, and the result must be returned in clip-local ticks through the tempo map. A
// task with an error in any of these steps still produces plausible curves and notes; therefore
// the placement is asserted exactly.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QRunnable>

#include <sndfile.hh>

#include <otter/Analysis/AnalysisExecutive.h>

#include <lite/Core/SingletonRegistry.h>
#include <lite/SynthrtEngine/SynthrtEngine.h>

#include "Modules/Extractors/ExtractMidiTask.h"
#include "Modules/Extractors/ExtractPitchTask.h"
#include "Modules/Inference/Models/InferParamCurve.h"

namespace fs = std::filesystem;

namespace {

    int failures = 0;

    void expect(bool condition, const std::string &what) {
        std::cout << (condition ? "  ok   " : "  FAIL ") << what << "\n";
        if (!condition) {
            ++failures;
        }
    }

    fs::path fixtureRoot() {
        if (const char *value = std::getenv("LITE_OTTER_FIXTURES"); value && *value) {
            return fs::path(value);
        }
        return fs::path(TEST_OTTER_FIXTURES);
    }

    /// Writes a 220 Hz tone of \a seconds, or that tone followed by \a gapSeconds of silence and
    /// the tone again. A gap of zero writes the single tone the other cases read.
    bool writeTone(const fs::path &path, int sampleRate, double seconds, double gapSeconds = 0.0) {
        SndfileHandle file(path.string().c_str(), SFM_WRITE, SF_FORMAT_WAV | SF_FORMAT_FLOAT, 1,
                           sampleRate);
        if (file.error() != SF_ERR_NO_ERROR) {
            return false;
        }
        const auto total = gapSeconds > 0.0 ? 2.0 * seconds + gapSeconds : seconds;
        std::vector<float> samples(static_cast<std::size_t>(total * sampleRate));
        const auto firstEnd = static_cast<std::size_t>(seconds * sampleRate);
        const auto secondBegin = static_cast<std::size_t>((seconds + gapSeconds) * sampleRate);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const bool silent = gapSeconds > 0.0 && i >= firstEnd && i < secondBegin;
            samples[i] =
                silent ? 0.0f
                       : static_cast<float>(0.5 * std::sin(2.0 * M_PI * 220.0 * i / sampleRate));
        }
        return file.write(samples.data(), static_cast<sf_count_t>(samples.size())) ==
               static_cast<sf_count_t>(samples.size());
    }

    // Runs a task on the current thread, as a worker of the task manager does.
    void runTask(Task &task) {
        static_cast<QRunnable &>(task).run();
    }

    // An audio clip that starts at tick 480 (500 ms at 120 BPM) with its first 400 ms trimmed, so
    // the visible region starts at 900 ms and the material origin, the project time of the first
    // sample of the file, is 500 ms. The file is 2 s long and fully visible after the trim.
    constexpr int kClipStartTick = 480;
    constexpr double kMaterialOriginMs = 500;
    constexpr double kTrimMs = 400;
    constexpr double kFileSeconds = 2.0;

    ExtractTask::Input clipInput(const Timeline &timeline, const fs::path &audio,
                                 const QString &analyzer) {
        ExtractTask::Input input;
        input.audioPath = QString::fromStdString(audio.string());
        input.analyzer = analyzer;
        input.timeline = timeline;
        input.audioClipStartTick = kClipStartTick;
        input.singingClipStartTick = kClipStartTick;
        input.audioMaterialOriginMs = kMaterialOriginMs;
        input.audioVisibleStartMs = kMaterialOriginMs + kTrimMs;
        input.audioVisibleEndMs = kMaterialOriginMs + kFileSeconds * 1000.0;
        return input;
    }

    // An audio clip that covers the whole file and starts where the file does: the material origin
    // and the project time of the first sample are both zero, so no trim and no tempo map stand
    // between the samples and the analyzed region.
    ExtractTask::Input wholeInput(const Timeline &timeline, const fs::path &audio,
                                  const QString &analyzer, double seconds) {
        ExtractTask::Input input;
        input.audioPath = QString::fromStdString(audio.string());
        input.analyzer = analyzer;
        input.timeline = timeline;
        input.audioVisibleEndMs = seconds * 1000.0;
        return input;
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    const auto fixtures = fixtureRoot();
    if (fixtures.empty() || !fs::is_directory(fixtures / "fixture-rmvpe") ||
        !fs::is_directory(fixtures / "fixture-note")) {
        std::cerr << "no otter fixtures found (set LITE_OTTER_FIXTURES to the directory that "
                     "contains fixture-rmvpe and fixture-note); skipping\n";
        return 77;
    }
    const fs::path runtimePath = SynthrtEngine::defaultRuntimePath();
    if (!fs::is_directory(runtimePath)) {
        std::cerr << "no ONNX Runtime deployed at " << runtimePath
                  << ": build the DsEditorLite target first, which deploys the plugin tree and "
                     "the runtime next to the test executables; skipping\n";
        return 77;
    }

    // The fixtures are copied instead of scanned in place, so that the scan finds only these two
    // packages and no other output of the otter build.
    const fs::path scratch = fs::temp_directory_path() /
                             ("dsh-otter-extraction-" +
                              std::to_string(QCoreApplication::applicationPid()));
    fs::remove_all(scratch);
    const fs::path packages = scratch / "packages";
    fs::create_directories(packages);
    fs::copy(fixtures / "fixture-rmvpe", packages / "fixture-rmvpe", fs::copy_options::recursive);
    fs::copy(fixtures / "fixture-note", packages / "fixture-note", fs::copy_options::recursive);
    const fs::path audio = scratch / "tone.wav";
    if (!writeTone(audio, 44100, kFileSeconds)) {
        std::cerr << "cannot write the test audio\n";
        return 1;
    }

    auto *engine = SingletonRegistry::create<SynthrtEngine>(nullptr);
    std::vector<lite::synthrt::PackageProblem> problems;
    if (!engine->initialize({QString::fromStdString(packages.string())}, {},
                            QStringLiteral("CPU"), -1, SynthrtEngine::defaultPluginRoot(),
                            runtimePath)) {
        std::cerr << "the engine did not initialize\n";
        return 1;
    }
    engine->refreshVoicebanks({packages}, &problems);
    for (const auto &problem : problems) {
        std::cerr << "  package problem: " << problem.path << ": " << problem.reason << "\n";
    }
    const auto pitchAnalyzers = engine->analyzers(lite::synthrt::f0Contract());
    const auto noteAnalyzers = engine->analyzers(lite::synthrt::noteContract());
    if (pitchAnalyzers.empty() || noteAnalyzers.empty()) {
        // The fixtures come from an otter build, which may be a different revision from the otter
        // linked by this editor. A declaration that the linked otter cannot load is not an editor
        // defect.
        std::cerr << "the otter fixtures did not load with the linked otter; skipping\n";
        fs::remove_all(scratch);
        return 77;
    }
    const auto pitchReference = QString::fromStdString(pitchAnalyzers.front().reference());
    const auto noteReference = QString::fromStdString(noteAnalyzers.front().reference());

    const Timeline timeline({
        {0, 120.0}
    });
    const double visibleStartMs = kMaterialOriginMs + kTrimMs;

    // Pitch: one frame every 10 ms from the visible start, placed on the 5-tick grid of the
    // singing clip, which starts where the audio clip does.
    {
        std::cout << "pitch extraction\n";
        ExtractPitchTask task(clipInput(timeline, audio, pitchReference));
        runTask(task);
        expect(task.success(), "the pitch extraction succeeds: " +
                                   task.errorMessage().toStdString());
        expect(task.result.size() == 1, "one span yields one segment");
        if (!task.result.isEmpty()) {
            const auto &segment = task.result.front();
            const double firstTick = timeline.msToTick(visibleStartMs);
            const int expectedStart =
                kClipStartTick +
                static_cast<int>(std::ceil((firstTick - kClipStartTick) / kParamCurveStepTicks)) *
                    kParamCurveStepTicks;
            expect(segment.globalStartTick == expectedStart,
                   "the curve starts at the first grid point of the visible region: " +
                       std::to_string(segment.globalStartTick) + " vs " +
                       std::to_string(expectedStart));
            // The last frame sits one interval before the end of the audio.
            const double lastFrameMs =
                kMaterialOriginMs + kFileSeconds * 1000.0 - 10.0;
            const int lastLocal = static_cast<int>(std::floor(
                (timeline.msToTick(lastFrameMs) - kClipStartTick) / kParamCurveStepTicks)) *
                                  kParamCurveStepTicks;
            const auto expectedCount =
                (lastLocal - (expectedStart - kClipStartTick)) / kParamCurveStepTicks + 1;
            expect(std::abs(static_cast<long long>(segment.values.size()) -
                            static_cast<long long>(expectedCount)) <= 1,
                   "the curve ends at the last frame: " + std::to_string(segment.values.size()) +
                       " points vs " + std::to_string(expectedCount));
            const auto voiced = std::count_if(segment.values.begin(), segment.values.end(),
                                              [](double value) { return value > 0; });
            expect(voiced > 0, "the fixture reports voiced frames");
            // Every point is either a pitch returned by the fixture or the unvoiced marker: the
            // placement does not blend across a voicing boundary.
            double lowest = 1e9;
            for (const auto value : segment.values) {
                if (value > 0) {
                    lowest = std::min(lowest, value);
                }
            }
            const bool clean = std::all_of(segment.values.begin(), segment.values.end(),
                                           [lowest](double value) {
                                               return value == 0 || value >= lowest - 1e-9;
                                           }) &&
                               lowest > 20;
            expect(clean, "no point lies between the unvoiced marker and a sung pitch");
        }
    }

    // MIDI: the fixture estimator returns a chromatic run from middle C, one note per 0.2 s of the
    // analyzed audio, so the key of a note identifies its 0.2 s step. The presence cutoff, which
    // the editor leaves at its default, drops some notes without moving the others. The notes are
    // returned in ticks local to the audio clip, so the first step is at the trim, not at zero.
    {
        std::cout << "MIDI extraction\n";
        ExtractMidiTask task(clipInput(timeline, audio, noteReference));
        runTask(task);
        expect(task.success(), "the MIDI extraction succeeds: " + task.errorMessage().toStdString());
        expect(!task.result.empty(), "the fixture transcribes notes");
        const auto steps =
            static_cast<int>(std::lround((kFileSeconds * 1000.0 - kTrimMs) / 200.0));
        bool placed = true;
        for (const auto &note : task.result) {
            const int step = note.note - 60;
            const double startMs = visibleStartMs + static_cast<double>(step) * 200.0;
            const int expectedStart =
                static_cast<int>(std::lround(timeline.msToTick(startMs))) - kClipStartTick;
            const int expectedLength = static_cast<int>(std::lround(
                timeline.msToTick(startMs + 200.0) - timeline.msToTick(startMs)));
            if (step < 0 || step >= steps || std::abs(note.start - expectedStart) > 1 ||
                std::abs(note.duration - expectedLength) > 1) {
                placed = false;
                std::cout << "       note key " << note.note << " at " << note.start << "+"
                          << note.duration << ", expected " << expectedStart << "+"
                          << expectedLength << "\n";
            }
        }
        expect(placed, "every note is placed on its 0.2 s step in clip-local ticks");
        // The model reports a confidence per note, and the extraction carries it out instead of
        // dropping it. The field is diagnostic today; a later change can present it without
        // plumbing it again.
        const auto confident =
            std::count_if(task.result.begin(), task.result.end(),
                          [](const ExtractMidiNote &note) { return note.confidence > 0; });
        expect(confident == static_cast<long long>(task.result.size()),
               "every transcribed note carries the confidence the model reported: " +
                   std::to_string(confident) + " of " + std::to_string(task.result.size()));
        expect(!task.result.empty() && task.result.front().note == 60 &&
                   task.result.front().start ==
                       static_cast<int>(std::lround(timeline.msToTick(visibleStartMs))) -
                           kClipStartTick,
               "the first note starts at the trim, not at the start of the clip");
    }

    // A region the host has to cut: the analyzer is called once per passage, the result carries one
    // segment per passage, and the progress names the passage. Without the name, a run over one
    // long passage and a run over several short ones look the same to the user, because neither the
    // bar nor the result says how many calls the selection produced.
    {
        std::cout << "cut region\n";
        const auto gapped = scratch / "two-passages.wav";
        constexpr double kPassageSeconds = 2.2;
        constexpr double kGapSeconds = 0.4;
        if (!writeTone(gapped, 44100, kPassageSeconds, kGapSeconds)) {
            std::cerr << "cannot write the test audio\n";
            return 1;
        }

        ExtractPitchTask task(
            wholeInput(timeline, gapped, pitchReference, 2.0 * kPassageSeconds + kGapSeconds));
        std::vector<QString> messages;
        QObject::connect(
            &task, &Task::statusUpdated, &task,
            [&messages](const TaskStatus &status) { messages.push_back(status.message); },
            Qt::DirectConnection);
        runTask(task);

        for (const auto &message : messages) {
            std::cout << "       status: " << message.toStdString() << "\n";
        }
        const auto names = [&messages](const QString &text) {
            return std::any_of(messages.begin(), messages.end(), [&text](const QString &message) {
                return message.contains(text);
            });
        };
        expect(task.success(), "the extraction of a cut region succeeds: " +
                                   task.errorMessage().toStdString());
        expect(task.result.size() == 2,
               "two passages yield two segments: " + std::to_string(task.result.size()));
        expect(names(QStringLiteral("passage 1 of 2")),
               "the progress names the first passage of two");
        expect(names(QStringLiteral("passage 2 of 2")),
               "the progress names the second passage of two");
    }

    // Cancellation before the run: the task reports a termination, not a failure.
    {
        std::cout << "cancellation\n";
        ExtractMidiTask task(clipInput(timeline, audio, noteReference));
        task.terminate();
        runTask(task);
        expect(task.errorCode() == ExtractTask::ErrorCode::Terminated && task.result.empty(),
               "a task terminated before it runs reports Terminated");
    }

    // Cancellation during the analysis: the first progress report of the analyzer stops the task,
    // the analyzer returns AnalysisError::Cancelled, and the task reports a termination rather
    // than an analyzer failure.
    {
        ExtractMidiTask task(clipInput(timeline, audio, noteReference));
        bool analysing = false;
        QObject::connect(&task, &Task::statusUpdated, &task,
                         [&task, &analysing](const TaskStatus &status) {
                             if (status.message.startsWith(QStringLiteral("Running inference"))) {
                                 if (analysing) {
                                     task.terminate();
                                 }
                                 analysing = true;
                             }
                         },
                         Qt::DirectConnection);
        runTask(task);
        expect(task.errorCode() == ExtractTask::ErrorCode::Terminated && task.result.empty(),
               "a task stopped during the analysis reports Terminated, not ModelRunFailed: " +
                   task.errorMessage().toStdString());
    }

    SingletonRegistry::destroy(engine);
    if (failures == 0) {
        fs::remove_all(scratch);
    } else {
        std::cerr << "files left in " << scratch << "\n";
    }
    std::cout << (failures == 0 ? "ok\n" : "failures\n");
    return failures == 0 ? 0 : 1;
}
