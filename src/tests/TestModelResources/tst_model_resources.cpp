#include "../TestSupport/ProcessFixture.h"
#include "../TestSupport/NativeRpc.h"
#include "../TestSupport/VoicebankFixture.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QtTest>

#include <sndfile.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <memory>

namespace {
    class NativeClient final {
    public:
        NativeClient(QProcess &editor, const QUrl &endpoint) : editor(editor), endpoint(endpoint) {
            manager.setProxy(QNetworkProxy::NoProxy);
        }

        bool call(const QString &operation, const QJsonObject &arguments, QJsonObject &result,
                  int timeoutMs = 5000) {
            error.clear();
            errorCode.clear();
            result = {};
            const auto id = QString::number(++sequence);
            const auto request = TestSupport::nativeRequest(id, operation, arguments);
            const auto requestBytes = QJsonDocument(request).toJson(QJsonDocument::Compact);
            TestSupport::recordProcessMessage(editor, "request", requestBytes);
            const auto response =
                TestSupport::nativeExchange(manager, endpoint, request, error, timeoutMs);
            TestSupport::readProcessStdout(editor);
            TestSupport::readProcessStderr(editor);
            if (!response) {
                error = operation + QStringLiteral(": ") + error;
                TestSupport::recordProcessMessage(editor, "failure", error.toUtf8());
                return false;
            }
            const auto bytes = QJsonDocument(*response).toJson(QJsonDocument::Compact);
            TestSupport::recordProcessMessage(editor, "response", bytes);
            if (response->contains(QStringLiteral("error")) ||
                !response->value(QStringLiteral("result")).isObject()) {
                errorCode = response->value(QStringLiteral("error"))
                                .toObject()
                                .value(QStringLiteral("data"))
                                .toObject()
                                .value(QStringLiteral("code"))
                                .toString();
                error = operation + QStringLiteral(": ") + QString::fromUtf8(bytes);
                return false;
            }
            result = response->value(QStringLiteral("result")).toObject();
            return true;
        }

        bool waitUntilReady() {
            QElapsedTimer deadline;
            deadline.start();
            QJsonObject status;
            while (deadline.elapsed() < 60000 && editor.state() != QProcess::NotRunning) {
                if (call(QStringLiteral("application.get_status"), {}, status, 1000) &&
                    status.value(QStringLiteral("host_mode")).toString() ==
                        QStringLiteral("headless"))
                    return true;
                QTest::qWait(100);
            }
            error = QStringLiteral("Headless startup failed: ") + error;
            return false;
        }

        bool mutate(const QString &operation, QJsonObject arguments, QJsonObject &result) {
            for (int attempt = 0; attempt < 4; ++attempt) {
                QJsonObject current;
                if (!call(QStringLiteral("documents.get"),
                          {
                              {QStringLiteral("document_id"), documentId}
                },
                          current))
                    return false;
                const auto document = current.value(QStringLiteral("document")).toObject();
                if (!document.value(QStringLiteral("revision")).isDouble()) {
                    error = QStringLiteral("documents.get did not return a revision");
                    return false;
                }
                arguments.insert(QStringLiteral("document_id"), documentId);
                arguments.insert(QStringLiteral("expected_revision"),
                                 document.value(QStringLiteral("revision")));
                if (call(operation, arguments, result))
                    return true;
                // Background inference may commit between requests. A rejected precondition
                // guarantees that this command was not applied; other failures are not retried.
                if (errorCode != QStringLiteral("revision_conflict"))
                    return false;
            }
            return false;
        }

        bool waitForInferenceModel(const QJsonObject &scope) {
            QElapsedTimer deadline;
            deadline.start();
            QJsonObject capabilities;
            // G2P and segmentation complete asynchronously after note insertion.
            while (deadline.elapsed() < 60000 && editor.state() != QProcess::NotRunning) {
                if (!call(QStringLiteral("inference.get_capabilities"),
                          {
                              {QStringLiteral("document_id"), documentId},
                              {QStringLiteral("scope"),       scope     }
                },
                          capabilities))
                    return false;
                if (!capabilities.value(QStringLiteral("capabilities"))
                         .toObject()
                         .value(QStringLiteral("models"))
                         .toArray()
                         .isEmpty())
                    return true;
                QTest::qWait(100);
            }
            error = QStringLiteral("The inserted note did not become an inference target: ") +
                    QString::fromUtf8(QJsonDocument(capabilities).toJson(QJsonDocument::Compact));
            return false;
        }

