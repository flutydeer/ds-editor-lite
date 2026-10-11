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

#include <algorithm>
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

    void runtimePluginFailurePreservesEditing_data() {
        QTest::addColumn<bool>("createDirectory");
        QTest::newRow("missing-plugin-root") << false;
        QTest::newRow("empty-plugin-root") << true;
    }

    void runtimePluginFailurePreservesEditing() {
        QFETCH(bool, createDirectory);
        TestSupport::ProcessFixture fixture(QStringLiteral("headless-plugin-recovery"));
        QVERIFY(fixture.isValid());
        QVERIFY(fixture.writeConfig({
            {QStringLiteral("general"),
             QJsonObject{{QStringLiteral("packageSearchPaths"),
                          QJsonArray{QString::fromUtf8(LITE_TEST_VOICEBANK_ROOT)}}}        },
            {QStringLiteral("inference"),
             QJsonObject{{QStringLiteral("executionProvider"), QStringLiteral("CPU")},
                         {QStringLiteral("autoStartInfer"), false}}                        },
            {QStringLiteral("automation"),
             QJsonObject{
                 {QStringLiteral("accessRoots"),
                  QJsonArray{fixture.path(), QString::fromUtf8(LITE_TEST_VOICEBANK_ROOT)}}}},
        }));
        const auto invalidRoot = fixture.filePath(QStringLiteral("runtime-plugins"));
        if (createDirectory)
            QVERIFY(QDir().mkpath(invalidRoot));
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const auto port = reservation.serverPort();
        reservation.close();
        auto &editor = fixture.process(QStringLiteral("editor"));
        auto environment = fixture.environment();
        environment.insert(QStringLiteral("DSEL_TEST_PLUGIN_ROOT"), invalidRoot);
        editor.setProcessEnvironment(environment);
        editor.setWorkingDirectory(QFileInfo(editorPath).absolutePath());
        NativeClient client(editor,
                            QUrl(QStringLiteral("http://127.0.0.1:%1/automation/v1").arg(port)));
        const auto start = [&] {
            editor.start(editorPath, {QStringLiteral("--headless"), QStringLiteral("--no-mcp"),
                                      QStringLiteral("--control-level"), QStringLiteral("l3"),
                                      QStringLiteral("--control-port"), QString::number(port)});
            return editor.waitForStarted(10000) && client.waitUntilReady();
        };
        QVERIFY2(start(), qPrintable(client.error));
        QJsonObject result;
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
        const QJsonObject document{
            {QStringLiteral("document_id"), client.documentId}
        };
        QVERIFY2(client.call(QStringLiteral("documents.get"), document, result),
                 qPrintable(client.error));
        const auto before = result;
        QVERIFY2(client.call(QStringLiteral("packages.refresh"), {}, result),
                 qPrintable(client.error));
        const auto failedTaskId = result.value(QStringLiteral("task_id"));
        QVERIFY2(client.waitForTask(result, true, 60000, QStringLiteral("failed")),
                 qPrintable(client.error));
        QVERIFY2(client.call(QStringLiteral("tasks.get"),
                             {
                                 {QStringLiteral("scope"),   QStringLiteral("application")},
                                 {QStringLiteral("task_id"), failedTaskId                 }
        },
                             result),
                 qPrintable(client.error));
        const auto error = result.value(QStringLiteral("error")).toObject();
        QCOMPARE(error.value(QStringLiteral("code")).toString(),
                 QStringLiteral("module_not_ready"));
        QVERIFY(!error.value(QStringLiteral("message")).toString().isEmpty());
        QVERIFY2(client.call(QStringLiteral("documents.get"), document, result),
                 qPrintable(client.error));
        QCOMPARE(result, before);
        QVERIFY2(
            client.mutate(
                QStringLiteral("tracks.insert"),
                {
                    {QStringLiteral("index"),  0                                                 },
                    {QStringLiteral("tracks"),
                     QJsonArray{QJsonObject{
                         {QStringLiteral("client_ref"), QStringLiteral("track")},
                         {QStringLiteral("name"), QStringLiteral("Editable without inference")}}}}
        },
                result),
            qPrintable(client.error));
        const auto trackId = createdId(result, QStringLiteral("track"));
        QVERIFY(trackId > 0);
        QVERIFY2(client.call(QStringLiteral("tracks.list"), document, result),
                 qPrintable(client.error));
        const auto tracks = result.value(QStringLiteral("tracks")).toArray();
        QCOMPARE(tracks.size(), 1);
        QCOMPARE(tracks.first().toObject().value(QStringLiteral("track_id")).toInt(), trackId);
        QVERIFY2(client.mutate(QStringLiteral("history.undo"), {}, result),
                 qPrintable(client.error));
        QVERIFY2(client.call(QStringLiteral("tracks.list"), document, result),
                 qPrintable(client.error));
        QVERIFY(result.value(QStringLiteral("tracks")).toArray().isEmpty());
        const auto exit = [&] {
            return client.call(QStringLiteral("application.request_exit"),
                               {
                                   {QStringLiteral("discard_changes"), true}
            },
                               result) &&
                   editor.waitForFinished(15000) && editor.exitStatus() == QProcess::NormalExit &&
                   editor.exitCode() == 0;
        };
        QVERIFY2(exit(), qPrintable(client.error));
        environment.remove(QStringLiteral("DSEL_TEST_PLUGIN_ROOT"));
        editor.setProcessEnvironment(environment);
        QVERIFY2(start(), qPrintable(client.error));
        QVERIFY2(client.call(QStringLiteral("packages.refresh"), {}, result),
                 qPrintable(client.error));
        QVERIFY2(client.waitForTask(result, true, 60000), qPrintable(client.error));
        QVERIFY2(client.call(QStringLiteral("voices.list"), {}, result), qPrintable(client.error));
        QVERIFY(!result.value(QStringLiteral("singers")).toArray().isEmpty());
        QVERIFY2(exit(), qPrintable(client.error));
    }

    void voicebankInferenceAndWaveExport_data() {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("lyric");
        QTest::addColumn<QString>("speakerId");
        QTest::addColumn<QString>("damagedStage");
        QTest::addColumn<QString>("providerId");
        QTest::addColumn<bool>("runtimeFailure");
        if (TestSupport::usingBundledVoicebank()) {
            QTest::newRow("mandarin-clear")
                << QStringLiteral("cmn") << QStringLiteral("啦") << QStringLiteral("clear")
                << QString() << QStringLiteral("CPU") << false;
            QTest::newRow("english-soft")
                << QStringLiteral("eng") << QStringLiteral("la") << QStringLiteral("soft")
                << QString() << QStringLiteral("CPU") << false;
        } else {
            QTest::newRow("configured-voicebank")
                << TestSupport::fixtureLanguage() << TestSupport::fixtureLyric() << QString()
                << QString() << QStringLiteral("CPU") << false;
        }
        QTest::newRow("repaired-acoustic-model")
            << QStringLiteral("eng") << QStringLiteral("la") << QStringLiteral("clear")
            << QStringLiteral("acoustic") << QStringLiteral("CPU") << false;
        QTest::newRow("repaired-vocoder-model")
            << QStringLiteral("eng") << QStringLiteral("la") << QStringLiteral("clear")
            << QStringLiteral("vocoder") << QStringLiteral("CPU") << false;
        for (const auto &stage :
             {QStringLiteral("duration"), QStringLiteral("pitch"), QStringLiteral("variance"),
              QStringLiteral("acoustic"), QStringLiteral("vocoder")}) {
            QTest::newRow(qPrintable(QStringLiteral("running-%1-model-recovers").arg(stage)))
                << QStringLiteral("eng") << QStringLiteral("la") << QStringLiteral("clear") << stage
                << QStringLiteral("CPU") << true;
        }
        QTest::newRow("directml-device")
            << QStringLiteral("eng") << QStringLiteral("la") << QStringLiteral("clear") << QString()
            << QStringLiteral("DirectML") << false;
        QTest::newRow("cuda-device")
            << QStringLiteral("eng") << QStringLiteral("la") << QStringLiteral("clear") << QString()
            << QStringLiteral("CUDA") << false;
    }

    void voicebankInferenceAndWaveExport() {
        QFETCH(QString, language);
        QFETCH(QString, lyric);
        QFETCH(QString, speakerId);
        QFETCH(QString, damagedStage);
        QFETCH(QString, providerId);
        QFETCH(bool, runtimeFailure);
        const bool repairModel = !damagedStage.isEmpty();
        const bool checksFailedExport =
            runtimeFailure && damagedStage == QStringLiteral("acoustic");
        const bool gpuProvider = providerId != QStringLiteral("CPU");
        const bool bundledVoicebank =
            repairModel || gpuProvider || TestSupport::usingBundledVoicebank();
        const auto configuredRoot = bundledVoicebank ? QString::fromUtf8(LITE_TEST_VOICEBANK_ROOT)
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
            if (runtimeFailure) {
                // G2P contexts must be registered before language models are initialized.
                const auto healthyRoot = fixture.filePath(QStringLiteral("healthy-voicebank"));
                std::filesystem::copy(std::filesystem::u8path(voicebankRoot.toUtf8().constData()),
                                      std::filesystem::u8path(healthyRoot.toUtf8().constData()),
                                      std::filesystem::copy_options::recursive, error);
                QVERIFY2(!error, qPrintable(QString::fromStdString(error.message())));
                QFile descriptor(QDir(healthyRoot).filePath(QStringLiteral("desc.json")));
                QVERIFY(descriptor.open(QIODevice::ReadOnly));
                auto metadata = QJsonDocument::fromJson(descriptor.readAll()).object();
                QVERIFY(!metadata.isEmpty());
                descriptor.close();
                metadata.insert(QStringLiteral("id"), QStringLiteral("ci-fixture-repaired"));
                metadata.insert(QStringLiteral("version"), QStringLiteral("1.0.1"));
                QVERIFY(descriptor.open(QIODevice::WriteOnly | QIODevice::Truncate));
                const auto encoded = QJsonDocument(metadata).toJson();
                QCOMPARE(descriptor.write(encoded), encoded.size());
            }
            voicebankRoot = copy;
            damagedModelPath =
                QDir(copy).filePath(QStringLiteral("inferences/%1/%1.onnx").arg(damagedStage));
            QFile model(damagedModelPath);
            QVERIFY(model.open(QIODevice::ReadOnly));
            originalModel = model.readAll();
            QVERIFY(!originalModel.isEmpty());
            model.close();
            QByteArray damagedModel("invalid ONNX");
            if (runtimeFailure) {
                QFile fault(
                    QDir(copy).filePath(QStringLiteral("test-errors/%1.onnx").arg(damagedStage)));
                QVERIFY(fault.open(QIODevice::ReadOnly));
                damagedModel = fault.readAll();
                QVERIFY(!damagedModel.isEmpty());
            }
            QVERIFY(model.open(QIODevice::WriteOnly | QIODevice::Truncate));
            QCOMPARE(model.write(damagedModel), damagedModel.size());
        }
        QVERIFY(QDir().mkpath(fixture.filePath(QStringLiteral("cache"))));
        QVERIFY(fixture.writeConfig({
            {QStringLiteral("general"),
             QJsonObject{{QStringLiteral("packageSearchPaths"),
                          QJsonArray{runtimeFailure ? fixture.path() : voicebankRoot}}}     },
            {QStringLiteral("inference"),
             QJsonObject{
                 {QStringLiteral("executionProvider"), providerId},
                 {QStringLiteral("autoStartInfer"), false},
                 {QStringLiteral("runVocoderOnCpu"), !gpuProvider},
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

        QVERIFY2(client.call(
                     QStringLiteral("settings.query"),
                     {
                         {QStringLiteral("domains"), QJsonArray{QStringLiteral("compute_device")}}
        },
                     result),
                 qPrintable(client.error));
        const auto computeSettings = result.value(QStringLiteral("domains"))
                                         .toObject()
                                         .value(QStringLiteral("compute_device"))
                                         .toObject();
        const auto availableProviders = computeSettings.value(QStringLiteral("candidates"))
                                            .toObject()
                                            .value(QStringLiteral("execution_providers"))
                                            .toArray();
        const bool providerInBuild = availableProviders.contains(QJsonValue(providerId));
        const auto activeProvider = computeSettings.value(QStringLiteral("configured"))
                                        .toObject()
                                        .value(QStringLiteral("execution_provider"))
                                        .toString();
        QVERIFY(!activeProvider.isEmpty());
        if (gpuProvider && (!providerInBuild || activeProvider != providerId)) {
            if (providerInBuild)
                QCOMPARE(activeProvider, QStringLiteral("CPU"));
            else
                QVERIFY(availableProviders.contains(QJsonValue(activeProvider)));
            QVERIFY2(client.call(QStringLiteral("application.request_exit"),
                                 {
                                     {QStringLiteral("discard_changes"), true}
            },
                                 result),
                     qPrintable(client.error));
            QVERIFY(editor.waitForFinished(15000));
            QCOMPARE(editor.exitStatus(), QProcess::NormalExit);
            QCOMPARE(editor.exitCode(), 0);
            const auto reason = providerInBuild
                                    ? QStringLiteral("%1 has no usable device; the editor used CPU")
                                    : QStringLiteral("%1 is not available in this build");
            QSKIP(qPrintable(reason.arg(providerId)));
        }
        QCOMPARE(activeProvider, providerId);

        const auto requestedSinger =
            bundledVoicebank ? QStringLiteral("fixture") : TestSupport::fixtureSingerId();
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
                if (runtimeFailure &&
                    singer.value(QStringLiteral("package_id")) != QStringLiteral("ci-fixture"))
                    continue;
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
        QJsonObject singer{
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
        if (bundledVoicebank) {
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
        QVERIFY2(client.call(QStringLiteral("inference.get_capabilities"),
                             {
                                 {QStringLiteral("document_id"), client.documentId},
                                 {QStringLiteral("scope"),       scope            }
        },
                             result),
                 qPrintable(client.error));
        const auto providers = result.value(QStringLiteral("capabilities"))
                                   .toObject()
                                   .value(QStringLiteral("providers"))
                                   .toArray();
        QVERIFY(std::any_of(providers.cbegin(), providers.cend(), [&](const QJsonValue &value) {
            const auto provider = value.toObject();
            return provider.value(QStringLiteral("id")).toString() == providerId &&
                   provider.value(QStringLiteral("available")).toBool();
        }));
        const auto verifyStageStatus = [&](const QString &stageId, const QString &expectedState) {
            QJsonObject status;
            QVERIFY2(client.call(QStringLiteral("inference.get_status"),
                                 {
                                     {QStringLiteral("document_id"), client.documentId},
                                     {QStringLiteral("scope"),       scope            },
            },
                                 status),
                     qPrintable(client.error));
            QJsonObject actualStage;
            for (const auto &value : status.value(QStringLiteral("status"))
                                         .toObject()
                                         .value(QStringLiteral("stages"))
                                         .toArray()) {
                const auto stage = value.toObject();
                if (stage.value(QStringLiteral("stage")) == stageId) {
                    actualStage = stage;
                    break;
                }
            }
            QVERIFY(!actualStage.isEmpty());
            QCOMPARE(actualStage.value(QStringLiteral("state")).toString(), expectedState);
            QVERIFY(actualStage.value(QStringLiteral("task_id")).isNull());
            QCOMPARE(actualStage.value(QStringLiteral("reason")).toString().isEmpty(),
                     expectedState == QStringLiteral("ready"));
        };
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

        QVERIFY2(client.mutate(QStringLiteral("inference.start"),
                               {
                                   {QStringLiteral("scope"),   scope                        },
                                   {QStringLiteral("options"),
                                    QJsonObject{{QStringLiteral("provider_id"), providerId}}},
        },
                               result),
                 qPrintable(client.error));
        if (repairModel) {
            QVERIFY2(client.waitForTask(result, false, 30000, QStringLiteral("failed")),
                     qPrintable(client.error));
            const auto failedStage = damagedStage == QStringLiteral("vocoder")
                                         ? QStringLiteral("acoustic")
                                         : damagedStage;
            verifyStageStatus(failedStage, QStringLiteral("failed"));
            if (QTest::currentTestFailed())
                return;
            const QDir cache(fixture.filePath(QStringLiteral("cache")));
            QVERIFY(cache.entryList({QStringLiteral("*.wav")}, QDir::Files).isEmpty());
            if (checksFailedExport) {
                const QByteArray originalExport("Keep the previous export");
                QFile existing(output);
                QVERIFY(existing.open(QIODevice::WriteOnly));
                QCOMPARE(existing.write(originalExport), originalExport.size());
                existing.close();
                QJsonObject documentBeforeExport;
                QVERIFY2(client.call(QStringLiteral("documents.get"),
                                     {
                                         {QStringLiteral("document_id"), client.documentId}
                },
                                     documentBeforeExport),
                         qPrintable(client.error));
                auto failedExportArguments = exportArguments;
                failedExportArguments.insert(QStringLiteral("overwrite_policy"),
                                             QStringLiteral("overwrite"));
                QVERIFY2(client.call(QStringLiteral("exports.audio.start"), failedExportArguments,
                                     result),
                         qPrintable(client.error));
                const auto failedExportId = result.value(QStringLiteral("task_id"));
                QVERIFY2(client.waitForTask(result, false, 30000, QStringLiteral("failed")),
                         qPrintable(client.error));
                QVERIFY2(client.call(QStringLiteral("tasks.get"),
                                     {
                                         {QStringLiteral("task_id"),     failedExportId            },
                                         {QStringLiteral("scope"),       QStringLiteral("document")},
                                         {QStringLiteral("document_id"), client.documentId         }
                },
                                     result),
                         qPrintable(client.error));
                const auto exportError = result.value(QStringLiteral("error")).toObject();
                QCOMPARE(exportError.value(QStringLiteral("code")).toString(),
                         QStringLiteral("io_error"));
                QVERIFY(!exportError.value(QStringLiteral("message")).toString().isEmpty());
                QVERIFY(existing.open(QIODevice::ReadOnly));
                QCOMPARE(existing.readAll(), originalExport);
                existing.close();
                QJsonObject documentAfterExport;
                QVERIFY2(client.call(QStringLiteral("documents.get"),
                                     {
                                         {QStringLiteral("document_id"), client.documentId}
                },
                                     documentAfterExport),
                         qPrintable(client.error));
                QCOMPARE(documentAfterExport, documentBeforeExport);
                QVERIFY(QDir(fixture.path())
                            .entryList({QStringLiteral("*.exporting")}, QDir::Files | QDir::Hidden)
                            .isEmpty());
            }
            if (runtimeFailure) {
                singer.insert(QStringLiteral("package_id"), QStringLiteral("ci-fixture-repaired"));
                singer.insert(QStringLiteral("package_version"), QStringLiteral("1.0.1"));
                QVERIFY2(client.mutate(QStringLiteral("tracks.set_voice"),
                                       {
                                           {QStringLiteral("track_id"), trackId              },
                                           {QStringLiteral("voice"),
                                            QJsonObject{{QStringLiteral("singer"), singer},
                                                        {QStringLiteral("speaker"), speaker}}}
                },
                                       result),
                         qPrintable(client.error));
                QVERIFY2(client.waitForInferenceModel(scope), qPrintable(client.error));
            } else {
                QFile model(damagedModelPath);
                QVERIFY(model.open(QIODevice::WriteOnly | QIODevice::Truncate));
                QCOMPARE(model.write(originalModel), originalModel.size());
            }
            QVERIFY2(client.mutate(QStringLiteral("inference.start"),
                                   {
                                       {QStringLiteral("scope"),   scope                        },
                                       {QStringLiteral("options"),
                                        QJsonObject{{QStringLiteral("provider_id"), providerId}}}
            },
                                   result),
                     qPrintable(client.error));
        }
        QVERIFY2(client.waitForTask(result, false, 300000), qPrintable(client.error));
        if (repairModel) {
            verifyStageStatus(damagedStage == QStringLiteral("vocoder") ? QStringLiteral("acoustic")
                                                                        : damagedStage,
                              QStringLiteral("ready"));
            if (QTest::currentTestFailed())
                return;
        }

        QVERIFY2(client.call(QStringLiteral("exports.audio.preview"), exportArguments, result),
                 qPrintable(client.error));
        bool reportsOverwrite = false;
        for (const auto &value : result.value(QStringLiteral("plan"))
                                     .toObject()
                                     .value(QStringLiteral("diagnostics"))
                                     .toArray()) {
            const auto diagnostic = value.toObject();
            if (diagnostic.value(QStringLiteral("code")) == QStringLiteral("will_overwrite")) {
                reportsOverwrite = true;
                QCOMPARE(diagnostic.value(QStringLiteral("blocking")).toBool(), checksFailedExport);
            } else {
                QVERIFY2(!diagnostic.value(QStringLiteral("blocking")).toBool(),
                         qPrintable(QString::fromUtf8(QJsonDocument(diagnostic).toJson())));
            }
        }
        QCOMPARE(reportsOverwrite, checksFailedExport);
        exportArguments.insert(QStringLiteral("overwrite_policy"), checksFailedExport
                                                                       ? QStringLiteral("overwrite")
                                                                       : QStringLiteral("reject"));
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
                             QJsonObject{{QStringLiteral("provider_id"), providerId}}}
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

            QVERIFY2(client.mutate(
                         QStringLiteral("tracks.insert"),
                         {
                             {QStringLiteral("index"),  1                                        },
                             {QStringLiteral("tracks"),
                              QJsonArray{QJsonObject{
                                  {QStringLiteral("client_ref"), QStringLiteral("destination")},
                                  {QStringLiteral("name"), QStringLiteral("Clear destination")}}}}
            },
                         result),
                     qPrintable(client.error));
            const auto destinationTrackId = createdId(result, QStringLiteral("destination"));
            QVERIFY(destinationTrackId > 0 && destinationTrackId != trackId);
            const QJsonObject destinationVoice{
                {QStringLiteral("singer"),  singer },
                {QStringLiteral("speaker"), speaker}
            };
            QVERIFY2(client.mutate(QStringLiteral("tracks.set_voice"),
                                   {
                                       {QStringLiteral("track_id"), destinationTrackId},
                                       {QStringLiteral("voice"),    destinationVoice  }
            },
                                   result),
                     qPrintable(client.error));
            const QJsonObject clipQuery{
                {QStringLiteral("document_id"), client.documentId},
                {QStringLiteral("clip_id"),     clipId           }
            };
            QVERIFY2(client.call(QStringLiteral("clips.get"), clipQuery, result),
                     qPrintable(client.error));
            const auto originalClip = result.value(QStringLiteral("snapshot")).toObject();
            const auto originalVoice = originalClip.value(QStringLiteral("voice_context"))
                                           .toObject()
                                           .value(QStringLiteral("effective_voice"))
                                           .toObject();
            QVERIFY2(client.call(QStringLiteral("notes.list"), clipQuery, result),
                     qPrintable(client.error));
            const auto originalNotes = result.value(QStringLiteral("notes")).toArray();
            QVERIFY(!originalNotes.isEmpty());
            const auto verifyPlacement = [&](const int expectedTrackId,
                                             const QJsonObject &expectedVoice) {
                QJsonObject current;
                QVERIFY2(client.call(QStringLiteral("clips.get"), clipQuery, current),
                         qPrintable(client.error));
                const auto snapshot = current.value(QStringLiteral("snapshot")).toObject();
                QCOMPARE(snapshot.value(QStringLiteral("clip_id")).toInt(), clipId);
                QCOMPARE(snapshot.value(QStringLiteral("track_id")).toInt(), expectedTrackId);
                QCOMPARE(snapshot.value(QStringLiteral("start")),
                         originalClip.value(QStringLiteral("start")));
                QCOMPARE(snapshot.value(QStringLiteral("length")),
                         originalClip.value(QStringLiteral("length")));
                const auto voiceContext =
                    snapshot.value(QStringLiteral("voice_context")).toObject();
                QVERIFY(voiceContext.value(QStringLiteral("inherits_track")).toBool());
                QVERIFY(voiceContext.value(QStringLiteral("own_voice")).isNull());
                QCOMPARE(voiceContext.value(QStringLiteral("effective_voice")).toObject(),
                         expectedVoice);
                QVERIFY2(client.call(QStringLiteral("notes.list"), clipQuery, current),
                         qPrintable(client.error));
                const auto notes = current.value(QStringLiteral("notes")).toArray();
                QCOMPARE(notes.size(), originalNotes.size());
                for (qsizetype i = 0; i < notes.size(); ++i) {
                    const auto note = notes.at(i).toObject();
                    const auto original = originalNotes.at(i).toObject();
                    QCOMPARE(note.value(QStringLiteral("note_id")),
                             original.value(QStringLiteral("note_id")));
                    QCOMPARE(note.value(QStringLiteral("local_start")),
                             original.value(QStringLiteral("local_start")));
                    QCOMPARE(note.value(QStringLiteral("length")),
                             original.value(QStringLiteral("length")));
                    QCOMPARE(note.value(QStringLiteral("lyric")),
                             original.value(QStringLiteral("lyric")));
                }
            };
            QVERIFY2(client.mutate(QStringLiteral("clips.move"),
                                   {
                                       {QStringLiteral("moves"),
                                        QJsonArray{QJsonObject{
                                            {QStringLiteral("clip_id"), clipId},
                                            {QStringLiteral("target_track_id"), destinationTrackId},
                                            {QStringLiteral("start"), 0}}}}
            },
                                   result),
                     qPrintable(client.error));
            verifyPlacement(destinationTrackId, destinationVoice);
            if (QTest::currentTestFailed())
                return;
            QVERIFY2(inferAndExport(QStringLiteral("cross-track.wav")), qPrintable(client.error));
            const auto movedAudio =
                decodeAudio(fixture.filePath(QStringLiteral("cross-track.wav")));
            QVERIFY2(movedAudio.error.isEmpty(), qPrintable(movedAudio.error));
            QCOMPARE(movedAudio.info.frames, fasterAudio.info.frames);
            QVERIFY(movedAudio.energy > 0);
            QVERIFY(movedAudio.pcm != fasterAudio.pcm);
            QVERIFY2(client.mutate(QStringLiteral("history.undo"), {}, result),
                     qPrintable(client.error));
            verifyPlacement(trackId, originalVoice);
            if (QTest::currentTestFailed())
                return;
            QVERIFY2(inferAndExport(QStringLiteral("cross-track-undo.wav")),
                     qPrintable(client.error));
            const auto restoredAudio =
                decodeAudio(fixture.filePath(QStringLiteral("cross-track-undo.wav")));
            QVERIFY2(restoredAudio.error.isEmpty(), qPrintable(restoredAudio.error));
            QCOMPARE(restoredAudio.pcm, fasterAudio.pcm);
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
