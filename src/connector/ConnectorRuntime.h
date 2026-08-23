#ifndef DSCONNECTOR_CONNECTORRUNTIME_H
#define DSCONNECTOR_CONNECTORRUNTIME_H

#include "BootstrapWatcher.h"
#include "ExposurePolicy.h"
#include "UpstreamMcpClient.h"

#include <lite/AutomationWire/McpProtocol.h>
#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QObject>
#include <QSet>

#include <functional>
#include <optional>

namespace DsConnector {

    struct ToolCallOutcome {
        QJsonObject result;
        std::optional<AutomationWire::Mcp::ProtocolError> protocolError;
    };

    class ConnectorRuntime final : public QObject {
        Q_OBJECT

    public:
        using ToolCallCallback = std::function<void(ToolCallOutcome)>;

        explicit ConnectorRuntime(ConnectorOptions options, QString bootstrapServiceName = {},
                                  QObject *parent = nullptr);

        void start();
        void stop();
        void reconnect();

        QString instanceId() const;
        QJsonObject status() const;
        QJsonArray downstreamTools() const;
        const ExposurePolicy &exposurePolicy() const;
        qint64 callTool(const QString &name, const QJsonObject &arguments,
                        ToolCallCallback callback);
        bool cancel(qint64 requestToken);

        static const QStringList &bridgeToolNames();
        static QJsonArray bridgeToolDefinitions();
        static std::optional<QJsonObject> findBridgeTool(const QString &name);

    signals:
        void statusChanged();

    private:
        void bootstrapChanged();
        void clearEditorState(const QString &error);
        void beginHandshake(const SingleInstanceAutomationSnapshot &snapshot);
        void requestToolsPage(quint64 epoch, const QString &cursor, QJsonArray accumulated,
                              QSet<QString> seenCursors, int pageCount);
        void finishHandshake(quint64 epoch, const UpstreamResult &manifestResult = {});
        void requestManifest(quint64 epoch);
        void requestManifestPage(quint64 epoch, const QString &cursor, QJsonObject accumulated,
                                 QSet<QString> seenCursors, int pageCount);
        QString editorUnavailableCode() const;
        QString actualAvailabilityCode(const QJsonObject &tool) const;
        QJsonArray filteredActualTools() const;
        QJsonObject actualTool(const QString &name) const;
        QJsonObject manifestOperation(const QString &name) const;
        QString compatibilityFor(const AutomationWire::ToolContract &tool) const;
        bool targetAllowed(const QJsonObject &tool, const QString &name) const;
        ToolCallOutcome connectorError(const QString &code, const QString &message = {}) const;
        qint64 forwardEditorTool(const QString &name, const QJsonObject &arguments,
                                 ToolCallCallback callback);
        ToolCallOutcome listActualTools(const QJsonObject &arguments) const;
        ToolCallOutcome searchActualTools(const QJsonObject &arguments) const;
        ToolCallOutcome describeActualTool(const QJsonObject &arguments) const;

        ConnectorOptions m_options;
        ExposurePolicy m_exposure;
        QString m_instanceId;
        QString m_version;
        BootstrapWatcher *m_bootstrap = nullptr;
        UpstreamMcpClient *m_upstream = nullptr;
        QJsonArray m_actualTools;
        QJsonObject m_manifest;
        QHash<QByteArray, bool> m_schemaValidationCache;
        QString m_manifestCompatibility = QStringLiteral("not_loaded");
        QString m_mcpError;
        bool m_mcpConnected = false;
        quint64 m_handshakeEpoch = 0;
        quint64 m_handledSnapshotSequence = 0;
        QString m_editorInstanceId;
        AutomationWire::OpaqueCursorCodec m_editorToolsCursorCodec;
    };

}

#endif // DSCONNECTOR_CONNECTORRUNTIME_H