        bool waitForTask(const QJsonObject &accepted, bool applicationScope, int timeoutMs,
                         const QString &expectedState = QStringLiteral("succeeded")) {
            const auto taskId = accepted.value(QStringLiteral("task_id")).toString();
            if (taskId.isEmpty()) {
                error = QStringLiteral("Operation did not return a task_id: ") +
                        QString::fromUtf8(QJsonDocument(accepted).toJson(QJsonDocument::Compact));
                return false;
            }
            QJsonObject arguments{
                {QStringLiteral("task_id"), taskId                                            },
                {QStringLiteral("scope"),
                 applicationScope ? QStringLiteral("application") : QStringLiteral("document")}
            };
            if (!applicationScope)
                arguments.insert(QStringLiteral("document_id"), documentId);
            QElapsedTimer deadline;
            deadline.start();
            QJsonObject task;
            while (deadline.elapsed() < timeoutMs && editor.state() != QProcess::NotRunning) {
                if (!call(QStringLiteral("tasks.get"), arguments, task))
                    return false;
                const auto state = task.value(QStringLiteral("state")).toString();
                if (state == expectedState)
                    return true;
                if (state == QStringLiteral("succeeded") || state == QStringLiteral("failed") ||
                    state == QStringLiteral("canceled")) {
                    error = QStringLiteral("Unexpected resource task outcome: ") +
                            QString::fromUtf8(QJsonDocument(task).toJson(QJsonDocument::Compact));
                    return false;
                }
                QTest::qWait(200);
            }
            error = QStringLiteral("Resource task timed out or editor exited: ") +
                    QString::fromUtf8(QJsonDocument(task).toJson(QJsonDocument::Compact));
            return false;
        }

        QString documentId;
        QString error;

    private:
        QProcess &editor;
        QUrl endpoint;
        QNetworkAccessManager manager;
        QString errorCode;
        int sequence = 0;
    };

    int createdId(const QJsonObject &result, const QString &clientRef) {
        for (const auto &value : result.value(QStringLiteral("created_objects")).toArray()) {
            const auto binding = value.toObject();
            if (binding.value(QStringLiteral("client_ref")).toString() == clientRef)
                return binding.value(QStringLiteral("object"))
                    .toObject()
                    .value(QStringLiteral("id"))
                    .toInt();
        }
        return 0;
    }

    struct DecodedAudio {
        SF_INFO info{};
        QByteArray pcm;
        double energy = 0;
        QString error;
    };

    DecodedAudio decodeAudio(const QString &path) {
        DecodedAudio decoded;
#ifdef Q_OS_WIN
        auto *opened =
            sf_wchar_open(reinterpret_cast<const wchar_t *>(path.utf16()), SFM_READ, &decoded.info);
#else
        auto *opened = sf_open(QFile::encodeName(path).constData(), SFM_READ, &decoded.info);
#endif
        const std::unique_ptr<SNDFILE, decltype(&sf_close)> audio(opened, sf_close);
        if (!audio) {
            decoded.error = QString::fromUtf8(sf_strerror(nullptr));
            return decoded;
        }
        std::array<float, 4096> samples;
        while (const auto count = sf_read_float(audio.get(), samples.data(), samples.size())) {
            for (sf_count_t index = 0; index < count; ++index) {
                const auto sample = samples[static_cast<size_t>(index)];
                if (!std::isfinite(sample)) {
                    decoded.error = QStringLiteral("Exported PCM contains a non-finite sample");
                    return decoded;
                }
                decoded.energy += double(sample) * sample;
            }
            decoded.pcm.append(reinterpret_cast<const char *>(samples.data()),
                               count * sizeof(float));
        }
        if (sf_error(audio.get()) != SF_ERR_NO_ERROR)
            decoded.error = QString::fromUtf8(sf_strerror(audio.get()));
        return decoded;
    }
}

