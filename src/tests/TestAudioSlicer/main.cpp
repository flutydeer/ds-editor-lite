// Covers the two pieces of extraction that belong to the host rather than to an analyzer:
// cutting audio at its silences, and handing over samples at the rate an analyzer asked for.
//
// The second of those rests on an assumption about talcs — that the rate given to
// AudioFormatInputSource::open() is the rate read() answers in, so the source resamples — which is
// checked here against a file written at another rate rather than taken on trust.

#include "Modules/Extractors/AnalysisAudio.h"
#include "Modules/Extractors/AudioSlicer.h"

#include <cmath>
#include <vector>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <TalcsFormat/AudioFormatIO.h>

namespace {

    int failures = 0;

    void expect(bool condition, const QString &what) {
        if (!condition) {
            QTextStream(stderr) << "FAIL: " << what << Qt::endl;
            ++failures;
        }
    }

    constexpr int SOURCE_RATE = 44100;

    /// Alternating passages and silences: \a pattern gives each stretch in seconds, starting with
    /// sound.
    std::vector<float> build(const std::vector<double> &pattern, int sampleRate = SOURCE_RATE) {
        std::vector<float> samples;
        bool sounding = true;
        double phase = 0;
        for (const auto seconds : pattern) {
            const auto count = static_cast<std::size_t>(seconds * sampleRate);
            for (std::size_t i = 0; i < count; ++i) {
                // A tone rather than a constant: the slicer measures RMS, and a DC level would
                // pass the threshold while telling nothing about whether windowing works.
                samples.push_back(sounding ? 0.5f * static_cast<float>(std::sin(phase)) : 0.0f);
                phase += 2 * M_PI * 220.0 / sampleRate;
            }
            sounding = !sounding;
        }
        return samples;
    }

    double seconds(int64_t frames, int sampleRate = SOURCE_RATE) {
        return static_cast<double>(frames) / sampleRate;
    }

    void testCutsAtSilence() {
        Extractors::SlicingProfile profile;
        // Three passages of three seconds, parted by silences of one second — comfortably past the
        // 300 ms gap and the 2 s minimum the profile asks for.
        const auto samples = build({3.0, 1.0, 3.0, 1.0, 3.0});
        const auto spans = profile.slice(samples, SOURCE_RATE);

        expect(spans.size() == 3, QString("three passages, got %1").arg(spans.size()));
        if (spans.size() != 3) {
            return;
        }
        for (const auto &span : spans) {
            expect(span.length() > 0, "a span must hold something");
            expect(seconds(span.length()) < 4.5,
                   QString("a passage should be around three seconds, got %1")
                       .arg(seconds(span.length())));
        }
        expect(spans.front().begin < static_cast<int64_t>(0.5 * SOURCE_RATE),
               "the first passage starts near the beginning");
        expect(spans.back().end <= static_cast<int64_t>(samples.size()),
               "no span may reach past the audio");
    }

    void testKeepsShortAudioWhole() {
        Extractors::SlicingProfile profile;
        // Below the shortest piece the profile would cut out, so there is nothing to decide.
        const auto samples = build({1.0});
        const auto spans = profile.slice(samples, SOURCE_RATE);
        expect(spans.size() == 1, "short audio stays in one piece");
        if (!spans.empty()) {
            expect(spans.front().begin == 0 && spans.front().end ==
                                                   static_cast<int64_t>(samples.size()),
                   "the one piece covers everything");
        }
    }

    void testCapsSpansThatSilenceCannotCut() {
        Extractors::SlicingProfile profile;
        // Sung without a breath: the silence pass has nothing to cut at, and the note model
        // refuses a span it cannot encode rather than degrading, so the cap has to do it.
        const auto samples = build({25.0});
        const auto uncapped = profile.slice(samples, SOURCE_RATE);
        expect(uncapped.size() == 1, "with no silence there is nothing to cut at");

        const auto spans = profile.slice(samples, SOURCE_RATE, 10.0);
        expect(spans.size() == 3, QString("twenty five seconds capped at ten gives three, got %1")
                                      .arg(spans.size()));
        int64_t covered = 0;
        int64_t previousEnd = -1;
        for (const auto &span : spans) {
            expect(seconds(span.length()) <= 10.0 + 1e-6,
                   QString("no piece may exceed the cap, got %1").arg(seconds(span.length())));
            if (previousEnd >= 0) {
                expect(span.begin == previousEnd, "the pieces must not gap or overlap");
            }
            previousEnd = span.end;
            covered += span.length();
        }
        expect(covered == uncapped.front().length(), "capping must not lose or repeat audio");

        // Equal pieces rather than full ones and a remainder: a sliver at the end is worse input
        // than three even pieces.
        for (const auto &span : spans) {
            expect(seconds(span.length()) > 5.0,
                   QString("pieces should be even, got one of %1").arg(seconds(span.length())));
        }
    }

