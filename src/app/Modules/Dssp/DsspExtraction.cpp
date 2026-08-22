#include "DsspExtraction.h"

#include "Model/AppOptions/AppOptions.h"

#include <lite/SynthrtEngine/SynthrtEngine.h>
#include <lite/Support/StringUtils.h>

#include <synthrt/Audio/AudioPipeline.h>
#include <synthrt/Audio/ResampleConfig.h>
#include <synthrt/Core/Plugin/PluginFactory.h>
#include <synthrt/Extract/MidiExtractor.h>
#include <synthrt/Extract/MidiExtractorPlugin.h>
#include <synthrt/Extract/PitchExtractor.h>
#include <synthrt/Extract/PitchExtractorPlugin.h>

#include <QDir>
#include <QLoggingCategory>
#include <QTemporaryFile>

#include <cmath>
#include <mutex>
#include <semaphore>

Q_LOGGING_CATEGORY(logDsspExtraction, "dssp.extraction")

namespace DsspExtraction {

    namespace {

        constexpr auto kNoteExtractorId = "game";
        constexpr auto kPitchExtractorId = "rmvpe";

        // Extraction also loads ONNX models; serialize HTTP extraction requests so
        // they do not pile onto document extraction tasks unboundedly.
        std::counting_semaphore<1> g_extractionSlots{1};

        struct ExtractionSlotGuard {
            ExtractionSlotGuard() { g_extractionSlots.acquire(); }
            ~ExtractionSlotGuard() { g_extractionSlots.release(); }
        };

        // Cached audio requirements per extractor id (queried on first successful
        // open). Guarded by a mutex because HTTP worker threads may race.
        std::mutex g_requirementsMutex;
        int g_notePreferredSampleRate = 44100;  // nominal GAME input rate
        int g_pitchPreferredSampleRate = 16000; // nominal RMVPE input rate