class TestModelResources final : public QObject {
    Q_OBJECT

public:
    QString editorPath;

private slots:

    void voicebankInferenceAndWaveExport_data() {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("lyric");
        QTest::addColumn<QString>("speakerId");
        QTest::addColumn<bool>("repairModel");
        if (TestSupport::usingBundledVoicebank()) {
            QTest::newRow("mandarin-clear") << QStringLiteral("cmn") << QStringLiteral("啦")
                                            << QStringLiteral("clear") << false;
            QTest::newRow("english-soft")
                << QStringLiteral("eng") << QStringLiteral("la") << QStringLiteral("soft") << false;
        } else {
            QTest::newRow("configured-voicebank")
                << TestSupport::fixtureLanguage() << TestSupport::fixtureLyric() << QString()
                << false;
        }
        QTest::newRow("repaired-acoustic-model")
            << QStringLiteral("eng") << QStringLiteral("la") << QStringLiteral("clear") << true;
    }

    void voicebankInferenceAndWaveExport() {
        QFETCH(QString, language);
        QFETCH(QString, lyric);
        QFETCH(QString, speakerId);
        QFETCH(bool, repairModel);
        const auto configuredRoot = repairModel ? QString::fromUtf8(LITE_TEST_VOICEBANK_ROOT)
                                                : TestSupport::voicebankRoot();
        QVERIFY2(!language.isEmpty(),
                 "DSEL_TEST_LANGUAGE is required when a voicebank is configured");
        QVERIFY2(!lyric.isEmpty(), "DSEL_TEST_LYRIC is required when a voicebank is configured");
        const QFileInfo rootInfo(configuredRoot);
        QVERIFY2(rootInfo.isAbsolute() && rootInfo.isDir(),
                 "DSEL_TEST_VOICEBANK_ROOT must name an existing absolute directory");
        auto voicebankRoot = rootInfo.canonicalFilePath();
        QVERIFY(!voicebankRoot.isEmpty());
        QVERIFY(QFileInfo::exists(editorPath));

        TestSupport::ProcessFixture fixture(QStringLiteral("headless-resources"));
        QVERIFY(fixture.isValid());
        QString damagedModelPath;
        QByteArray originalModel;
        if (repairModel) {
            const auto copy = fixture.filePath(QStringLiteral("voicebank"));
            std::error_code error;
            std::filesystem::copy(std::filesystem::u8path(voicebankRoot.toUtf8().constData()),
                                  std::filesystem::u8path(copy.toUtf8().constData()),
                                  std::filesystem::copy_options::recursive, error);
            QVERIFY2(!error, qPrintable(QString::fromStdString(error.message())));
            voicebankRoot = copy;
            damagedModelPath =
                QDir(copy).filePath(QStringLiteral("inferences/acoustic/acoustic.onnx"));
            QFile model(damagedModelPath);
            QVERIFY(model.open(QIODevice::ReadOnly));
            originalModel = model.readAll();
            QVERIFY(!originalModel.isEmpty());
            model.close();
            QVERIFY(model.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(model.write("invalid ONNX"), qint64(12));
        }
        QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("cache"))));
        QVERIFY(fixture.writeConfig({
            {QStringLiteral("general"),
             QJsonObject{{QStringLiteral("packageSearchPaths"), QJsonArray{voicebankRoot}}} },
            {QStringLiteral("inference"),
             QJsonObject{
                 {QStringLiteral("executionProvider"), QStringLiteral("CPU")},
                 {QStringLiteral("autoStartInfer"), false},
                 {QStringLiteral("runVocoderOnCpu"), true},
                 {QStringLiteral("samplingSteps"), 20},
                 {QStringLiteral("cacheDirectory"), fixture.filePath(QStringLiteral("cache"))},
             }                                                                              },
            {QStringLiteral("automation"),
             QJsonObject{
                 {QStringLiteral("accessRoots"), QJsonArray{voicebankRoot, fixture.path()}}}},
        }));
        QTcpServer portProbe;
        QVERIFY(portProbe.listen(QHostAddress::LocalHost, 0));
        const auto port = portProbe.serverPort();
        portProbe.close();
        auto &editor = fixture.process(QStringLiteral("editor"));
        editor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        editor.start(editorPath, {QStringLiteral("--headless"), QStringLiteral("--no-mcp"),
                                  QStringLiteral("--control-level"), QStringLiteral("l3"),
                                  QStringLiteral("--control-port"), QString::number(port)});
        QVERIFY2(editor.waitForStarted(10000), qPrintable(editor.errorString()));
        NativeClient client(editor,
                            QUrl(QStringLiteral("http://127.0.0.1:%1/automation/v1").arg(port)));
        QVERIFY2(client.waitUntilReady(), qPrintable(client.error));
        QJsonObject result;
        QVERIFY2(client.call(QStringLiteral("packages.refresh"), {}, result),
                 qPrintable(client.error));
        QVERIFY2(client.waitForTask(result, true, 60000), qPrintable(client.error));

        const auto requestedSinger =
            repairModel ? QStringLiteral("fixture") : TestSupport::fixtureSingerId();
        QVERIFY2(!requestedSinger.isEmpty(),
                 "DSEL_TEST_SINGER_ID is required when a voicebank is configured");
        QList<QJsonObject> candidates;
        QString cursor;
        do {
            QJsonObject arguments{
                {QStringLiteral("limit"), 100}
            };
            if (!cursor.isEmpty())
                arguments.insert(QStringLiteral("cursor"), cursor);
            QVERIFY2(client.call(QStringLiteral("voices.list"), arguments, result),
                     qPrintable(client.error));
            for (const auto &value : result.value(QStringLiteral("singers")).toArray()) {
                const auto singer = value.toObject();
                if (requestedSinger.isEmpty() ||
                    singer.value(QStringLiteral("singer_id")).toString() == requestedSinger)
                    candidates.append(singer);
            }
            cursor = result.value(QStringLiteral("next_cursor")).toString();
        } while (!cursor.isEmpty());
        QVERIFY2(candidates.size() == 1,
                 "Voicebank root must expose one matching singer; set DSEL_TEST_SINGER_ID or "
                 "provide a root containing only that package/version");
        const auto selected = candidates.first();
        const QJsonObject singer{
            {QStringLiteral("package_id"),      selected.value(QStringLiteral("package_id"))     },
            {QStringLiteral("package_version"), selected.value(QStringLiteral("package_version"))},
            {QStringLiteral("singer_id"),       selected.value(QStringLiteral("singer_id"))      }
        };
        QVERIFY2(client.call(QStringLiteral("voices.describe"),
                             {
                                 {QStringLiteral("singer"), singer}
        },
                             result),
                 qPrintable(client.error));
        const auto voiceSnapshot = result.value(QStringLiteral("snapshot")).toObject();
        QCOMPARE(voiceSnapshot.value(QStringLiteral("resolution_state")).toString(),
                 QStringLiteral("resolved"));
        bool languageReady = false;
        for (const auto &value : voiceSnapshot.value(QStringLiteral("languages")).toArray()) {
            const auto entry = value.toObject();
            if (entry.value(QStringLiteral("language_id")).toString() == language)
                languageReady = entry.value(QStringLiteral("g2p_ready")).toBool();
        }
        QVERIFY2(languageReady,
                 "The configured language must be supported by the singer and have a ready G2P");
        QJsonValue speaker(QJsonValue::Null);
        const auto defaultSpeaker =
            speakerId.isEmpty()
                ? voiceSnapshot.value(QStringLiteral("default_speaker_id")).toString()
                : speakerId;
        if (!defaultSpeaker.isEmpty())
            speaker = QJsonObject{
                {QStringLiteral("speaker_id"), defaultSpeaker}
            };

        QVERIFY2(client.call(QStringLiteral("documents.new"),
                             {
                                 {QStringLiteral("unsaved_policy"), QStringLiteral("discard")}
        },
                             result),
                 qPrintable(client.error));
        client.documentId = result.value(QStringLiteral("current"))
                                .toObject()
                                .value(QStringLiteral("document_id"))
                                .toString();
        QVERIFY(!client.documentId.isEmpty());
        QVERIFY2(
            client.mutate(QStringLiteral("tracks.insert"),
                          {
                              {QStringLiteral("index"),  0                                    },
                              {QStringLiteral("tracks"),
                               QJsonArray{QJsonObject{
                                   {QStringLiteral("client_ref"), QStringLiteral("track")},
                                   {QStringLiteral("name"), QStringLiteral("Resource test")}}}},
        },
                          result),
            qPrintable(client.error));
        const auto trackId = createdId(result, QStringLiteral("track"));
        QVERIFY(trackId > 0);
        QVERIFY2(client.mutate(QStringLiteral("tracks.set_voice"),
                               {
                                   {QStringLiteral("track_id"), trackId              },
                                   {QStringLiteral("voice"),
                                    QJsonObject{{QStringLiteral("singer"), singer},
                                                {QStringLiteral("speaker"), speaker}}},
        },
                               result),
                 qPrintable(client.error));
        QVERIFY2(client.mutate(QStringLiteral("clips.insert"),
                               {
                                   {QStringLiteral("clips"),
                                    QJsonArray{QJsonObject{
                                        {QStringLiteral("track_id"), trackId},
                                        {QStringLiteral("start"), 0},
                                        {QStringLiteral("client_ref"), QStringLiteral("clip")}}}},
        },
                               result),
                 qPrintable(client.error));
        const auto clipId = createdId(result, QStringLiteral("clip"));
        QVERIFY(clipId > 0);
        QVERIFY2(client.mutate(
                     QStringLiteral("notes.insert"),
                     {
                         {QStringLiteral("clip_id"), clipId},
                         {QStringLiteral("notes"),
                          QJsonArray{QJsonObject{
                              {QStringLiteral("local_start"), 0},
                              {QStringLiteral("length"), 960},
                              {QStringLiteral("key_index"), 60},
                              {QStringLiteral("lyric"), lyric},
                              {QStringLiteral("language"),
                               QJsonObject{{QStringLiteral("mode"), QStringLiteral("explicit")},
                                           {QStringLiteral("language_id"), language}}},
                          }}                               },
        },
                     result),
                 qPrintable(client.error));

        const QJsonObject scope{
            {QStringLiteral("kind"),     QStringLiteral("clip")},
            {QStringLiteral("clip_ids"), QJsonArray{clipId}    }
        };
        QVERIFY2(client.waitForInferenceModel(scope), qPrintable(client.error));
        if (TestSupport::usingBundledVoicebank() || repairModel) {
            QVERIFY2(client.call(QStringLiteral("notes.list"),
                                 {
                                     {QStringLiteral("document_id"), client.documentId},
                                     {QStringLiteral("clip_id"),     clipId           }
            },
                                 result),
                     qPrintable(client.error));
            const auto notes = result.value(QStringLiteral("notes")).toArray();
            QCOMPARE(notes.size(), 1);
            const auto note = notes.first().toObject();
            QCOMPARE(note.value(QStringLiteral("pronunciation"))
                         .toObject()
                         .value(QStringLiteral("value"))
                         .toString(),
                     language == QStringLiteral("cmn") ? QStringLiteral("la")
                                                       : QStringLiteral("l aa"));
            const auto phonemes = note.value(QStringLiteral("phonemes")).toArray();
            QCOMPARE(phonemes.size(), 2);
            QCOMPARE(phonemes.first().toObject().value(QStringLiteral("symbol")).toString(),
                     QStringLiteral("l"));
            QCOMPARE(phonemes.last().toObject().value(QStringLiteral("symbol")).toString(),
                     language == QStringLiteral("cmn") ? QStringLiteral("a")
                                                       : QStringLiteral("aa"));
        }
        QVERIFY2(
            client.mutate(QStringLiteral("inference.start"),
                          {
                              {QStringLiteral("scope"),   scope                                   },
                              {QStringLiteral("options"),
                               QJsonObject{{QStringLiteral("provider_id"), QStringLiteral("CPU")}}},
        },
                          result),
            qPrintable(client.error));
        if (repairModel) {
            QVERIFY2(client.waitForTask(result, false, 30000, QStringLiteral("failed")),
                     qPrintable(client.error));
            const QDir cache(fixture.filePath(QStringLiteral("cache")));
            QVERIFY(cache.entryList({QStringLiteral("*.wav")}, QDir::Files).isEmpty());
            QFile model(damagedModelPath);
            QVERIFY(model.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(model.write(originalModel), originalModel.size());
            model.close();
            QVERIFY2(client.mutate(
                         QStringLiteral("inference.start"),
                         {
                             {QStringLiteral("scope"),   scope                                   },
                             {QStringLiteral("options"),
                              QJsonObject{{QStringLiteral("provider_id"), QStringLiteral("CPU")}}}
            },
                         result),
                     qPrintable(client.error));
        }
        QVERIFY2(client.waitForTask(result, false, 300000), qPrintable(client.error));

        const auto output = fixture.filePath(QStringLiteral("render.wav"));
        QJsonObject exportArguments{
            {QStringLiteral("document_id"), client.documentId},
            {QStringLiteral("path"),        output           },
            {QStringLiteral("options"),
             QJsonObject{
                 {QStringLiteral("format"), QStringLiteral("wav")},
                 {QStringLiteral("sample_rate"), 44100},
                 {QStringLiteral("channel_mode"), QStringLiteral("mono")},
                 {QStringLiteral("mixing_mode"), QStringLiteral("mixed")},
                 {QStringLiteral("source"), QStringLiteral("all")},
             }                                               },
        };
        QVERIFY2(client.call(QStringLiteral("exports.audio.preview"), exportArguments, result),
                 qPrintable(client.error));
        for (const auto &value : result.value(QStringLiteral("plan"))
                                     .toObject()
                                     .value(QStringLiteral("diagnostics"))
                                     .toArray())
            QVERIFY2(!value.toObject().value(QStringLiteral("blocking")).toBool(),
                     qPrintable(QString::fromUtf8(QJsonDocument(value.toObject()).toJson())));
        exportArguments.insert(QStringLiteral("overwrite_policy"), QStringLiteral("reject"));
        QVERIFY2(client.call(QStringLiteral("exports.audio.start"), exportArguments, result),
                 qPrintable(client.error));
        QVERIFY2(client.waitForTask(result, false, 120000), qPrintable(client.error));

        const auto firstAudio = decodeAudio(output);
        QVERIFY2(firstAudio.error.isEmpty(), qPrintable(firstAudio.error));
        QVERIFY(firstAudio.info.frames > 0);
        QCOMPARE(firstAudio.info.samplerate, 44100);
        QCOMPARE(firstAudio.info.channels, 1);
        QCOMPARE(firstAudio.pcm.size(), firstAudio.info.frames * qint64(sizeof(float)));
        QVERIFY2(firstAudio.energy > 0,
                 "Successful synthesis and export must produce non-silent audio");

        if (TestSupport::usingBundledVoicebank() && language == QStringLiteral("cmn")) {
            const QDir cache(fixture.filePath(QStringLiteral("cache")));
            const auto cachedAudio = cache.entryList({QStringLiteral("*.wav")}, QDir::Files);
            QVERIFY(!cachedAudio.isEmpty());
            QMap<QString, QDateTime> initialWrites;
            for (const auto &name : cachedAudio)
                initialWrites.insert(name, QFileInfo(cache.filePath(name)).lastModified());
            const auto inferAndExport = [&](const QString &name) {
                if (!client.waitForInferenceModel(scope) ||
                    !client.mutate(
                        QStringLiteral("inference.start"),
                        {
                            {QStringLiteral("scope"),   scope                                   },
                            {QStringLiteral("options"),
                             QJsonObject{{QStringLiteral("provider_id"), QStringLiteral("CPU")}}}
                },
                        result) ||
                    !client.waitForTask(result, false, 300000))
                    return false;
                exportArguments.insert(QStringLiteral("path"), fixture.filePath(name));
                return client.call(QStringLiteral("exports.audio.start"), exportArguments,
                                   result) &&
                       client.waitForTask(result, false, 120000);
            };

            QVERIFY2(inferAndExport(QStringLiteral("repeat.wav")), qPrintable(client.error));
            const auto repeatedAudio = decodeAudio(fixture.filePath(QStringLiteral("repeat.wav")));
            QVERIFY2(repeatedAudio.error.isEmpty(), qPrintable(repeatedAudio.error));
            QCOMPARE(repeatedAudio.pcm, firstAudio.pcm);
            QCOMPARE(cache.entryList({QStringLiteral("*.wav")}, QDir::Files), cachedAudio);
            for (auto it = initialWrites.cbegin(); it != initialWrites.cend(); ++it)
                QCOMPARE(QFileInfo(cache.filePath(it.key())).lastModified(), it.value());

            QVERIFY2(client.mutate(QStringLiteral("tracks.set_voice"),
                                   {
                                       {QStringLiteral("track_id"), trackId                 },
                                       {QStringLiteral("voice"),
                                        QJsonObject{{QStringLiteral("singer"), singer},
                                                    {QStringLiteral("speaker"),
                                                     QJsonObject{{QStringLiteral("speaker_id"),
                                                                  QStringLiteral("soft")}}}}}
            },
                                   result),
                     qPrintable(client.error));
            QVERIFY2(inferAndExport(QStringLiteral("soft.wav")), qPrintable(client.error));
            const auto softAudio = decodeAudio(fixture.filePath(QStringLiteral("soft.wav")));
            QVERIFY2(softAudio.error.isEmpty(), qPrintable(softAudio.error));
            QCOMPARE(softAudio.info.frames, firstAudio.info.frames);
            QVERIFY(softAudio.energy > 0);
            QVERIFY(softAudio.pcm != firstAudio.pcm);
            const auto changedSpeakerCache =
                cache.entryList({QStringLiteral("*.wav")}, QDir::Files);
            QVERIFY(changedSpeakerCache != cachedAudio);

            QVERIFY2(
                client.mutate(QStringLiteral("tempos.set"),
                              {
                                  {QStringLiteral("tick"),  0    },
                                  {QStringLiteral("tempo"), 240.0}
            },
                              result),
                qPrintable(client.error));
            QVERIFY2(inferAndExport(QStringLiteral("faster.wav")), qPrintable(client.error));
            const auto fasterAudio = decodeAudio(fixture.filePath(QStringLiteral("faster.wav")));
            QVERIFY2(fasterAudio.error.isEmpty(), qPrintable(fasterAudio.error));
            QVERIFY(fasterAudio.info.frames > 0);
            QVERIFY(fasterAudio.info.frames < softAudio.info.frames);
            QVERIFY(fasterAudio.energy > 0);
            QVERIFY(cache.entryList({QStringLiteral("*.wav")}, QDir::Files) != changedSpeakerCache);
        }
        QVERIFY2(client.call(QStringLiteral("application.request_exit"),
                             {
                                 {QStringLiteral("discard_changes"), true}
        },
                             result),
                 qPrintable(client.error));
        QVERIFY(editor.waitForFinished(15000));
        QCOMPARE(editor.exitStatus(), QProcess::NormalExit);
        QCOMPARE(editor.exitCode(), 0);
    }
};

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    TestModelResources test;
    auto arguments = application.arguments();
    const auto index = arguments.indexOf(QStringLiteral("--editor"));
    if (index >= 0 && index + 1 < arguments.size()) {
        arguments.removeAt(index);
        test.editorPath = arguments.takeAt(index);
    }
    return QTest::qExec(&test, arguments);
}

#include "tst_model_resources.moc"
