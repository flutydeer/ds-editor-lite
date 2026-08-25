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

class QTimer;

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
        bool cancel(qint64 requestToken,
                    const QString &reason = QStringLiteral("request_cancelled"));

        static const QStringList &bridgeToolNames();
        static QJsonArray bridgeToolDefinitions();
        static std::optional<QJsonObject> findBridgeTool(const QString &name);

    signals:
        void statusChanged();

    private:
        void bootstrapChanged();
        void clearEditorState(const QString &error);
        void beginHandshake(const SingleInstanceAutomationSnapshot &snapshot);
        void startHandshakeAttempt();
        void startModernHandshake(quint64 epoch);
        void startLegacyHandshake(quint64 epoch);
        void failHandshake(quint64 epoch, const QString &error,
                           const QString &manifestCompatibility = QStringLiteral("not_loaded"),
                           bool preserveMcpConnection = false);
        void completeHandshakeCycle(quint64 epoch, bool succeeded);
        void requestToolsPage(quint64 epoch, const QString &cursor, QJsonArray accumulated,
                              QSet<QString> seenCursors, int pageCount);
        void requestStatus(quint64 epoch);
        void finishHandshake(quint64 epoch, const UpstreamResult &statusResult = {});
        QString editorUnavailableCode() const;
        QString actualAvailabilityCode(const QJsonObject &tool) const;
        void rebuildToolCaches();
        void clearToolCaches();
        const QJsonArray &filteredActualTools() const;
        QJsonObject actualTool(const QString &name) const;
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
        QTimer *m_handshakeRetryTimer = nullptr;
        QJsonArray m_actualTools;
        QJsonObject m_manifest;
        QHash<QString, QJsonObject> m_actualToolIndex;
        QJsonArray m_filteredActualToolsCache;
        QStringList m_pendingSelectorsCache;
        QString m_filteredActualToolsDigest;
        QString m_connectorManifestDigest;
        QHash<QByteArray, bool> m_schemaValidationCache;
        QString m_manifestCompatibility = QStringLiteral("not_loaded");
        int m_compatibleCount = 0;
        int m_incompatibleCount = 0;
        int m_unavailableCount = 0;
        QString m_mcpError;
        QString m_mcpProtocolVersion;
        bool m_mcpConnected = false;
        quint64 m_handshakeEpoch = 0;
        quint64 m_handledSnapshotSequence = 0;
        QString m_editorInstanceId;
        std::optional<SingleInstanceAutomationSnapshot> m_handshakeTarget;
        bool m_handshakeInProgress = false;
        bool m_handshakeFollowUp = false;
        bool m_handshakeRefreshPending = false;
        int m_handshakeRetryAttempt = 0;
        AutomationWire::OpaqueCursorCodec m_editorToolsCursorCodec;
    };

}

#endif // DSCONNECTOR_CONNECTORRUNTIME_H
