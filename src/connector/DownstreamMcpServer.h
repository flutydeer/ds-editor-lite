#ifndef DSCONNECTOR_DOWNSTREAMMCPSERVER_H
#define DSCONNECTOR_DOWNSTREAMMCPSERVER_H

#include "ConnectorRuntime.h"

#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QObject>
#include <QSet>

namespace DsConnector {

    class DownstreamMcpServer final : public QObject {
        Q_OBJECT

    public:
        explicit DownstreamMcpServer(ConnectorRuntime *runtime, QObject *parent = nullptr);

        void processLine(const QByteArray &line);

    signals:
        void responseLine(const QByteArray &line);

    private:
        void sendJson(const QJsonObject &message);
        void sendError(const QJsonValue &id, int code, const QString &message,
                       const QJsonValue &data = QJsonValue(QJsonValue::Undefined));
        void handleCancellation(const AutomationWire::Mcp::RequestEnvelope &request);
        void handleToolsList(const AutomationWire::Mcp::RequestEnvelope &request);
        void handleToolsCall(const AutomationWire::Mcp::RequestEnvelope &request);
        static QString idKey(const QJsonValue &id);

        ConnectorRuntime *m_runtime = nullptr;
        QHash<QString, qint64> m_pending;
        QSet<QString> m_inFlight;
        QSet<QString> m_cancelled;
        AutomationWire::OpaqueCursorCodec m_toolsCursorCodec;
    };

}

#endif // DSCONNECTOR_DOWNSTREAMMCPSERVER_H
