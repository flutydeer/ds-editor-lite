#include "ConnectorOptions.h"
#include "ConnectorRuntime.h"
#include "DownstreamMcpServer.h"
#include "StdioTransport.h"

#include <lite/ProductMetadata.h>

#include <QCoreApplication>
#include <QTextStream>

namespace {
    void printHelp() {
        QTextStream(stdout)
            << "DS Connector Lite " << LiteProductMetadata::Version << Qt::endl
            << "Usage: DsConnectorLite [options]" << Qt::endl
            << "  --exposure-profile l0|l1|l2|l3" << Qt::endl
            << "  --include-tool id:<name>|category:<name>|prefix:<text>" << Qt::endl
            << "  --exclude-tool id:<name>|category:<name>|prefix:<text>"
               " (L0 tools are intrinsic)"
            << Qt::endl;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QString::fromLatin1(LiteProductMetadata::Publisher));
    QCoreApplication::setApplicationName(QStringLiteral("DS Connector Lite"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(LiteProductMetadata::Version));

    auto arguments = application.arguments();
    arguments.removeFirst();
    if (arguments.contains(QStringLiteral("--help")) || arguments.contains(QStringLiteral("-h"))) {
        printHelp();
        return 0;
    }
    if (arguments.contains(QStringLiteral("--version"))) {
        QTextStream(stdout) << LiteProductMetadata::Version << Qt::endl;
        return 0;
    }

    DsConnector::ConnectorOptions options;
    QString optionError;
    if (!DsConnector::parseConnectorOptions(arguments, options, optionError)) {
        QTextStream(stderr) << "DsConnectorLite: " << optionError << Qt::endl;
        return 2;
    }

    DsConnector::ConnectorRuntime runtime(std::move(options));
    DsConnector::DownstreamMcpServer server(&runtime);
    DsConnector::StdioTransport transport;
    QObject::connect(&transport, &DsConnector::StdioTransport::lineReceived, &server,
                     &DsConnector::DownstreamMcpServer::processLine);
    QObject::connect(&server, &DsConnector::DownstreamMcpServer::responseLine, &transport,
                     &DsConnector::StdioTransport::writeLine);
    QObject::connect(&server, &DsConnector::DownstreamMcpServer::pendingResponseStarted,
                     &transport, &DsConnector::StdioTransport::beginPendingResponse);
    QObject::connect(&server, &DsConnector::DownstreamMcpServer::pendingResponseFinished,
                     &transport, &DsConnector::StdioTransport::endPendingResponse);
    QObject::connect(&transport, &DsConnector::StdioTransport::inputClosed, &application,
                     &QCoreApplication::quit);
    QObject::connect(&transport, &DsConnector::StdioTransport::transportError, &application,
                     [&application](const QString &error) {
                         QTextStream(stderr) << "DsConnectorLite: " << error << Qt::endl;
                         application.exit(3);
                     });

    QString transportError;
    if (!transport.start(&transportError)) {
        QTextStream(stderr) << "DsConnectorLite: " << transportError << Qt::endl;
        return 3;
    }
    runtime.start();
    const auto result = application.exec();
    runtime.stop();
    return result;
}
