#include "tst_bootstrap.h"
#include "Bootstrap/SingleInstanceCoordinator.h"
#include "Bootstrap/SingleInstanceIdentity.h"
#include "Bootstrap/SingleInstanceProtocol.h"

#include <QCoreApplication>
#include <QtTest>
#include "../TestSupport/TestAssertions.h"
#include <QDir>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QStandardPaths>
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

void BootstrapTests::coordinator() {
    QTemporaryDir directory;
    expect(directory.isValid(), "temporary instance directory must be available");
    if (!directory.isValid())
        return;

    const auto serverName = uniqueServerName();
    SingleInstanceCoordinator primary(directory.path(), serverName);
    expect(primary.start() == SingleInstanceCoordinator::StartResult::Primary,
           "first coordinator must become primary");

    SingleInstanceCoordinator secondary(directory.path(), serverName);
    expect(secondary.start() == SingleInstanceCoordinator::StartResult::Secondary,
           "second coordinator must detect the primary");

    const auto firstRequest = openRequest({QDir(directory.path()).filePath("first.dspx")});
    QString error;
    expect(secondary.forwardRequest(firstRequest, error), "secondary request must be acknowledged");

    QList<SingleInstanceRequest> received;
    primary.setRequestHandler(
        [&received](const SingleInstanceRequest &request) { received.append(request); });
    primary.pauseRequestDispatchAndFlush();
    expect(received.size() == 1 && received.first().requestId == firstRequest.requestId,
           "acknowledged request before handler setup must be flushed deterministically");
    primary.resumeRequestDispatch();

    const auto secondRequest = openRequest({QDir(directory.path()).filePath("second.dspx")});
    error.clear();
    expect(secondary.forwardRequest(secondRequest, error),
           "request after handler setup must be acknowledged");
    primary.pauseRequestDispatchAndFlush();
    expect(received.size() == 2 && received.last().requestId == secondRequest.requestId,
           "acknowledged request after handler setup must be flushed deterministically");
    primary.resumeRequestDispatch();

    primary.pauseRequestDispatchAndFlush();
    const auto deferredRequest = openRequest({QDir(directory.path()).filePath("deferred.dspx")});
    FramedClient deferredClient;
    expect(deferredClient.connectTo(serverName) && deferredClient.send(deferredRequest),
           "request client must connect while dispatch is paused");
    QByteArray deferredPayload;
    QString deferredError;
    expect(!deferredClient.receive(deferredPayload, deferredError, 100),
           "request accepted after the barrier must not be acknowledged while paused");
    primary.resumeRequestDispatch();
    SingleInstanceResponse deferredResponse;
    expect(deferredClient.receive(deferredPayload, deferredError) &&
               SingleInstanceProtocol::decodeResponse(deferredPayload, deferredResponse,
                                                      deferredError) &&
               deferredResponse.accepted && deferredResponse.requestId == deferredRequest.requestId,
           "paused request must be acknowledged after dispatch resumes");
    primary.pauseRequestDispatchAndFlush();
    expect(received.size() == 3 && received.last().requestId == deferredRequest.requestId,
           "resumed request must reach the installed handler");
    primary.resumeRequestDispatch();

    SingleInstanceRequest activateRequest;
    activateRequest.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    activateRequest.command = SingleInstanceCommand::Activate;
    error.clear();
    expect(secondary.forwardRequest(activateRequest, error),
           "legacy activate request must remain supported");
    primary.pauseRequestDispatchAndFlush();
    expect(received.size() == 4 && received.last().requestId == activateRequest.requestId &&
               received.last().command == SingleInstanceCommand::Activate &&
               received.last().paths.isEmpty(),
           "legacy activate request must retain its v1 behavior");
    primary.resumeRequestDispatch();

    primary.shutdown();
    SingleInstanceCoordinator replacement(directory.path(), serverName);
    expect(replacement.start() == SingleInstanceCoordinator::StartResult::Primary,
           "a new coordinator must take ownership after primary shutdown");
    replacement.shutdown();
}
