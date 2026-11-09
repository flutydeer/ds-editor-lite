#include "tst_bootstrap.h"
#include "Bootstrap/SingleInstanceCoordinator.h"
#include "Bootstrap/SingleInstanceIdentity.h"
#include "Bootstrap/SingleInstanceProtocol.h"
#include "Bootstrap/SecondaryHandoff.h"

#include <QCoreApplication>
#include <QtTest>
#include "../TestSupport/TestAssertions.h"
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QRegularExpression>
#include <QTimer>
#include <QLocalSocket>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QUuid>
#include <lite/ProductMetadata.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

namespace {
    using TestSupport::expect;

    bool waitUntil(const std::function<bool()> &condition, const int timeoutMs = 1000) {
        QElapsedTimer timer;
        timer.start();
        while (!condition() && timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents();
            QThread::msleep(1);
        }
        return condition();
    }

    SingleInstanceRequest openRequest(const QStringList &paths) {
        return {
            QUuid::createUuid().toString(QUuid::WithoutBraces),
            SingleInstanceCommand::OpenProjects,
            paths,
        };
    }

    SingleInstanceRequest automationRequest(const SingleInstanceCommand command,
                                            const QString &connectorId = {}) {
        SingleInstanceRequest request;
        request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        request.command = command;
        if (command == SingleInstanceCommand::AutomationWatch) {
            request.connector.instanceId = connectorId.isEmpty()
                                               ? QUuid::createUuid().toString(QUuid::WithoutBraces)
                                               : connectorId;
            request.connector.version = QStringLiteral("test-connector-1");
        }
        return request;
    }

    SingleInstanceAutomationStatus
        readyStatus(const SingleInstanceAutomationStatus &initial,
                    const QString &endpoint = QStringLiteral("http://127.0.0.1:52341/mcp")) {
        auto status = initial;
        status.state = SingleInstanceAutomationState::ServerReady;
        status.buildId = QStringLiteral("test-build-id");
        status.serverEnabled = true;
        status.serverEndpoint = endpoint;
        status.error.clear();
        return status;
    }

    class FramedClient final {
    public:
        bool connectTo(const QString &serverName, const int timeoutMs = 1000) {
            socket.connectToServer(serverName, QIODevice::ReadWrite);
            return socket.waitForConnected(timeoutMs);
        }

        bool send(const SingleInstanceRequest &request, const int timeoutMs = 1000) {
            const auto message =
                SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeRequest(request));
            return sendFrame(message, timeoutMs);
        }

        bool sendFrame(const QByteArray &message, const int timeoutMs = 1000) {
            if (socket.write(message) != message.size())
                return false;
            // A completed write need not produce another bytesWritten event to wait for.
            if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(timeoutMs))
                return socket.bytesToWrite() == 0;
            return true;
        }

        bool receive(QByteArray &payload, QString &error, const int timeoutMs = 1000) {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < timeoutMs) {
                error.clear();
                if (SingleInstanceProtocol::takeFrame(buffer, payload, error))
                    return true;
                if (!error.isEmpty())
                    return false;
                if (socket.bytesAvailable()) {
                    buffer.append(socket.readAll());
                    continue;
                }
                const auto remaining = timeoutMs - static_cast<int>(timer.elapsed());
                if (remaining <= 0 || !socket.waitForReadyRead(remaining)) {
                    if (socket.bytesAvailable())
                        buffer.append(socket.readAll());
                    else
                        break;
                }
            }
            error = QStringLiteral("Timed out waiting for a framed response");
            return false;
        }

        bool receiveSnapshot(SingleInstanceAutomationSnapshot &snapshot,
                             const int timeoutMs = 1000) {
            QByteArray payload;
            QString error;
            return receive(payload, error, timeoutMs) &&
                   SingleInstanceProtocol::decodeAutomationSnapshot(payload, snapshot, error);
        }

        bool waitForDisconnect(const int timeoutMs = 1000) {
            if (socket.state() == QLocalSocket::UnconnectedState)
                return true;
            socket.waitForDisconnected(timeoutMs);
            return socket.state() == QLocalSocket::UnconnectedState;
        }

        QLocalSocket socket;

    private:
        QByteArray buffer;
    };

    QString uniqueServerName() {
        return QStringLiteral("DsEditorLite-Test-%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    }

    // Drive the primary's main-thread metacalls while waiting for a framed
    // response. The IPC worker cannot acknowledge a request on its own any
    // more, so tests must let the main thread actually run.
    bool receiveWithEventLoop(FramedClient &client, QByteArray &payload, QString &error,
                              const int timeoutMs = 3000) {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeoutMs) {
            QCoreApplication::processEvents();
            if (client.receive(payload, error, 10))
                return true;
        }
        return false;
    }

    SingleInstanceRequest handoffRequest(const QStringList &paths) {
        auto request = openRequest(paths);
        request.command = SingleInstanceCommand::ApplicationHandoff;
        request.handoffVersion = SingleInstanceProtocol::handoffVersion;
        return request;
    }

    bool decodeState(const QByteArray &payload, SingleInstanceHandoffResponse &response,
                     QString &error) {
        return SingleInstanceProtocol::decodeHandoffResponse(payload, response, error);
    }

}

