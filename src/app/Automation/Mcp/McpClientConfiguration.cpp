#include "McpClientConfiguration.h"

#include <lite/ProductMetadata.h>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Automation::McpClientConfiguration {
    namespace {
        QString configurationJson(const QJsonObject &server) {
            return QString::fromUtf8(QJsonDocument(server).toJson(QJsonDocument::Indented))
                .trimmed();
        }
    } // namespace

    QString connectorExecutablePath(const QString &applicationDirectory) {
        auto fileName = QString::fromLatin1(LiteProductMetadata::ConnectorExecutableBasename);
#ifdef Q_OS_WIN
        fileName += QStringLiteral(".exe");
#endif
        return QDir::toNativeSeparators(QDir(applicationDirectory).absoluteFilePath(fileName));
    }

    QStringList connectorArguments(const AutomationOption::ControlLevel controlLevel,
                                   QStringList enabledCustomOperations) {
        QString levelName;
        switch (controlLevel) {
            case AutomationOption::ControlLevel::L1:
                levelName = QStringLiteral("l1");
                break;
            case AutomationOption::ControlLevel::L2:
                levelName = QStringLiteral("l2");
                break;
            case AutomationOption::ControlLevel::L3:
                levelName = QStringLiteral("l3");
                break;
            case AutomationOption::ControlLevel::Custom:
                levelName = QStringLiteral("l0");
                break;
        }

        QStringList arguments{QStringLiteral("--control-level"), levelName};
        if (controlLevel != AutomationOption::ControlLevel::Custom)
            return arguments;

        enabledCustomOperations.removeAll(QString{});
        enabledCustomOperations.removeDuplicates();
        enabledCustomOperations.sort();
        for (const auto &operationId : enabledCustomOperations)
            arguments.append(QStringLiteral("--include-tool=id:%1").arg(operationId));
        return arguments;
    }

    QString stdioJson(const QString &command, const QStringList &arguments) {
        const QJsonObject server{
            {QStringLiteral("type"),    QStringLiteral("stdio")              },
            {QStringLiteral("command"), QDir::fromNativeSeparators(command)  },
            {QStringLiteral("args"),    QJsonArray::fromStringList(arguments)},
        };
        return configurationJson(server);
    }

    QString streamableHttpJson(const QString &endpoint) {
        const QJsonObject server{
            {QStringLiteral("type"), QStringLiteral("streamable-http")},
            {QStringLiteral("url"),  endpoint                         },
        };
        return configurationJson(server);
    }

} // namespace Automation::McpClientConfiguration