        std::optional<DsspApi::Problem> parseExtractionRequest(const QJsonObject &body,
                                                               QString &extractorId,
                                                               QByteArray &audioData) {
            if (!DsspApi::readString(body, QStringLiteral("extractor"), extractorId)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"extractor\""));
            }
            QJsonObject input;
            if (!DsspApi::readObject(body, QStringLiteral("input"), input)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"input\""));
            }
            QString audioUrl;
            if (!DsspApi::readString(input, QStringLiteral("audio_url"), audioUrl)) {
                return DsspApi::validationError(
                    QStringLiteral("Missing request field \"input.audio_url\""));
            }
            QString mimeType;
            if (!DsspApi::decodeDataUrl(audioUrl, audioData, mimeType) || audioData.isEmpty()) {
                return DsspApi::validationError(
                    QStringLiteral("Unsupported or invalid \"input.audio_url\" (data URL expected)"));
            }
            return std::nullopt;
        }

        /// Write the raw audio bytes to a temporary file and decode them via the
        /// FFmpeg pipeline (the decoder only accepts file paths).
        std::optional<DsspApi::Problem> decodeAudioBytes(const QByteArray &audioData,
                                                         srt::audio::AudioBuffer &buffer,
                                                         int &sampleRate) {
            QString tempPath;
            {
                QTemporaryFile tempFile(
                    QStringLiteral("%1/dssp_audio_XXXXXX").arg(QDir::tempPath()));
                tempFile.setAutoRemove(false);
                if (!tempFile.open()) {
                    return DsspApi::internalError(
                        QStringLiteral("Failed to create temporary audio file"));
                }
                if (tempFile.write(audioData) != audioData.size()) {
                    return DsspApi::internalError(
                        QStringLiteral("Failed to write temporary audio file"));
                }
                tempFile.flush();
                tempPath = tempFile.fileName();
            }

            const auto utf8Path = tempPath.toUtf8().toStdString();
            auto pipeline = srt::audio::AudioPipeline::create();
            auto infoExp = pipeline.probe(utf8Path);
            if (!infoExp) {
                QFile::remove(tempPath);
                return DsspApi::internalError(
                    QStringLiteral("Failed to probe audio data: %1")
                        .arg(QString::fromUtf8(infoExp.error().message())));
            }
            const auto info = infoExp.take();
            auto bufferExp = pipeline.decodeAndResample(
                utf8Path, srt::audio::ResampleConfig::forMonoFloat(info.sampleRate));
            QFile::remove(tempPath);
            if (!bufferExp) {
                return DsspApi::internalError(
                    QStringLiteral("Failed to decode audio data: %1")
                        .arg(QString::fromUtf8(bufferExp.error().message())));
            }
            buffer = bufferExp.take();
            sampleRate = info.sampleRate;
            return std::nullopt;
        }

        srt::core::Expected<srt::core::NO<srt::extract::MidiExtractor>>
            createMidiExtractor(SynthrtEngine::RuntimeOperationLease &lease) {
            auto *plugins = lease.runtime().services().get<srt::core::PluginFactory>();
            if (!plugins)
                return srt::core::Error(srt::core::ErrorCode::InferenceNotInitialized,
                                        "PluginFactory is not available");
            auto *plugin = plugins->plugin<srt::extract::MidiExtractorPlugin>(kNoteExtractorId);
            if (!plugin)
                return srt::core::Error(srt::core::ErrorCode::InferenceModelNotFound,
                                        "GAME MidiExtractor plugin not found");
            return plugin->createExtractor(&lease.runtime());
        }

        srt::core::Expected<srt::core::NO<srt::extract::PitchExtractor>>
            createPitchExtractor(SynthrtEngine::RuntimeOperationLease &lease) {
            auto *plugins = lease.runtime().services().get<srt::core::PluginFactory>();
            if (!plugins)
                return srt::core::Error(srt::core::ErrorCode::InferenceNotInitialized,
                                        "PluginFactory is not available");
            auto *plugin = plugins->plugin<srt::extract::PitchExtractorPlugin>(kPitchExtractorId);
            if (!plugin)
                return srt::core::Error(srt::core::ErrorCode::InferenceModelNotFound,
                                        "RMVPE PitchExtractor plugin not found");
            return plugin->createExtractor(&lease.runtime());
        }

        double midiToCent(const double midi) {
            if (midi <= 0)
                return 0;
            return std::clamp(midi * 100.0, 0.0, 12800.0);
        }

        QJsonObject extractorInfo(const QString &id, const QString &name,
                                  const int preferredSampleRate) {
            return QJsonObject{
                {QStringLiteral("id"), id},
                {QStringLiteral("name"), name},
                {QStringLiteral("preferred_audio_sample_rate"), preferredSampleRate},
            };
        }

        int notePreferredSampleRate() {
            std::lock_guard lock(g_requirementsMutex);
            return g_notePreferredSampleRate;
        }

        int pitchPreferredSampleRate() {
            std::lock_guard lock(g_requirementsMutex);
            return g_pitchPreferredSampleRate;
        }

    } // namespace

    DsspApi::Result extractorList() {
        const QJsonObject list{
            {QStringLiteral("note"),
             QJsonArray{extractorInfo(QString::fromLatin1(kNoteExtractorId),
                                      QStringLiteral("GAME"), notePreferredSampleRate())}},
            {QStringLiteral("tempo"), QJsonArray{}},
            {QStringLiteral("pitch"),
             QJsonArray{extractorInfo(QString::fromLatin1(kPitchExtractorId),
                                      QStringLiteral("RMVPE"), pitchPreferredSampleRate())}},
        };
        return DsspApi::Result::ok(list);
    }

    DsspApi::Result noteExtractor(const QString &extractorId) {
        if (extractorId != QLatin1String(kNoteExtractorId)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Unknown note extractor \"%1\"").arg(extractorId)));
        }
        return DsspApi::Result::ok(extractorInfo(QString::fromLatin1(kNoteExtractorId),
                                                 QStringLiteral("GAME"),
                                                 notePreferredSampleRate()));
    }

    DsspApi::Result tempoExtractor(const QString &extractorId) {
        Q_UNUSED(extractorId);
        return DsspApi::Result::fail(DsspApi::notImplemented(
            QStringLiteral("Tempo extraction is not implemented by this service")));
    }

    DsspApi::Result pitchExtractor(const QString &extractorId) {
        if (extractorId != QLatin1String(kPitchExtractorId)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Unknown pitch extractor \"%1\"").arg(extractorId)));
        }
        return DsspApi::Result::ok(extractorInfo(QString::fromLatin1(kPitchExtractorId),
                                                 QStringLiteral("RMVPE"),
                                                 pitchPreferredSampleRate()));
    }

    DsspApi::Result extractNote(const QJsonObject &body) {
        if (!SynthrtEngine::instance().midiExtractionReady()) {
            return DsspApi::Result::fail(
                DsspApi::internalError(QStringLiteral("Note extraction is not available")));
        }
        QString extractorId;
        QByteArray audioData;
        if (auto problem = parseExtractionRequest(body, extractorId, audioData))
            return DsspApi::Result::fail(*problem);
        if (extractorId != QLatin1String(kNoteExtractorId)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Unknown note extractor \"%1\"").arg(extractorId)));
        }

        const auto gameDir = appOptions->general()->gameDir;
        const auto modelPath = StringUtils::qstr_to_path(gameDir);
        if (modelPath.empty() || !exists(modelPath) || !is_directory(modelPath)) {
            return DsspApi::Result::fail(DsspApi::internalError(
                gameDir.isEmpty() ? QStringLiteral("GAME model directory is not configured")
                                  : QStringLiteral("Invalid GAME model directory: %1").arg(gameDir)));
        }

        ExtractionSlotGuard slotGuard;
        auto runtimeLease = SynthrtEngine::instance().acquireMidiExtractionOperation();
        if (!runtimeLease) {
            return DsspApi::Result::fail(
                DsspApi::internalError(QStringLiteral("MIDI extraction is not available")));
        }
        auto extractorExp = createMidiExtractor(runtimeLease);
        if (!extractorExp) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Failed to create GAME extractor: %1")
                    .arg(QString::fromUtf8(extractorExp.error().message()))));
        }
        auto extractor = extractorExp.take();
        if (auto exp = extractor->open(modelPath); !exp) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Failed to open GAME model: %1")
                    .arg(QString::fromUtf8(exp.error().message()))));
        }
        {
            std::lock_guard lock(g_requirementsMutex);
            g_notePreferredSampleRate = extractor->audioRequirements().sampleRate;
        }

        srt::audio::AudioBuffer buffer;
        int sampleRate = 0;
        if (auto problem = decodeAudioBytes(audioData, buffer, sampleRate))
            return DsspApi::Result::fail(*problem);

        srt::extract::MidiExtractOptions options;
        options.tempo = 120.0f;
        auto resultExp = extractor->extract(buffer, sampleRate, options);
        if (!resultExp) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("GAME extraction failed: %1")
                    .arg(QString::fromUtf8(resultExp.error().message()))));
        }

        const auto result = resultExp.take();
        constexpr double kTicksPerBeat = 480.0;
        constexpr double kSecondsPerBeat = 60.0 / 120.0;
        constexpr double kSecondsPerTick = kSecondsPerBeat / kTicksPerBeat;

        QJsonArray notes;
        for (const auto &note : result.notes) {
            notes.append(QJsonObject{
                {QStringLiteral("position"),
                 QJsonObject{
                     {QStringLiteral("gap"), note.start * kSecondsPerTick},
                     {QStringLiteral("duration"), note.duration * kSecondsPerTick},
                 }},
                {QStringLiteral("cent"), note.note * 100},
            });
        }

        return DsspApi::Result::ok(QJsonObject{
            {QStringLiteral("output"), QJsonObject{{QStringLiteral("notes"), notes}}},
        });
    }

    DsspApi::Result extractTempo(const QJsonObject &body) {
        Q_UNUSED(body);
        return DsspApi::Result::fail(DsspApi::notImplemented(
            QStringLiteral("Tempo extraction is not implemented by this service")));
    }

    DsspApi::Result extractPitch(const QJsonObject &body) {
        if (!SynthrtEngine::instance().pitchExtractionReady()) {
            return DsspApi::Result::fail(
                DsspApi::internalError(QStringLiteral("Pitch extraction is not available")));
        }
        QString extractorId;
        QByteArray audioData;
        if (auto problem = parseExtractionRequest(body, extractorId, audioData))
            return DsspApi::Result::fail(*problem);
        if (extractorId != QLatin1String(kPitchExtractorId)) {
            return DsspApi::Result::fail(DsspApi::validationError(
                QStringLiteral("Unknown pitch extractor \"%1\"").arg(extractorId)));
        }

        const auto rmvpePath = appOptions->general()->rmvpePath;
        const auto modelPath = StringUtils::qstr_to_path(rmvpePath);
        if (modelPath.empty() || !exists(modelPath) || is_directory(modelPath)) {
            return DsspApi::Result::fail(DsspApi::internalError(
                rmvpePath.isEmpty() ? QStringLiteral("RMVPE model path is not configured")
                                    : QStringLiteral("Invalid RMVPE model path: %1").arg(rmvpePath)));
        }

        ExtractionSlotGuard slotGuard;
        auto runtimeLease = SynthrtEngine::instance().acquirePitchExtractionOperation();
        if (!runtimeLease) {
            return DsspApi::Result::fail(
                DsspApi::internalError(QStringLiteral("Pitch extraction is not available")));
        }
        auto extractorExp = createPitchExtractor(runtimeLease);
        if (!extractorExp) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Failed to create RMVPE extractor: %1")
                    .arg(QString::fromUtf8(extractorExp.error().message()))));
        }
        auto extractor = extractorExp.take();
        if (auto exp = extractor->open(modelPath); !exp) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("Failed to open RMVPE model: %1")
                    .arg(QString::fromUtf8(exp.error().message()))));
        }
        {
            std::lock_guard lock(g_requirementsMutex);
            g_pitchPreferredSampleRate = extractor->audioRequirements().sampleRate;
        }

        srt::audio::AudioBuffer buffer;
        int sampleRate = 0;
        if (auto problem = decodeAudioBytes(audioData, buffer, sampleRate))
            return DsspApi::Result::fail(*problem);

        auto resultExp = extractor->extract(buffer, sampleRate);
        if (!resultExp) {
            return DsspApi::Result::fail(DsspApi::internalError(
                QStringLiteral("RMVPE extraction failed: %1")
                    .arg(QString::fromUtf8(resultExp.error().message()))));
        }

        // Flatten slices into 10 ms frames (aligned with the in-app extraction
        // convention) and group voiced runs into segments.
        const auto result = resultExp.take();
        constexpr int kFrameRate = 100; // 10 ms per frame
        QJsonArray segments;
        QJsonArray segment;
        int gap = 0;
        bool inSegment = false;
        const auto flushSegment = [&] {
            if (!inSegment)
                return;
            segments.append(QJsonObject{
                {QStringLiteral("gap"), gap},
                {QStringLiteral("pitch"), segment},
            });
            segment = QJsonArray{};
            inSegment = false;
            gap = 0;
        };
        for (const auto &frame : result.frames) {
            for (size_t i = 0; i < frame.f0.size(); ++i) {
                const auto f0 = frame.f0[i];
                const auto voiced = (i < frame.uv.size()) ? frame.uv[i] : f0 > 0;
                if (!voiced || f0 <= 0) {
                    if (inSegment)
                        flushSegment();
                    else
                        ++gap;
                    continue;
                }
                const auto midi = 69 + 12 * std::log2(f0 / 440.0);
                segment.append(midiToCent(midi));
                inSegment = true;
            }
        }
        flushSegment();

        return DsspApi::Result::ok(QJsonObject{
            {QStringLiteral("output"),
             QJsonObject{
                 {QStringLiteral("segments"), segments},
                 {QStringLiteral("sample_rate"), kFrameRate},
             }},
        });
    }

} // namespace DsspExtraction
