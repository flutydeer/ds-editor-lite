#include "Bootstrap/SingleInstanceCoordinator.h"
#include "Bootstrap/SingleInstanceProtocol.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QUuid>

#include <functional>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

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

    bool verifyProtocol() {
        const auto request = openRequest(
            {QStringLiteral("C:/projects/a.dspx"), QStringLiteral("C:/projects/b.dspx")});
        const auto encoded = SingleInstanceProtocol::encodeRequest(request);
        SingleInstanceRequest decoded;
        QString error;
        bool ok = true;
        ok &= expect(SingleInstanceProtocol::decodeRequest(encoded, decoded, error),
                     "request payload must round-trip");
        ok &=
            expect(decoded.requestId == request.requestId, "request ID must survive serialization");
        ok &= expect(decoded.command == SingleInstanceCommand::OpenProjects,
                     "request command must survive serialization");
        ok &= expect(decoded.paths == request.paths, "project paths must survive serialization");

        const auto framed = SingleInstanceProtocol::frame(encoded);
        QByteArray buffer = framed.left(3);
        QByteArray payload;
        error.clear();
        ok &= expect(!SingleInstanceProtocol::takeFrame(buffer, payload, error) && error.isEmpty(),
                     "partial frame header must wait for more data");
        buffer.append(framed.mid(3));
        ok &= expect(SingleInstanceProtocol::takeFrame(buffer, payload, error),
                     "complete frame must be extracted");
        ok &= expect(payload == encoded && buffer.isEmpty(),
                     "frame extraction must consume exactly one frame");

        QByteArray oversized(4, '\0');
        const auto tooLarge = static_cast<quint32>(SingleInstanceProtocol::maxPayloadSize + 1);
        oversized[0] = static_cast<char>((tooLarge >> 24) & 0xff);
        oversized[1] = static_cast<char>((tooLarge >> 16) & 0xff);
        oversized[2] = static_cast<char>((tooLarge >> 8) & 0xff);
        oversized[3] = static_cast<char>(tooLarge & 0xff);
        error.clear();
        ok &= expect(!SingleInstanceProtocol::takeFrame(oversized, payload, error) &&
                         !error.isEmpty(),
                     "oversized frames must be rejected");
        return ok;
    }

    bool verifyCoordinator() {
        QTemporaryDir directory;
        bool ok = expect(directory.isValid(), "temporary instance directory must be available");
        if (!directory.isValid())
            return false;

        const auto serverName = QStringLiteral("DsEditorLite-Test-%1")
                                    .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
        SingleInstanceCoordinator primary(directory.path(), serverName);
        ok &= expect(primary.start() == SingleInstanceCoordinator::StartResult::Primary,
                     "first coordinator must become primary");

        SingleInstanceCoordinator secondary(directory.path(), serverName);
        ok &= expect(secondary.start() == SingleInstanceCoordinator::StartResult::Secondary,
                     "second coordinator must detect the primary");

        const auto firstRequest = openRequest({QDir(directory.path()).filePath("first.dspx")});
        QString error;
        ok &= expect(secondary.forwardRequest(firstRequest, error),
                     "secondary request must be acknowledged");

        QList<SingleInstanceRequest> received;
        primary.setRequestHandler(
            [&received](const SingleInstanceRequest &request) { received.append(request); });
        ok &= expect(waitUntil([&] { return received.size() == 1; }) &&
                         received.first().requestId == firstRequest.requestId,
                     "request received before UI setup must be retained");

        const auto secondRequest = openRequest({QDir(directory.path()).filePath("second.dspx")});
        error.clear();
        ok &= expect(secondary.forwardRequest(secondRequest, error),
                     "request after handler setup must be acknowledged");
        ok &= expect(waitUntil([&] { return received.size() == 2; }) &&
                         received.last().requestId == secondRequest.requestId,
                     "request after UI setup must reach the handler");

        primary.shutdown();
        SingleInstanceCoordinator replacement(directory.path(), serverName);
        ok &= expect(replacement.start() == SingleInstanceCoordinator::StartResult::Primary,
                     "a new coordinator must take ownership after primary shutdown");
        replacement.shutdown();
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("OpenVPI"));
    QCoreApplication::setApplicationName(QStringLiteral("DsEditorLiteSingleInstanceTest"));
    const bool ok = verifyProtocol() && verifyCoordinator();
    return ok ? 0 : 1;
}