void BootstrapTests::initTestCase() {
    QCoreApplication::setOrganizationName(QStringLiteral("OpenVPI"));
    QCoreApplication::setApplicationName(QStringLiteral("DsEditorLiteBootstrapTest"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.2.3-test"));
}

void BootstrapTests::protocol() {
    const auto request =
        openRequest({QStringLiteral("C:/projects/a.dspx"), QStringLiteral("C:/projects/b.dspx")});
    const auto encoded = SingleInstanceProtocol::encodeRequest(request);
    SingleInstanceRequest decoded;
    QString error;
    expect(SingleInstanceProtocol::decodeRequest(encoded, decoded, error),
           "request payload must round-trip");
    expect(decoded.requestId == request.requestId, "request ID must survive serialization");
    expect(decoded.command == SingleInstanceCommand::OpenProjects,
           "request command must survive serialization");
    expect(decoded.paths == request.paths, "project paths must survive serialization");

    auto unsupportedObject = QJsonDocument::fromJson(encoded).object();
    unsupportedObject.insert(QStringLiteral("protocolVersion"),
                             SingleInstanceProtocol::protocolVersion + 1);
    SingleInstanceRequest unsupportedRequest;
    error.clear();
    expect(!SingleInstanceProtocol::decodeRequest(
               QJsonDocument(unsupportedObject).toJson(QJsonDocument::Compact), unsupportedRequest,
               error) &&
               unsupportedRequest.requestId == request.requestId,
           "unsupported protocol versions must retain the request ID for rejection");

    const auto framed = SingleInstanceProtocol::frame(encoded);
    QByteArray buffer = framed.left(3);
    QByteArray payload;
    error.clear();
    expect(!SingleInstanceProtocol::takeFrame(buffer, payload, error) && error.isEmpty(),
           "partial frame header must wait for more data");
    buffer.append(framed.mid(3));
    expect(SingleInstanceProtocol::takeFrame(buffer, payload, error),
           "complete frame must be extracted");
    expect(payload == encoded && buffer.isEmpty(),
           "frame extraction must consume exactly one frame");

    QByteArray oversized(4, '\0');
    const auto tooLarge = static_cast<quint32>(SingleInstanceProtocol::maxPayloadSize + 1);
    oversized[0] = static_cast<char>((tooLarge >> 24) & 0xff);
    oversized[1] = static_cast<char>((tooLarge >> 16) & 0xff);
    oversized[2] = static_cast<char>((tooLarge >> 8) & 0xff);
    oversized[3] = static_cast<char>(tooLarge & 0xff);
    error.clear();
    expect(!SingleInstanceProtocol::takeFrame(oversized, payload, error) && !error.isEmpty(),
           "oversized frames must be rejected");

    const auto watchRequest =
        automationRequest(SingleInstanceCommand::AutomationWatch, QStringLiteral("watcher"));
    SingleInstanceRequest decodedWatch;
    error.clear();
    expect(SingleInstanceProtocol::decodeRequest(
               SingleInstanceProtocol::encodeRequest(watchRequest), decodedWatch, error),
           "watch request must round-trip");
    expect(decodedWatch.command == SingleInstanceCommand::AutomationWatch &&
               decodedWatch.connector.instanceId == watchRequest.connector.instanceId &&
               decodedWatch.connector.version == watchRequest.connector.version,
           "watch request must retain connector identity");

    const auto malformedWatch = QByteArrayLiteral(
        R"({"protocolVersion":1,"requestId":"watch","command":"automation.watch"})");
    error.clear();
    expect(!SingleInstanceProtocol::decodeRequest(malformedWatch, decodedWatch, error),
           "watch request without connector identity must be rejected");

    for (const auto state : {
             SingleInstanceAutomationState::EditorStarting,
             SingleInstanceAutomationState::ServerDisabled,
             SingleInstanceAutomationState::ServerStarting,
             SingleInstanceAutomationState::ServerReady,
             SingleInstanceAutomationState::ServerStopping,
             SingleInstanceAutomationState::EditorStopping,
             SingleInstanceAutomationState::Error,
         }) {
        SingleInstanceAutomationState parsed;
        const auto name = SingleInstanceProtocol::automationStateName(state);
        expect(!name.isEmpty() && SingleInstanceProtocol::parseAutomationState(name, parsed) &&
                   parsed == state,
               "every automation state must round-trip");
    }

    SingleInstanceAutomationStatus status{
        SingleInstanceAutomationState::ServerReady,
        QStringLiteral("editor-instance"),
        QStringLiteral("C:/apps/DsEditorLite.exe"),
        QStringLiteral("1.2.3"),
        QStringLiteral("build-id"),
        QStringLiteral("gui"),
        true,
        QStringLiteral("http://127.0.0.1:52341/mcp"),
        {},
    };
    const SingleInstanceAutomationSnapshot stateEvent{
        QStringLiteral("state-request"),
        1234,
        status,
    };
    const auto encodedState = SingleInstanceProtocol::encodeAutomationSnapshot(stateEvent);
    SingleInstanceAutomationSnapshot decodedState;
    error.clear();
    expect(SingleInstanceProtocol::decodeAutomationSnapshot(encodedState, decodedState, error),
           "automation state snapshot must round-trip");
    expect(decodedState.requestId == stateEvent.requestId &&
               decodedState.primaryProcessId == stateEvent.primaryProcessId &&
               decodedState.result.state == stateEvent.result.state &&
               decodedState.result.editorInstanceId == stateEvent.result.editorInstanceId &&
               decodedState.result.executablePath == stateEvent.result.executablePath &&
               decodedState.result.applicationVersion == stateEvent.result.applicationVersion &&
               decodedState.result.buildId == stateEvent.result.buildId &&
               decodedState.result.hostMode == stateEvent.result.hostMode &&
               decodedState.result.serverEnabled == stateEvent.result.serverEnabled &&
               decodedState.result.serverEndpoint == stateEvent.result.serverEndpoint &&
               decodedState.result.error == stateEvent.result.error,
           "automation state snapshot must retain every field");

    buffer = SingleInstanceProtocol::frame(encodedState);
    buffer.append(SingleInstanceProtocol::frame(encodedState));
    error.clear();
    expect(SingleInstanceProtocol::takeFrame(buffer, payload, error) &&
               SingleInstanceProtocol::takeFrame(buffer, payload, error) && buffer.isEmpty(),
           "multiple frames in one buffer must be parsed independently");
}

void BootstrapTests::identity() {
    QTemporaryDir first;
    QTemporaryDir second;
    expect(first.isValid() && second.isValid(), "temporary identity directories must be available");
    if (!first.isValid() || !second.isValid())
        return;
    const auto firstName = SingleInstanceIdentity::serviceName(first.path());
    expect(!SingleInstanceIdentity::productIdentity().isEmpty() &&
               firstName == SingleInstanceIdentity::serviceName(first.path()),
           "single-instance service identity must be public and deterministic");
    expect(firstName != SingleInstanceIdentity::serviceName(second.path()),
           "different data directories must produce different service names");
    expect(SingleInstanceIdentity::lockFilePath(first.path())
               .startsWith(SingleInstanceIdentity::normalizeDataDirectory(first.path())),
           "lock path must use the shared normalized editor data directory");

    const auto previousOrganization = QCoreApplication::organizationName();
    const auto previousApplication = QCoreApplication::applicationName();
    QCoreApplication::setOrganizationName(QString::fromLatin1(LiteProductMetadata::Publisher));
    QCoreApplication::setApplicationName(QString::fromLatin1(LiteProductMetadata::ProductName));
    const auto editorAppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    expect(SingleInstanceIdentity::normalizeDataDirectory(
               SingleInstanceIdentity::defaultDataDirectory()) ==
               SingleInstanceIdentity::normalizeDataDirectory(editorAppData),
           "shared default data directory must preserve the editor v1 location");
    QCoreApplication::setOrganizationName(previousOrganization);
    QCoreApplication::setApplicationName(previousApplication);
}

void BootstrapTests::automationBootstrap() {
    QTemporaryDir directory;
    expect(directory.isValid(), "temporary bootstrap directory must be available");
    if (!directory.isValid())
        return;

    SingleInstanceCoordinator headless(AppHostMode::Headless);
    expect(headless.automationState().hostMode == QStringLiteral("headless"),
           "headless coordinator must publish its real host mode before listening");

    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    expect(primary.start() == SingleInstanceCoordinator::StartResult::Primary,
           "automation bootstrap coordinator must become primary");

    const auto initial = primary.automationState();
    expect(initial.state == SingleInstanceAutomationState::EditorStarting &&
               !initial.editorInstanceId.isEmpty() && !initial.executablePath.isEmpty() &&
               initial.hostMode == QStringLiteral("gui") && !initial.serverEnabled &&
               initial.serverEndpoint.isEmpty(),
           "primary must publish a complete starting snapshot");

    FramedClient discoverClient;
    const auto discoverRequest = automationRequest(SingleInstanceCommand::AutomationDiscover);
    expect(discoverClient.connectTo(serverName) && discoverClient.send(discoverRequest),
           "discover client must connect and send its request");
    SingleInstanceAutomationSnapshot discovered;
    expect(discoverClient.receiveSnapshot(discovered) &&
               discovered.requestId == discoverRequest.requestId &&
               discovered.result.editorInstanceId == initial.editorInstanceId &&
               discovered.result.state == SingleInstanceAutomationState::EditorStarting &&
               discovered.primaryProcessId == QCoreApplication::applicationPid(),
           "discover must return the current complete snapshot");
    expect(discoverClient.waitForDisconnect(), "discover connection must close after one snapshot");

    FramedClient firstWatcher;
    FramedClient secondWatcher;
    const auto firstWatchRequest =
        automationRequest(SingleInstanceCommand::AutomationWatch, QStringLiteral("watcher-a"));
    const auto secondWatchRequest =
        automationRequest(SingleInstanceCommand::AutomationWatch, QStringLiteral("watcher-b"));
    expect(firstWatcher.connectTo(serverName) && firstWatcher.send(firstWatchRequest) &&
               secondWatcher.connectTo(serverName) && secondWatcher.send(secondWatchRequest),
           "multiple watchers must connect independently");
    SingleInstanceAutomationSnapshot firstSnapshot;
    SingleInstanceAutomationSnapshot secondSnapshot;
    expect(firstWatcher.receiveSnapshot(firstSnapshot) &&
               secondWatcher.receiveSnapshot(secondSnapshot) &&
               firstSnapshot.requestId == firstWatchRequest.requestId &&
               secondSnapshot.requestId == secondWatchRequest.requestId,
           "each watcher must immediately receive its own initial snapshot");

    auto ready = readyStatus(initial);
    primary.updateAutomationState(ready);
    expect(firstWatcher.receiveSnapshot(firstSnapshot) &&
               secondWatcher.receiveSnapshot(secondSnapshot) && firstSnapshot.requestId.isEmpty() &&
               secondSnapshot.requestId.isEmpty() &&
               firstSnapshot.result.state == SingleInstanceAutomationState::ServerReady &&
               secondSnapshot.result.serverEndpoint == ready.serverEndpoint,
           "state updates must broadcast a full snapshot to every watcher");

    primary.broadcastAutomationState();
    expect(firstWatcher.receiveSnapshot(firstSnapshot) &&
               secondWatcher.receiveSnapshot(secondSnapshot) &&
               firstSnapshot.result.buildId == ready.buildId &&
               secondSnapshot.result.editorInstanceId == ready.editorInstanceId,
           "explicit broadcasts must retain all current state fields");

    auto starting = ready;
    starting.state = SingleInstanceAutomationState::ServerStarting;
    starting.serverEndpoint.clear();
    primary.updateAutomationState(starting);
    ready.serverEndpoint = QStringLiteral("http://127.0.0.1:52342/mcp");
    primary.updateAutomationState(ready);
    expect(firstWatcher.receiveSnapshot(firstSnapshot) &&
               firstSnapshot.result.state == SingleInstanceAutomationState::ServerStarting &&
               firstWatcher.receiveSnapshot(firstSnapshot) &&
               firstSnapshot.result.state == SingleInstanceAutomationState::ServerReady &&
               firstSnapshot.result.serverEndpoint == ready.serverEndpoint,
           "watch connection must preserve consecutive framed state updates");
    expect(secondWatcher.receiveSnapshot(secondSnapshot) &&
               secondSnapshot.result.state == SingleInstanceAutomationState::ServerStarting &&
               secondWatcher.receiveSnapshot(secondSnapshot) &&
               secondSnapshot.result.state == SingleInstanceAutomationState::ServerReady,
           "all watchers must preserve consecutive framed updates");

    firstWatcher.socket.disconnectFromServer();
    expect(firstWatcher.waitForDisconnect(), "a disconnected watcher must close independently");
    auto disabled = ready;
    disabled.state = SingleInstanceAutomationState::ServerDisabled;
    disabled.serverEnabled = false;
    disabled.serverEndpoint.clear();
    primary.updateAutomationState(disabled);
    expect(secondWatcher.receiveSnapshot(secondSnapshot) &&
               secondSnapshot.result.state == SingleInstanceAutomationState::ServerDisabled,
           "remaining watchers must continue after another watcher disconnects");

    primary.shutdown();
    expect(secondWatcher.receiveSnapshot(secondSnapshot) &&
               secondSnapshot.result.state == SingleInstanceAutomationState::EditorStopping,
           "shutdown must publish editor_stopping to live watchers");
    expect(secondWatcher.waitForDisconnect(), "shutdown must close every remaining watcher");
}

void BootstrapTests::watcherLimit() {
    QTemporaryDir directory;
    expect(directory.isValid(), "temporary watcher directory must be available");
    if (!directory.isValid())
        return;

    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    expect(primary.start() == SingleInstanceCoordinator::StartResult::Primary,
           "watcher-limit coordinator must become primary");

    std::vector<std::unique_ptr<FramedClient>> watchers;
    watchers.reserve(static_cast<std::size_t>(SingleInstanceProtocol::maxWatcherCount));
    for (qsizetype index = 0; index < SingleInstanceProtocol::maxWatcherCount; ++index) {
        auto watcher = std::make_unique<FramedClient>();
        const auto request = automationRequest(SingleInstanceCommand::AutomationWatch,
                                               QStringLiteral("limit-watcher-%1").arg(index));
        SingleInstanceAutomationSnapshot snapshot;
        const auto connected = watcher->connectTo(serverName) && watcher->send(request) &&
                               watcher->receiveSnapshot(snapshot);
        expect(connected, "watchers up to the configured limit must be accepted");
        if (!connected)
            break;
        watchers.push_back(std::move(watcher));
    }

    if (watchers.size() == static_cast<std::size_t>(SingleInstanceProtocol::maxWatcherCount)) {
        FramedClient overflow;
        const auto overflowRequest = automationRequest(SingleInstanceCommand::AutomationWatch,
                                                       QStringLiteral("overflow-watcher"));
        QByteArray payload;
        QString error;
        SingleInstanceResponse response;
        expect(overflow.connectTo(serverName) && overflow.send(overflowRequest) &&
                   overflow.receive(payload, error) &&
                   SingleInstanceProtocol::decodeResponse(payload, response, error) &&
                   !response.accepted && response.requestId == overflowRequest.requestId &&
                   response.error == QStringLiteral("too_many_requests"),
               "watchers above the configured limit must receive a stable rejection");
        expect(overflow.waitForDisconnect(), "an over-limit watcher connection must be closed");

        watchers.front()->socket.abort();
        watchers.front()->waitForDisconnect();
        QThread::msleep(50);

        FramedClient replacement;
        const auto replacementRequest = automationRequest(SingleInstanceCommand::AutomationWatch,
                                                          QStringLiteral("replacement-watcher"));
        SingleInstanceAutomationSnapshot replacementSnapshot;
        expect(replacement.connectTo(serverName) && replacement.send(replacementRequest) &&
                   replacement.receiveSnapshot(replacementSnapshot),
               "disconnecting a watcher must immediately release its slot");
    }

    primary.shutdown();
}

void BootstrapTests::initialReadTimeout() {
    QTemporaryDir directory;
    expect(directory.isValid(), "temporary initial-read-timeout directory must be available");
    if (!directory.isValid())
        return;

    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    expect(primary.start() == SingleInstanceCoordinator::StartResult::Primary,
           "initial-read-timeout coordinator must become primary");

    const auto unsupported = automationRequest(SingleInstanceCommand::AutomationDiscover);
    auto unsupportedObject =
        QJsonDocument::fromJson(SingleInstanceProtocol::encodeRequest(unsupported)).object();
    unsupportedObject.insert(QStringLiteral("protocolVersion"),
                             SingleInstanceProtocol::protocolVersion + 1);
    FramedClient unsupportedClient;
    const auto unsupportedFrame = SingleInstanceProtocol::frame(
        QJsonDocument(unsupportedObject).toJson(QJsonDocument::Compact));
    QByteArray responsePayload;
    QString error;
    SingleInstanceResponse response;
    expect(unsupportedClient.connectTo(serverName) &&
               unsupportedClient.sendFrame(unsupportedFrame) &&
               unsupportedClient.receive(responsePayload, error) &&
               SingleInstanceProtocol::decodeResponse(responsePayload, response, error) &&
               !response.accepted && response.requestId == unsupported.requestId,
           "unsupported bootstrap versions must echo the original request ID");

    std::vector<std::unique_ptr<QLocalSocket>> stalledClients;
    stalledClients.reserve(static_cast<std::size_t>(SingleInstanceProtocol::maxConnectionCount));
    for (qsizetype index = 0; index < SingleInstanceProtocol::maxConnectionCount; ++index) {
        auto socket = std::make_unique<QLocalSocket>();
        socket->connectToServer(serverName, QIODevice::ReadWrite);
        const auto connected = socket->waitForConnected(1000);
        expect(connected, "stalled bootstrap clients must fill every connection slot");
        if (!connected)
            break;
        if (index % 2 != 0) {
            QByteArray partialFrame(4, '\0');
            partialFrame[3] = static_cast<char>(16);
            partialFrame.append('{');
            socket->write(partialFrame);
            socket->waitForBytesWritten(1000);
        }
        stalledClients.push_back(std::move(socket));
        QCoreApplication::processEvents();
    }

    const auto allDisconnected = waitUntil(
        [&] {
            return std::all_of(stalledClients.cbegin(), stalledClients.cend(),
                               [](const auto &socket) {
                                   return socket->state() == QLocalSocket::UnconnectedState;
                               });
        },
        SingleInstanceProtocol::initialReadTimeoutMs + 2000);
    expect(allDisconnected,
           "idle and partial bootstrap frames must be dropped after a fixed timeout");

    FramedClient replacement;
    const auto replacementRequest = automationRequest(SingleInstanceCommand::AutomationDiscover);
    SingleInstanceAutomationSnapshot snapshot;
    const auto replacementDiagnostic = [&] {
        return QStringLiteral("state=%1 pending=%2 available=%3 socket_error=%4")
            .arg(int(replacement.socket.state()))
            .arg(replacement.socket.bytesToWrite())
            .arg(replacement.socket.bytesAvailable())
            .arg(replacement.socket.errorString());
    };
    const auto connected = replacement.connectTo(serverName);
    QVERIFY2(connected, qPrintable(replacementDiagnostic()));
    const auto sent = replacement.send(replacementRequest);
    QVERIFY2(sent, qPrintable(replacementDiagnostic()));
    const auto received = replacement.receive(responsePayload, error);
    QVERIFY2(received, qPrintable(error + QStringLiteral("; ") + replacementDiagnostic()));
    const auto decoded =
        SingleInstanceProtocol::decodeAutomationSnapshot(responsePayload, snapshot, error);
    QVERIFY2(decoded, qPrintable(error));
    QCOMPARE(snapshot.requestId, replacementRequest.requestId);

    primary.shutdown();
}

void BootstrapTests::coordinator_data() {
    QTest::addColumn<bool>("blockedDataDirectory");
    QTest::newRow("create-data-directory") << false;
    QTest::newRow("recover-blocked-data-directory") << true;
}

void BootstrapTests::coordinator() {
    QFETCH(bool, blockedDataDirectory);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const auto serverName = uniqueServerName();
    const auto dataPath = directory.filePath(QStringLiteral("application-data"));
    SingleInstanceCoordinator primary(dataPath, serverName);
    if (blockedDataDirectory) {
        QFile obstruction(dataPath);
        QVERIFY(obstruction.open(QIODevice::WriteOnly));
        QCOMPARE(obstruction.write("owned test file"), qint64(15));
        obstruction.close();
        QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Error);
        QVERIFY(!primary.errorString().isEmpty());
        QVERIFY(primary.errorString().contains(dataPath));
        QVERIFY(obstruction.remove());
    }
    QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Primary);
    QVERIFY(QDir(dataPath).exists());

    SingleInstanceCoordinator secondary(dataPath, serverName);
    expect(secondary.start() == SingleInstanceCoordinator::StartResult::Secondary,
           "second coordinator must detect the primary");

    // IPC receipt is not a successful handoff. Until the main thread admits the
    // request, a legacy client must not receive an accepted ACK.
    const auto firstRequest = openRequest({QDir(directory.path()).filePath("first.dspx")});
    FramedClient earlyClient;
    expect(earlyClient.connectTo(serverName) && earlyClient.send(firstRequest),
           "early client must connect and send");
    QByteArray payload;
    QString error;
    expect(!earlyClient.receive(payload, error, 150),
           "bytes received by the IPC worker must not acknowledge a legacy request");

    QList<SingleInstanceRequest> received;
    primary.setRequestHandler(
        [&received](const SingleInstanceRequest &request) { received.append(request); });
    primary.enableAdmission();
    expect(receiveWithEventLoop(earlyClient, payload, error),
           "the main thread must be driven to admit the first request");
    SingleInstanceResponse firstResponse;
    expect(SingleInstanceProtocol::decodeResponse(payload, firstResponse, error),
           "legacy response format must be preserved");
    expect(firstResponse.accepted && firstResponse.requestId == firstRequest.requestId,
           "ACK must arrive only after the main thread took the request");
    expect(received.size() == 1 && received.first().requestId == firstRequest.requestId,
           "the request must reach the installed handler exactly once");

    // A second request is acknowledged only after the same takeover boundary.
    const auto secondRequest = openRequest({QDir(directory.path()).filePath("second.dspx")});
    FramedClient secondClient;
    expect(secondClient.connectTo(serverName) && secondClient.send(secondRequest),
           "second client must connect and send");
    expect(receiveWithEventLoop(secondClient, payload, error),
           "second request must be acknowledged after main-thread takeover");
    SingleInstanceResponse secondResponse;
    expect(SingleInstanceProtocol::decodeResponse(payload, secondResponse, error) &&
               secondResponse.accepted && secondResponse.requestId == secondRequest.requestId,
           "second legacy request must retain its v1 response format");
    expect(received.size() == 2 && received.last().requestId == secondRequest.requestId,
           "two distinct requests must reach the handler exactly once each");

    // The Headless readiness barrier still defers dispatch.
    primary.pauseRequestDispatchAndFlush();
    const auto deferredRequest = openRequest({QDir(directory.path()).filePath("deferred.dspx")});
    FramedClient deferredClient;
    expect(deferredClient.connectTo(serverName) && deferredClient.send(deferredRequest),
           "request client must connect while dispatch is paused");
    expect(!deferredClient.receive(payload, error, 100),
           "a paused request must not be acknowledged");
    primary.resumeRequestDispatch();
    SingleInstanceResponse deferredResponse;
    expect(receiveWithEventLoop(deferredClient, payload, error) &&
               SingleInstanceProtocol::decodeResponse(payload, deferredResponse, error) &&
               deferredResponse.accepted && deferredResponse.requestId == deferredRequest.requestId,
           "paused request must be acknowledged after dispatch resumes");
    expect(received.size() == 3 && received.last().requestId == deferredRequest.requestId,
           "resumed request must reach the installed handler");

    // Legacy activate keeps its request shape and is still deferred to takeover.
    SingleInstanceRequest activateRequest;
    activateRequest.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    activateRequest.command = SingleInstanceCommand::Activate;
    FramedClient activateClient;
    expect(activateClient.connectTo(serverName) && activateClient.send(activateRequest),
           "legacy activate request must remain supported");
    expect(receiveWithEventLoop(activateClient, payload, error),
           "legacy activate must be acknowledged after takeover");
    expect(received.size() == 4 && received.last().command == SingleInstanceCommand::Activate &&
               received.last().paths.isEmpty(),
           "legacy activate request must retain its v1 behavior");

    primary.shutdown();
    SingleInstanceCoordinator replacement(dataPath, serverName);
    expect(replacement.start() == SingleInstanceCoordinator::StartResult::Primary,
           "a new coordinator must take ownership after primary shutdown");
    replacement.shutdown();
}