    void testResamplesToTheRateTheAnalyzerAsked() {
        QTemporaryDir directory;
        expect(directory.isValid(), "a temporary directory is needed");
        if (!directory.isValid()) {
            return;
        }
        const auto path = QDir(directory.path()).filePath("tone.wav");

        constexpr double DURATION = 2.0;
        {
            const auto samples = build({DURATION});
            QFile file(path);
            expect(file.open(QIODevice::WriteOnly), "the fixture file must be writable");
            talcs::AudioFormatIO io(&file);
            io.setFormat(talcs::AudioFormatIO::WAV | talcs::AudioFormatIO::PCM_16);
            io.setChannelCount(1);
            io.setSampleRate(SOURCE_RATE);
            expect(io.open(talcs::AudioFormatIO::Write), "the fixture file must be openable");
            io.write(samples.data(), static_cast<qint64>(samples.size()));
            io.close();
        }

        constexpr int WANTED_RATE = 16000;
        QFile file(path);
        expect(file.open(QIODevice::ReadOnly), "the fixture file must be readable");
        talcs::AudioFormatIO io(&file);
        QString error;
        auto prepared = Extractors::prepareAudio(&io, 0, DURATION * 1000.0, WANTED_RATE,
                                                 [] { return false; }, error);
        expect(prepared.has_value(), "preparing the audio should succeed: " + error);
        if (!prepared) {
            return;
        }

        expect(prepared->sampleRate == WANTED_RATE, "the samples come back at the rate asked for");
        // This is the assumption under test: the file is at 44.1 kHz and the analyzer wanted
        // 16 kHz, so the count must follow the rate that was asked for, not the file's.
        const auto expectedFrames = static_cast<std::size_t>(DURATION * WANTED_RATE);
        const auto got = prepared->samples.size();
        expect(got > expectedFrames * 9 / 10 && got < expectedFrames * 11 / 10,
               QString("two seconds at 16 kHz is about %1 samples, got %2")
                   .arg(expectedFrames)
                   .arg(got));
        expect(std::abs(prepared->startMs) < 1e-6, "a request from zero starts at zero");

        // The region is honoured, and the reported start moves with it, because an analyzer
        // anchors its answer to that number.
        QString laterError;
        auto later = Extractors::prepareAudio(&io, 500.0, 1500.0, WANTED_RATE,
                                              [] { return false; }, laterError);
        expect(later.has_value(), "a middle region should also work: " + laterError);
        if (later) {
            expect(std::abs(later->startMs - 500.0) < 1.0,
                   QString("the region starts where it was asked to, got %1").arg(later->startMs));
            const auto half = static_cast<std::size_t>(1.0 * WANTED_RATE);
            expect(later->samples.size() > half * 9 / 10 && later->samples.size() < half * 11 / 10,
                   QString("one second, got %1 samples").arg(later->samples.size()));
        }

        // Cancellation reports nothing and says nothing, which is how the caller tells it apart
        // from a failure.
        QString cancelledError;
        auto cancelled = Extractors::prepareAudio(&io, 0, DURATION * 1000.0, WANTED_RATE,
                                                  [] { return true; }, cancelledError);
        expect(!cancelled.has_value(), "a cancelled read produces nothing");
        expect(cancelledError.isEmpty(), "a cancelled read is not an error");
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    testCutsAtSilence();
    testKeepsShortAudioWhole();
    testCapsSpansThatSilenceCannotCut();
    testResamplesToTheRateTheAnalyzerAsked();

    return failures == 0 ? 0 : 1;
}
