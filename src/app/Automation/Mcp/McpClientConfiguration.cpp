#include "McpClientConfiguration.h"

#include <lite/ProductMetadata.h>

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Automation::McpClientConfiguration {
    namespace {
        QString configurationJson(const QJsonObject &server) {
            return QString::fromUtf8(QJsonDocument(server).toJson(QJsonDocument::Indented)).trimmed();
        }
    } // namespace

    QString connectorExecutablePath(const QString &applicationDirectory) {
        auto fileName =
            QString::fromLatin1(LiteProductMetadata::ConnectorExecutableBasename);
#ifdef Q_OS_WIN
        fileName += QStringLiteral(".exe");
#endif
        return QDir::toNativeSeparators(QDir(applicationDirectory).absoluteFilePath(fileName));
    }

    QStringList connectorArguments(const AutomationOption::Profile profile,
                                   QStringList enabledCustomOperations) {
        QString profileName;
        switch (profile) {
            case AutomationOption::Profile::L1:
                profileName = QStringLiteral("l1");
                break;
            case AutomationOption::Profile::L2:
                profileName = QStringLiteral("l2");
                break;
            case AutomationOption::Profile::L3:
                profileName = QStringLiteral("l3");
                break;
            case AutomationOption::Profile::Custom:
                profileName = QStringLiteral("l0");
                break;
        }

        QStringList arguments{QStringLiteral("--exposure-profile"), profileName};
        if (profile != AutomationOption::Profile::Custom)
            return arguments;

        enabledCustomOperations.removeAll(QString{});
        enabledCustomOperations.removeDuplicates();
        enabledCustomOperations.sort();
        for (const auto &operationId : enabledCustomOperations) {
            arguments.append(QStringLiteral("--include-tool"));
            arguments.append(QStringLiteral("id:%1").arg(operationId));
        }
        return arguments;
    }

    QString stdioJson(const QString &command, const QStringList &arguments) {
        const QJsonObject server{
            {QStringLiteral("type"),    QStringLiteral("stdio")                },
            {QStringLiteral("command"), command                                },
            {QStringLiteral("args"),    QJsonArray::fromStringList(arguments)   },
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