void BootstrapTests::forwardRequestReportsPeerFailureAndRecovers_data() {
    QTest::addColumn<QString>("reason");
    QTest::addColumn<bool>("mismatchedResponse");
    QTest::addColumn<bool>("disconnectBeforeAck");
    QTest::newRow("rejected-with-reason")
        << QStringLiteral("The primary cannot open this project") << false << false;
    QTest::newRow("rejected-without-reason") << QString() << false << false;
    QTest::newRow("mismatched-confirmation") << QString() << true << false;
    QTest::newRow("lost-confirmation") << QString() << false << true;
}

void BootstrapTests::forwardRequestReportsPeerFailureAndRecovers() {
    QFETCH(QString, reason);
    QFETCH(bool, mismatchedResponse);
    QFETCH(bool, disconnectBeforeAck);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto serverName = uniqueServerName();
    QLocalServer server;
    QVERIFY2(server.listen(serverName), qPrintable(server.errorString()));
    const QList<SingleInstanceRequest> expected{
        openRequest({directory.filePath(QStringLiteral("first.dspx"))}),
        openRequest({directory.filePath(QStringLiteral("recovered.dspx"))}),
    };
    QList<SingleInstanceRequest> received;
    QString peerError;
    QObject::connect(&server, &QLocalServer::newConnection, &server, [&] {
        auto *socket = server.nextPendingConnection();
        if (!socket)
            return;
        const auto buffer = std::make_shared<QByteArray>();
        const auto readRequest = [&, socket, buffer] {
            buffer->append(socket->readAll());
            QByteArray payload;
            QString error;
            if (!SingleInstanceProtocol::takeFrame(*buffer, payload, error)) {
                if (!error.isEmpty()) {
                    peerError = error;
                    socket->disconnectFromServer();
                }
                return;
            }
            SingleInstanceRequest request;
            if (!SingleInstanceProtocol::decodeRequest(payload, request, error)) {
                peerError = error;
                socket->disconnectFromServer();
                return;
            }
            received.append(request);
            if (received.size() == 1 && disconnectBeforeAck) {
                socket->disconnectFromServer();
                return;
            }
            SingleInstanceResponse response{
                request.requestId, true, {}, QCoreApplication::applicationPid()};
            if (received.size() == 1) {
                response.accepted = mismatchedResponse;
                response.error = reason;
                if (mismatchedResponse)
                    response.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            }
            const auto frame =
                SingleInstanceProtocol::frame(SingleInstanceProtocol::encodeResponse(response));
            if (socket->write(frame) != frame.size())
                peerError = socket->errorString();
            socket->disconnectFromServer();
        };
        QObject::connect(socket, &QLocalSocket::readyRead, &server, readRequest);
        readRequest();
    });
    bool forwarded = false;
    bool recovered = false;
    QString forwardError;
    QString recoveryError;
    // Keep the peer's event loop running while the coordinator waits for an ACK.
    const auto clientThread = std::unique_ptr<QThread>(QThread::create([&] {
        SingleInstanceCoordinator secondary(directory.path(), serverName);
        forwarded = secondary.forwardRequest(expected.at(0), forwardError);
        recovered = secondary.forwardRequest(expected.at(1), recoveryError);
    }));
    const auto joinClient = qScopeGuard([&] {
        if (clientThread->isRunning())
            clientThread->wait(7000);
    });
    clientThread->start();
    QTRY_VERIFY_WITH_TIMEOUT(clientThread->isFinished(), 8000);
    QVERIFY(clientThread->wait(1000));
    QVERIFY2(peerError.isEmpty(), qPrintable(peerError));
    QCOMPARE(received.size(), expected.size());
    for (qsizetype i = 0; i < expected.size(); ++i) {
        QCOMPARE(received.at(i).requestId, expected.at(i).requestId);
        QCOMPARE(received.at(i).command, expected.at(i).command);
        QCOMPARE(received.at(i).paths, expected.at(i).paths);
    }
    QVERIFY(!forwarded);
    QVERIFY(!forwardError.isEmpty());
    if (!reason.isEmpty())
        QCOMPARE(forwardError, reason);
    QVERIFY2(recovered, qPrintable(recoveryError));
    QVERIFY(recoveryError.isEmpty());
}

