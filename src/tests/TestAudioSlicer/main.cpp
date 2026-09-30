// Tests the two extraction stages that the host implements instead of an analyzer: slicing audio
// at silences, and delivering samples at the sample rate that the analyzer requires.
//
// The second stage relies on an assumption about talcs: read() returns samples at the rate passed
// to AudioFormatInputSource::open(), so the source resamples. The test verifies this assumption
// against a file written at a different rate.

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

    /// Builds alternating tone passages and silences. \a pattern specifies the length of each
    /// segment in seconds, starting with a tone passage.
    std::vector<float> build(const std::vector<double> &pattern, int sampleRate = SOURCE_RATE) {
        std::vector<float> samples;
        bool sounding = true;
        double phase = 0;
        for (const auto seconds : pattern) {
            const auto count = static_cast<std::size_t>(seconds * sampleRate);
            for (std::size_t i = 0; i < count; ++i) {
                // A tone instead of a constant level: the slicer measures RMS, and a DC level
                // passes the threshold without exercising the windowing.
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
        // Three passages of three seconds separated by one-second silences, well above the 300 ms
        // minimum gap and the 2 s minimum slice length of the profile.
        const auto samples = build({3.0, 1.0, 3.0, 1.0, 3.0});
        const auto spans = profile.slice(samples, SOURCE_RATE);

        expect(spans.size() == 3, QString("three passages, got %1").arg(spans.size()));
        if (spans.size() != 3) {
            return;
        }
        for (const auto &span : spans) {
            expect(span.length() > 0, "a span must not be empty");
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
        // The audio is shorter than the minimum slice length of the profile, so no cut is made.
        const auto samples = build({1.0});
        const auto spans = profile.slice(samples, SOURCE_RATE);
        expect(spans.size() == 1, "short audio stays in one piece");
        if (!spans.empty()) {
            expect(spans.front().begin == 0 && spans.front().end ==
                                                   static_cast<int64_t>(samples.size()),
                   "the single piece covers the whole audio");
        }
    }

    void testCapsSpansThatSilenceCannotCut() {
        Extractors::SlicingProfile profile;
        // A passage without a breath: the silence pass finds no cut point, and the note model
        // rejects a span longer than its input limit instead of degrading. The length cap
        // therefore splits the span.
        const auto samples = build({25.0});
        const auto uncapped = profile.slice(samples, SOURCE_RATE);
        expect(uncapped.size() == 1, "audio without silence must not be cut");

        const auto spans = profile.slice(samples, SOURCE_RATE, 10.0);
        expect(spans.size() == 3, QString("twenty five seconds capped at ten gives three, got %1")
                                      .arg(spans.size()));
        int64_t covered = 0;
        int64_t previousEnd = -1;
        for (const auto &span : spans) {
            expect(seconds(span.length()) <= 10.0 + 1e-6,
                   QString("no piece may exceed the cap, got %1").arg(seconds(span.length())));
            if (previousEnd >= 0) {
                expect(span.begin == previousEnd, "the pieces must be contiguous");
            }
            previousEnd = span.end;
            covered += span.length();
        }
        expect(covered == uncapped.front().length(), "capping must not lose or repeat audio");

        // The cap produces equal pieces instead of full pieces and a remainder, because a short
        // remainder is worse analyzer input than three equal pieces.
        for (const auto &span : spans) {
            expect(seconds(span.length()) > 5.0,
                   QString("pieces should have equal length, got %1").arg(seconds(span.length())));
        }
    }

    void testResamplesToTheRateTheAnalyzerAsked() {
        QTemporaryDir directory;
        expect(directory.isValid(), "the temporary directory must be valid");
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

        expect(prepared->sampleRate == WANTED_RATE, "the samples must use the requested rate");
        // Assumption under test: the file is at 44.1 kHz and the analyzer requires 16 kHz, so the
        // sample count must follow the requested rate, not the file rate.
        const auto expectedFrames = static_cast<std::size_t>(DURATION * WANTED_RATE);
        const auto got = prepared->samples.size();
        expect(got > expectedFrames * 9 / 10 && got < expectedFrames * 11 / 10,
               QString("two seconds at 16 kHz is about %1 samples, got %2")
                   .arg(expectedFrames)
                   .arg(got));
        expect(std::abs(prepared->startMs) < 1e-6, "a region from zero must report a zero start");

        // The requested region is respected, and the reported start follows the region, because
        // an analyzer anchors its result to the reported start.
        QString laterError;
        auto later = Extractors::prepareAudio(&io, 500.0, 1500.0, WANTED_RATE,
                                              [] { return false; }, laterError);
        expect(later.has_value(), "a middle region should also work: " + laterError);
        if (later) {
            expect(std::abs(later->startMs - 500.0) < 1.0,
                   QString("the region must start as requested, got %1").arg(later->startMs));
            const auto half = static_cast<std::size_t>(1.0 * WANTED_RATE);
            expect(later->samples.size() > half * 9 / 10 && later->samples.size() < half * 11 / 10,
                   QString("one second, got %1 samples").arg(later->samples.size()));
        }

        // A cancelled read returns no samples and no error message; the empty error message
        // distinguishes cancellation from failure for the caller.
        QString cancelledError;
        auto cancelled = Extractors::prepareAudio(&io, 0, DURATION * 1000.0, WANTED_RATE,
                                                  [] { return true; }, cancelledError);
        expect(!cancelled.has_value(), "a cancelled read produces nothing");
        expect(cancelledError.isEmpty(), "a cancelled read is not an error");
    }

    /// Checks that prepareAudio() fails on a stream that was not opened. talcs rejects an unopened
    /// stream, and the rejection is indistinguishable from a missing file. Opening the stream is
    /// therefore the responsibility of the caller. The test keeps that requirement visible at the
    /// boundary where it is easily omitted; omitting it makes every extraction fail with a
    /// file-open error.
    void testRefusesAStreamThatWasNeverOpened() {
        QTemporaryDir directory;
        expect(directory.isValid(), "the temporary directory must be valid");
        if (!directory.isValid()) {
            return;
        }
        const auto path = QDir(directory.path()).filePath("tone.wav");
        {
            const auto samples = build({0.5});
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

        // The same file and the same API as the case above; the only difference is the stream.
        QFile unopened(path);
        talcs::AudioFormatIO unopenedIo(&unopened);
        QString error;
        auto prepared = Extractors::prepareAudio(&unopenedIo, 0, 500.0, SOURCE_RATE,
                                                 [] { return false; }, error);
        expect(!prepared.has_value(), "an unopened stream must not yield samples");
        expect(error == QStringLiteral("Failed to open the audio file"),
               "an unopened stream must be reported as a file-open failure, got: " + error);

        // The same file yields samples after opening; therefore the case above isolates the stream
        // state and not the file.
        QFile opened(path);
        expect(opened.open(QIODevice::ReadOnly), "the fixture file must be readable");
        talcs::AudioFormatIO openedIo(&opened);
        QString openedError;
        auto ok = Extractors::prepareAudio(&openedIo, 0, 500.0, SOURCE_RATE, [] { return false; },
                                           openedError);
        expect(ok.has_value(), "the same file read after opening must work: " + openedError);
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    testCutsAtSilence();
    testKeepsShortAudioWhole();
    testCapsSpansThatSilenceCannotCut();
    testResamplesToTheRateTheAnalyzerAsked();
    testRefusesAStreamThatWasNeverOpened();

    return failures == 0 ? 0 : 1;
}
