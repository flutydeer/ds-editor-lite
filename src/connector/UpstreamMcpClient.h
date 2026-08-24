#ifndef DSCONNECTOR_UPSTREAMMCPCLIENT_H
#define DSCONNECTOR_UPSTREAMMCPCLIENT_H

#include <lite/AutomationWire/McpProtocol.h>

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QUrl>

#include <functional>
#include <optional>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace DsConnector {

    struct UpstreamResult {
        QJsonObject result;
        std::optional<AutomationWire::Mcp::ProtocolError> protocolError;
        QString connectorError;
        QString connectorErrorMessage;
        int httpStatus = 0;
        bool outcomeUnknown = false;

        bool succeeded() const {
            return connectorError.isEmpty() && !protocolError.has_value();
        }
    };

    class UpstreamMcpClient final : public QObject {
        Q_OBJECT

    public:
        using Callback = std::function<void(UpstreamResult)>;

        explicit UpstreamMcpClient(QString connectorInstanceId, QString connectorVersion,
                                   QObject *parent = nullptr);
        ~UpstreamMcpClient() override;

        bool setEndpoint(const QString &endpoint, QString *error = nullptr);
        void clearEndpoint(const QString &reason);
        QUrl endpoint() const;
        qint64 send(const QString &method, QJsonObject params, Callback callback,
                    int timeoutMs = 30000,
                    const QHash<QByteArray, QByteArray> &parameterHeaders = {});
        bool cancel(qint64 requestToken,
                    const QString &reason = QStringLiteral("request_cancelled"));
        void abortAll(const QString &reason);
        qsizetype pendingCount() const;

    private:
        struct PendingRequest {
            QNetworkReply *reply = nullptr;
            QTimer *timer = nullptr;
            QJsonValue upstreamId;
            Callback callback;
            QString forcedError;
            bool forcedOutcomeUnknown = false;
            QByteArray responseBody;
        };

        void finish(qint64 token);
        static std::optional<QJsonObject> decodeResponseBody(const QByteArray &body,
                                                             const QByteArray &contentType,
                                                             QString &error);

        QString m_connectorInstanceId;
        QString m_connectorVersion;
        QNetworkAccessManager *m_network = nullptr;
        QUrl m_endpoint;
        qint64 m_nextToken = 1;
        QHash<qint64, PendingRequest> m_pending;
    };

}

#endif // DSCONNECTOR_UPSTREAMMCPCLIENT_H