void BootstrapTests::queuedStartupConnectionIsAcknowledged() {
    // Old behavior asserted an early ACK while the main thread did not process
    // events. The new contract is the opposite: receipt and takeover are two
    // distinct states, and only Taken lets a secondary exit successfully.
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto serverName = uniqueServerName();
    const auto request = handoffRequest({directory.filePath("startup.dspx")});

    SingleInstanceCoordinator primary(directory.path(), serverName);
    QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Primary);

    FramedClient client;
    expect(client.connectTo(serverName) && client.send(request),
           "handoff client must connect before the primary is ready");
    QByteArray payload;
    QString error;
    expect(receiveWithEventLoop(client, payload, error, 1000),
           "the IPC worker must report receipt even while the main thread is stalled");
    SingleInstanceHandoffResponse receipt;
    expect(decodeState(payload, receipt, error), "receipt must use the handoff response format");
    expect(receipt.state == HandoffRequestState::Received,
           "receipt proves only that the IPC worker is alive");
    expect(receipt.requestId == request.requestId, "receipt must carry the request ID");
    expect(receipt.handoffVersion == SingleInstanceProtocol::handoffVersion,
           "receipt must advertise the handoff version");
    expect(!client.receive(payload, error, 150),
           "no terminal state may be published before main-thread takeover");

    QList<SingleInstanceRequest> received;
    primary.setRequestHandler([&](const auto &pending) { received.append(pending); });
    primary.enableAdmission();
    expect(receiveWithEventLoop(client, payload, error),
           "the primary must publish a terminal state after takeover");
    SingleInstanceHandoffResponse terminal;
    expect(decodeState(payload, terminal, error), "terminal state must decode");
    expect(terminal.state == HandoffRequestState::Taken,
           "Taken is the only successful terminal state");
    expect(received.size() == 1, "the handoff must be admitted exactly once");
    expect(received.first().requestId == request.requestId &&
               received.first().paths == request.paths,
           "the admitted request must match the submitted payload");

    // Same UUID and payload must replay the existing state, never enqueue twice.
    FramedClient replay;
    expect(replay.connectTo(serverName) && replay.send(request),
           "duplicate submission must connect");
    expect(receiveWithEventLoop(replay, payload, error),
           "duplicate submission must return the existing state");
    SingleInstanceHandoffResponse replayResponse;
    expect(decodeState(payload, replayResponse, error) &&
               replayResponse.state == HandoffRequestState::Taken,
           "duplicate submission must replay Taken, not enqueue again");
    expect(received.size() == 1, "duplicate submission must not reach the handler twice");
}

