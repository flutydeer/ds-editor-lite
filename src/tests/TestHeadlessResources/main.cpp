#include "../TestSupport/ProcessFixture.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTcpServer>
#include <QTimer>
#include <QtTest>

#include <sndfile.h>

#include <array>
#include <cmath>
#include <memory>

namespace {
    class NativeClient final {
    public:
        NativeClient(QProcess &editor, const QUrl &endpoint) : editor(editor), endpoint(endpoint) {
        }

        bool call(const QString &operation, const QJsonObject &arguments, QJsonObject &result,
                  int timeoutMs = 5000) {
            error.clear();
            result = {};
            const auto id = QString::number(++sequence);
            const auto requestBytes =
                QJsonDocument(QJsonObject{
                                  {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                  {QStringLiteral("id"),      id                   },
                                  {QStringLiteral("method"),  operation            },
                                  {QStringLiteral("params"),  arguments            },
            })
                    .toJson(QJsonDocument::Compact);
            TestSupport::recordProcessMessage(editor, "request", requestBytes);
            QNetworkRequest request(endpoint);
            request.setHeader(QNetworkRequest::ContentTypeHeader,
                              QStringLiteral("application/json"));
            auto *reply = manager.post(request, requestBytes);
            QEventLoop loop;
            QTimer timeout;
            timeout.setSingleShot(true);
            QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
            QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
            timeout.start(timeoutMs);
            loop.exec();
            const bool finished = reply->isFinished();
            if (!finished)
                reply->abort();
            const auto bytes = reply->readAll();
            const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const auto networkError = reply->errorString();
            reply->deleteLater();
            TestSupport::recordProcessMessage(editor, "response", bytes);
            TestSupport::readProcessStdout(editor);
            TestSupport::readProcessStderr(editor);
            if (!finished || status != 200) {
                error = QStringLiteral("%1: HTTP %2, %3, response=%4")
                            .arg(operation)
                            .arg(status)
                            .arg(networkError, QString::fromUtf8(bytes));
                return false;
            }
            QJsonParseError parseError;
            const auto parsed = QJsonDocument::fromJson(bytes, &parseError);
            const auto response = parsed.object();
            if (parseError.error != QJsonParseError::NoError || !parsed.isObject() ||
                response.value(QStringLiteral("id")).toString() != id ||
                response.contains(QStringLiteral("error")) ||
                !response.value(QStringLiteral("result")).isObject()) {
                error = operation + QStringLiteral(": ") + QString::fromUtf8(bytes);
                return false;
            }
            result = response.value(QStringLiteral("result")).toObject();
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
            return call(operation, arguments, result);
        }

        bool waitForTask(const QJsonObject &accepted, bool applicationScope, int timeoutMs) {
            const auto taskId = accepted.value(QStringLiteral("task_id")).toString();
            if (taskId.isEmpty()) {
                error = QStringLiteral("Operation did not return a task_id: ") +
                        QString::fromUtf8(QJsonDocument(accepted).toJson(QJsonDocument::Compact));
                return false;
            }
            QJsonObject arguments{
                {QStringLiteral("task_id"), taskId}
            };
            if (applicationScope)
                arguments.insert(QStringLiteral("scope"), QStringLiteral("application"));
            else
                arguments.insert(QStringLiteral("document_id"), documentId);
            QElapsedTimer deadline;
            deadline.start();
            QJsonObject task;
            while (deadline.elapsed() < timeoutMs && editor.state() != QProcess::NotRunning) {
                if (!call(QStringLiteral("tasks.get"), arguments, task))
                    return false;
                const auto state = task.value(QStringLiteral("state")).toString();
                if (state == QStringLiteral("succeeded"))
                    return true;
                if (state == QStringLiteral("failed") || state == QStringLiteral("canceled")) {
                    error = QStringLiteral("Resource task did not succeed: ") +
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
}

class TestHeadlessResources final : public QObject {
    Q_OBJECT

public:
    QString editorPath;

private slots:

    void voicebankInferenceAndWaveExport() {
        const auto configuredRoot = qEnvironmentVariable("DSEL_TEST_VOICEBANK_ROOT");
        if (configuredRoot.isEmpty())
            QSKIP("Set DSEL_TEST_VOICEBANK_ROOT, DSEL_TEST_LANGUAGE and DSEL_TEST_LYRIC to run the "
                  "real CPU voicebank workflow");
        const auto language = qEnvironmentVariable("DSEL_TEST_LANGUAGE");
        const auto lyric = qEnvironmentVariable("DSEL_TEST_LYRIC");
        QVERIFY2(!language.isEmpty(),
                 "DSEL_TEST_LANGUAGE is required when a voicebank is configured");
        QVERIFY2(!lyric.isEmpty(), "DSEL_TEST_LYRIC is required when a voicebank is configured");
        const QFileInfo rootInfo(configuredRoot);
        QVERIFY2(rootInfo.isAbsolute() && rootInfo.isDir(),
                 "DSEL_TEST_VOICEBANK_ROOT must name an existing absolute directory");
        const auto voicebankRoot = rootInfo.canonicalFilePath();
        QVERIFY(!voicebankRoot.isEmpty());
        QVERIFY(QFileInfo::exists(editorPath));

        TestSupport::ProcessFixture fixture(QStringLiteral("headless-resources"));
        QVERIFY(fixture.isValid());
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

        const auto requestedSinger = qEnvironmentVariable("DSEL_TEST_SINGER_ID");
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
            voiceSnapshot.value(QStringLiteral("default_speaker_id")).toString();
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
        QVERIFY2(
            client.mutate(QStringLiteral("notes.insert"),
                          {
                              {QStringLiteral("clip_id"), clipId                                                 },
                              {QStringLiteral("notes"),   QJsonArray{QJsonObject{
                                                            {QStringLiteral("local_start"), 0},
                                                            {QStringLiteral("length"), 960},
                                                            {QStringLiteral("key_index"), 60},
                                                            {QStringLiteral("lyric"), lyric},
                                                            {QStringLiteral("language"), language},
                                                        }}},
        },
                          result),
            qPrintable(client.error));

        const QJsonObject scope{
            {QStringLiteral("kind"),     QStringLiteral("clip")},
            {QStringLiteral("clip_ids"), QJsonArray{clipId}    }
        };
        QVERIFY2(client.call(QStringLiteral("inference.get_capabilities"),
                             {
                                 {QStringLiteral("document_id"), client.documentId},
                                 {QStringLiteral("scope"),       scope            },
        },
                             result),
                 qPrintable(client.error));
        QVERIFY2(client.mutate(QStringLiteral("inference.start"),
                               {
                                   {QStringLiteral("scope"),   scope        },
                         {QStringLiteral("options"),
                          QJsonObject{{QStringLiteral("provider_id"), QStringLiteral("CPU")}}},
        },
                               result),
                 qPrintable(client.error));
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

        SF_INFO info{};
#ifdef Q_OS_WIN
        auto *opened =
            sf_wchar_open(reinterpret_cast<const wchar_t *>(output.utf16()), SFM_READ, &info);
#else
        auto *opened = sf_open(QFile::encodeName(output).constData(), SFM_READ, &info);
#endif
        const std::unique_ptr<SNDFILE, decltype(&sf_close)> audio(opened, sf_close);
        QVERIFY2(audio != nullptr, sf_strerror(nullptr));
        QVERIFY(info.frames > 0);
        QCOMPARE(info.samplerate, 44100);
        QCOMPARE(info.channels, 1);
        std::array<float, 4096> samples;
        double energy = 0;
        sf_count_t total = 0;
        while (const auto count = sf_read_float(audio.get(), samples.data(), samples.size())) {
            for (sf_count_t index = 0; index < count; ++index) {
                const auto sample = samples[static_cast<size_t>(index)];
                QVERIFY(std::isfinite(sample));
                energy += double(sample) * sample;
            }
            total += count;
        }
        QCOMPARE(sf_error(audio.get()), SF_ERR_NO_ERROR);
        QCOMPARE(total, info.frames);
        QVERIFY2(energy > 0, "Successful synthesis and export must produce non-silent audio");
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
    TestHeadlessResources test;
    auto arguments = application.arguments();
    const auto index = arguments.indexOf(QStringLiteral("--editor"));
    if (index >= 0 && index + 1 < arguments.size()) {
        arguments.removeAt(index);
        test.editorPath = arguments.takeAt(index);
    }
    return QTest::qExec(&test, arguments);
}

#include "main.moc"
