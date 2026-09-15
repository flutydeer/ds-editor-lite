#pragma once

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include <optional>

namespace TestSupport {
    inline QJsonObject nativeRequest(const QString &id, const QString &method,
                                     const std::optional<QJsonObject> &params = std::nullopt) {
        QJsonObject request{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"),      id                   },
            {QStringLiteral("method"),  method               },
        };
        if (params)
            request.insert(QStringLiteral("params"), *params);
        return request;
    }

    inline std::optional<QJsonObject> nativeExchange(QNetworkAccessManager &manager,
                                                     const QUrl &endpoint,
                                                     const QJsonObject &request, QString &error,
                                                     const int timeoutMilliseconds = 5000) {
        error.clear();
        QNetworkRequest httpRequest(endpoint);
        httpRequest.setRawHeader("Content-Type", "application/json; charset=utf-8");
        httpRequest.setRawHeader("Accept", "application/json");
        auto *reply =
            manager.post(httpRequest, QJsonDocument(request).toJson(QJsonDocument::Compact));
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        timeout.start(timeoutMilliseconds);
        loop.exec();
        if (!reply->isFinished()) {
            reply->abort();
            error = QStringLiteral("Native request timed out");
            reply->deleteLater();
            return std::nullopt;
        }
        const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto networkError = reply->error();
        const auto responseBytes = reply->readAll();
        const auto networkErrorText = reply->errorString();
        reply->deleteLater();
        if (status != 200 || networkError != QNetworkReply::NoError) {
            error =
                QStringLiteral("Native HTTP request failed: status=%1, network_error=%2, body=%3")
                    .arg(status)
                    .arg(networkErrorText, QString::fromUtf8(responseBytes));
            return std::nullopt;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(responseBytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            error = QStringLiteral("Native response was not a JSON object: %1")
                        .arg(QString::fromUtf8(responseBytes));
            return std::nullopt;
        }
        const auto response = document.object();
        if (response.value(QStringLiteral("id")) != request.value(QStringLiteral("id"))) {
            error =
                QStringLiteral("Native response returned an unexpected id: %1")
                    .arg(QString::fromUtf8(QJsonDocument(response).toJson(QJsonDocument::Compact)));
            return std::nullopt;
        }
        return response;
    }
}