void BootstrapTests::handoffProtocolRoundTrips() {
    const auto request = handoffRequest({QStringLiteral("C:/projects/a.dspx")});
    const auto encoded = SingleInstanceProtocol::encodeRequest(request);
    SingleInstanceRequest decoded;
    QString error;
    expect(SingleInstanceProtocol::decodeRequest(encoded, decoded, error),
           "handoff request must round-trip");
    expect(decoded.command == SingleInstanceCommand::ApplicationHandoff,
           "handoff command must survive serialization");
    expect(decoded.handoffVersion == SingleInstanceProtocol::handoffVersion,
           "handoff version must survive serialization");
    expect(decoded.paths == request.paths, "handoff paths must survive serialization");

    SingleInstanceHandoffResponse response;
    response.requestId = request.requestId;
    response.instanceId = QStringLiteral("instance-uuid");
    response.primaryProcessId = 4242;
    response.hostMode = QStringLiteral("Gui");
    response.stage = PrimaryStartupStage::Initializing;
    response.heartbeatAgeMs = 3500;
    response.state = HandoffRequestState::Pending;
    response.error = QStringLiteral("waiting");
    response.handoffVersion = SingleInstanceProtocol::handoffVersion;
    const auto responseFrame = SingleInstanceProtocol::encodeHandoffResponse(response);
    SingleInstanceHandoffResponse decodedResponse;
    expect(SingleInstanceProtocol::decodeHandoffResponse(responseFrame, decodedResponse, error),
           "handoff response must round-trip");
    expect(decodedResponse.requestId == request.requestId &&
               decodedResponse.instanceId == response.instanceId &&
               decodedResponse.primaryProcessId == response.primaryProcessId &&
               decodedResponse.hostMode == response.hostMode &&
               decodedResponse.stage == response.stage &&
               decodedResponse.heartbeatAgeMs == response.heartbeatAgeMs &&
               decodedResponse.state == response.state && decodedResponse.error == response.error &&
               decodedResponse.handoffVersion == response.handoffVersion,
           "all handoff response fields must survive serialization");

    // A legacy response must not be mistaken for a handoff state.
    const SingleInstanceResponse legacy{request.requestId, true, {}, 4242};
    const auto legacyFrame = SingleInstanceProtocol::encodeResponse(legacy);
    SingleInstanceHandoffResponse notAHandoff;
    error.clear();
    expect(!SingleInstanceProtocol::decodeHandoffResponse(legacyFrame, notAHandoff, error),
           "a legacy ACK must not decode as a handoff state");
    SingleInstanceResponse decodedLegacy;
    error.clear();
    expect(SingleInstanceProtocol::decodeResponse(legacyFrame, decodedLegacy, error) &&
               decodedLegacy.accepted,
           "legacy response format must remain decodable");

    // Digest stability and payload conflict detection.
    auto conflicting = request;
    conflicting.paths = QStringList{QStringLiteral("C:/projects/b.dspx")};
    expect(SingleInstanceProtocol::requestDigest(request) !=
               SingleInstanceProtocol::requestDigest(conflicting),
           "different payloads must produce different digests");
    expect(SingleInstanceProtocol::requestDigest(request) ==
               SingleInstanceProtocol::requestDigest(request),
           "the same payload must produce a stable digest");

    SingleInstanceRequest cancel;
    cancel.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cancel.command = SingleInstanceCommand::ApplicationCancel;
    cancel.targetRequestId = request.requestId;
    SingleInstanceRequest decodedCancel;
    error.clear();
    expect(SingleInstanceProtocol::decodeRequest(
               SingleInstanceProtocol::encodeRequest(cancel), decodedCancel, error) &&
               decodedCancel.targetRequestId == request.requestId,
           "cancel target must survive serialization");
}

void BootstrapTests::handoffWaitsForMainThreadTakeover() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Primary);

    const auto request = handoffRequest({directory.filePath("stuck.dspx")});
    FramedClient client;
    expect(client.connectTo(serverName) && client.send(request),
           "client must connect while the primary main thread is stalled");
    QByteArray payload;
    QString error;
    expect(receiveWithEventLoop(client, payload, error, 1000), "receipt must arrive");
    SingleInstanceHandoffResponse receipt;
    expect(decodeState(payload, receipt, error) &&
               receipt.state == HandoffRequestState::Received,
           "a stalled main thread must not produce Taken");
    // The primary owns the lock and answers IPC, but no handler is installed.
    // Simulate the stalled window: do not enable admission at all.
    expect(!client.receive(payload, error, 250),
           "a secondary must keep waiting instead of exiting successfully");

    // After the main thread recovers, the queued request is taken exactly once.
    QList<SingleInstanceRequest> received;
    primary.setRequestHandler([&](const auto &pending) { received.append(pending); });
    primary.enableAdmission();
    expect(receiveWithEventLoop(client, payload, error),
           "recovery must publish a terminal state");
    SingleInstanceHandoffResponse terminal;
    expect(decodeState(payload, terminal, error) &&
               terminal.state == HandoffRequestState::Taken,
           "recovery must end in Taken");
    expect(received.size() == 1 && received.first().requestId == request.requestId,
           "the queued request must be admitted exactly once after recovery");
}

void BootstrapTests::handoffLedgerDedupeCancelAndQuery() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Primary);

    const auto request = handoffRequest({directory.filePath("cancel.dspx")});
    // Cancel before the main thread takes over.
    FramedClient pendingClient;
    expect(pendingClient.connectTo(serverName) && pendingClient.send(request),
           "pending client must connect");
    QByteArray payload;
    QString error;
    expect(receiveWithEventLoop(pendingClient, payload, error, 1000),
           "pending request must report receipt");

    SingleInstanceRequest cancel;
    cancel.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    cancel.command = SingleInstanceCommand::ApplicationCancel;
    cancel.targetRequestId = request.requestId;
    FramedClient cancelClient;
    expect(cancelClient.connectTo(serverName) && cancelClient.send(cancel),
           "cancel client must connect");
    expect(receiveWithEventLoop(cancelClient, payload, error),
           "cancel must receive a state response");
    SingleInstanceHandoffResponse cancelResponse;
    expect(decodeState(payload, cancelResponse, error) &&
               cancelResponse.state == HandoffRequestState::Cancelled,
           "an unclaimed request must be cancellable");

    // Query must recover the terminal state instead of replaying the open.
    SingleInstanceRequest query;
    query.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.command = SingleInstanceCommand::ApplicationQuery;
    query.targetRequestId = request.requestId;
    FramedClient queryClient;
    expect(queryClient.connectTo(serverName) && queryClient.send(query),
           "query client must connect");
    expect(receiveWithEventLoop(queryClient, payload, error),
           "query must receive a state response");
    SingleInstanceHandoffResponse queryResponse;
    expect(decodeState(payload, queryResponse, error) &&
               queryResponse.state == HandoffRequestState::Cancelled,
           "query must recover the terminal state");

    // Same UUID with a different payload is rejected, never enqueued.
    auto conflicting = request;
    conflicting.paths = QStringList{directory.filePath("other.dspx")};
    FramedClient conflictClient;
    expect(conflictClient.connectTo(serverName) && conflictClient.send(conflicting),
           "conflicting client must connect");
    expect(receiveWithEventLoop(conflictClient, payload, error),
           "conflicting payload must be answered");
    SingleInstanceHandoffResponse conflictResponse;
    expect(decodeState(payload, conflictResponse, error) &&
               conflictResponse.state == HandoffRequestState::Rejected &&
               conflictResponse.error ==
                   QStringLiteral("duplicate_request_id_with_different_payload"),
           "a reused UUID with a different payload must be rejected");

    // An unknown ID returns result_unknown rather than promising exactly-once.
    query.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    query.targetRequestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    FramedClient unknownClient;
    expect(unknownClient.connectTo(serverName) && unknownClient.send(query),
           "unknown query must connect");
    expect(receiveWithEventLoop(unknownClient, payload, error),
           "unknown query must be answered");
    SingleInstanceHandoffResponse unknownResponse;
    expect(decodeState(payload, unknownResponse, error) &&
               unknownResponse.state == HandoffRequestState::Unknown,
           "an unknown request ID must report result_unknown");
}

void BootstrapTests::handoffCancelAfterDispatchIsNotConfirmed() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Primary);

    const auto request = handoffRequest({directory.filePath("dispatched.dspx")});
    FramedClient handoffClient;
    expect(handoffClient.connectTo(serverName) && handoffClient.send(request),
           "handoff client must connect");
    QByteArray payload;
    QString error;
    expect(receiveWithEventLoop(handoffClient, payload, error, 1000),
           "handoff must report receipt");
    SingleInstanceHandoffResponse receipt;
    expect(decodeState(payload, receipt, error) &&
               receipt.state == HandoffRequestState::Received,
           "receipt must precede any dispatch");

    QList<SingleInstanceRequest> received;
    primary.setRequestHandler([&](const auto &pending) { received.append(pending); });
    // Admission dispatches to the main thread, but this test deliberately does
    // not run main-thread events yet: the request is dispatched, not taken.
    primary.enableAdmission();

    // Run the cancel from a worker thread so the test's main thread cannot
    // accidentally service the queued admitRequest metacall.
    bool cancelAnswered = false;
    SingleInstanceHandoffResponse cancelResponse;
    QString cancelError;
    const auto cancelThread = std::unique_ptr<QThread>(QThread::create([&] {
        SingleInstanceRequest cancel;
        cancel.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        cancel.command = SingleInstanceCommand::ApplicationCancel;
        cancel.targetRequestId = request.requestId;
        FramedClient client;
        if (!client.connectTo(serverName) || !client.send(cancel)) {
            cancelError = QStringLiteral("cancel client could not send");
            return;
        }
        QByteArray responsePayload;
        QString responseError;
        if (client.receive(responsePayload, responseError, 2000) &&
            SingleInstanceProtocol::decodeHandoffResponse(responsePayload, cancelResponse,
                                                          responseError))
            cancelAnswered = true;
        else
            cancelError = responseError;
    }));
    cancelThread->start();
    QVERIFY(cancelThread->wait(6000));
    expect(cancelAnswered, cancelError.isEmpty() ? QStringLiteral("no cancel response")
                                                 : cancelError);
    expect(cancelResponse.state == HandoffRequestState::Pending,
           "cancelling a dispatched-but-untaken request must report Pending");
    expect(cancelResponse.error == QStringLiteral("request_already_dispatched"),
           "the primary must explain why cancellation was refused");
    expect(SingleInstanceProtocol::cancelDisposition(cancelResponse.state) ==
               HandoffCancelDisposition::NotConfirmed,
           "Pending must never be interpreted as a confirmed cancellation");
    expect(SingleInstanceProtocol::cancelDisposition(HandoffRequestState::Cancelled) ==
               HandoffCancelDisposition::Cancelled,
           "only an explicit Cancelled is a confirmed cancellation");
    expect(SingleInstanceProtocol::cancelDisposition(HandoffRequestState::Taken) ==
               HandoffCancelDisposition::AlreadyTaken,
           "a taken request must not be reported as cancelled");

    // Once the main thread recovers, the original request is still taken once.
    expect(receiveWithEventLoop(handoffClient, payload, error),
           "the original handoff must still reach a terminal state");
    SingleInstanceHandoffResponse terminal;
    expect(decodeState(payload, terminal, error) &&
               terminal.state == HandoffRequestState::Taken,
           "an unconfirmed cancel must not stop the original takeover");
    expect(received.size() == 1, "the request must be admitted exactly once");
}

void BootstrapTests::handoffTerminalLedgerEvictsPredictably() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Primary);
    QList<SingleInstanceRequest> received;
    primary.setRequestHandler([&](const auto &pending) { received.append(pending); });
    primary.enableAdmission();

    const auto total = static_cast<int>(SingleInstanceProtocol::maxTerminalRecords) + 4;
    QStringList ids;
    ids.reserve(total);
    for (int index = 0; index < total; ++index) {
        auto request = handoffRequest(
            {directory.filePath(QStringLiteral("project-%1.dspx").arg(index))});
        ids.append(request.requestId);
        FramedClient client;
        expect(client.connectTo(serverName) && client.send(request),
               "each handoff client must connect and send");
        QByteArray payload;
        QString error;
        expect(receiveWithEventLoop(client, payload, error),
               "each handoff must report a state");
        SingleInstanceHandoffResponse state;
        expect(decodeState(payload, state, error), "each state must decode");
        if (state.state == HandoffRequestState::Received ||
            state.state == HandoffRequestState::Pending) {
            expect(receiveWithEventLoop(client, payload, error),
                   "a receipt must be followed by a terminal state");
            expect(decodeState(payload, state, error), "terminal state must decode");
        }
        expect(state.state == HandoffRequestState::Taken,
               "every admitted request must end in Taken");
    }
    expect(received.size() == total, "every request must be admitted exactly once");

    const auto queryState = [&](const QString &target) {
        SingleInstanceRequest query;
        query.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        query.command = SingleInstanceCommand::ApplicationQuery;
        query.targetRequestId = target;
        FramedClient client;
        if (!client.connectTo(serverName) || !client.send(query))
            return HandoffRequestState::Unknown;
        QByteArray payload;
        QString error;
        if (!receiveWithEventLoop(client, payload, error))
            return HandoffRequestState::Unknown;
        SingleInstanceHandoffResponse response;
        if (!decodeState(payload, response, error))
            return HandoffRequestState::Unknown;
        return response.state;
    };
    expect(queryState(ids.first()) == HandoffRequestState::Unknown,
           "terminal records beyond the retention cap must be evicted");
    expect(queryState(ids.last()) == HandoffRequestState::Taken,
           "recent terminal records must remain queryable after eviction");
    expect(queryState(ids.at(total / 2)) == HandoffRequestState::Taken,
           "records inside the retention window must survive eviction");
}

void BootstrapTests::handoffExpiredTerminalQueryReturnsUnknown() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Primary);
    primary.setLedgerRetentionForTest(60);
    QList<SingleInstanceRequest> received;
    primary.setRequestHandler([&](const auto &pending) { received.append(pending); });
    primary.enableAdmission();

    const auto request = handoffRequest({directory.filePath("expiring.dspx")});
    FramedClient handoffClient;
    expect(handoffClient.connectTo(serverName) && handoffClient.send(request),
           "handoff client must connect");
    QByteArray payload;
    QString error;
    SingleInstanceHandoffResponse state;
    expect(receiveWithEventLoop(handoffClient, payload, error) &&
               decodeState(payload, state, error),
           "handoff must report a state");
    if (state.state == HandoffRequestState::Received ||
        state.state == HandoffRequestState::Pending) {
        expect(receiveWithEventLoop(handoffClient, payload, error) &&
                   decodeState(payload, state, error),
               "a receipt must be followed by a terminal state");
    }
    expect(state.state == HandoffRequestState::Taken,
           "the request must be taken before expiry testing");

    const auto queryState = [&] {
        SingleInstanceRequest query;
        query.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        query.command = SingleInstanceCommand::ApplicationQuery;
        query.targetRequestId = request.requestId;
        FramedClient client;
        if (!client.connectTo(serverName) || !client.send(query))
            return HandoffRequestState::Unknown;
        QByteArray responsePayload;
        QString responseError;
        if (!receiveWithEventLoop(client, responsePayload, responseError))
            return HandoffRequestState::Unknown;
        SingleInstanceHandoffResponse response;
        if (!decodeState(responsePayload, response, responseError))
            return HandoffRequestState::Unknown;
        return response.state;
    };
    expect(queryState() == HandoffRequestState::Taken,
           "a terminal record inside the retention window must be queryable");
    // No further request is finalized during this wait: expiry has to be
    // evaluated by the query itself, not as a side effect of finalize().
    QTest::qWait(150);
    expect(queryState() == HandoffRequestState::Unknown,
           "an expired terminal record must answer result_unknown");
    primary.shutdown();
}

void BootstrapTests::handoffPollsHeartbeatWhilePending() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    QCOMPARE(primary.start(), SingleInstanceCoordinator::StartResult::Primary);

    // A stalled main thread: the IPC worker stays alive, admission never opens
    // and no new heartbeat is published. The secondary must still notice the
    // heartbeat ageing out instead of reporting "starting" forever.
    for (auto *timer : primary.findChildren<QTimer *>())
        timer->stop();
    primary.noteMainThreadHeartbeat();

    SecondaryHandoffOptions options;
    options.interactive = false;
    options.connectTimeoutMs = 1000;
    options.progressDelayMs = 20;
    options.recoveryDelayMs = 100000;
    options.totalBudgetMs = 4200;
    SecondaryHandoffController secondary(serverName, directory.path(), AppHostMode::Gui);
    const auto result =
        secondary.run(handoffRequest({directory.filePath("stalled.dspx")}), options);

    expect(result.outcome == HandoffOutcome::Unknown,
           "a primary that never takes the request must not report success");
    expect(result.heartbeatAgeMs > 3000,
           "polling must refresh the heartbeat age past the stalled threshold");
    expect(result.status.contains(QStringLiteral("not responding"), Qt::CaseInsensitive),
           "the status text must report an unresponsive main thread");
    const QRegularExpression pollCount(QStringLiteral("statusPollCount=(\\d+)"));
    const auto match = pollCount.match(secondary.diagnostics());
    expect(match.hasMatch() && match.captured(1).toInt() >= 2,
           "the pending handoff must be polled repeatedly");
    primary.shutdown();
}
