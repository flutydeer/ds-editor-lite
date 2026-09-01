#ifndef AUTOMATIONWIRE_CANONICALJSON_H
#define AUTOMATIONWIRE_CANONICALJSON_H

#include <QByteArray>
#include <QJsonValue>
#include <QString>

namespace AutomationWire {

    QByteArray canonicalJson(const QJsonValue &value, QString *errorMessage = nullptr);
    QString sha256Digest(const QJsonValue &value, QString *errorMessage = nullptr);
    bool canonicalJsonEqual(const QJsonValue &left, const QJsonValue &right);

}

#endif // AUTOMATIONWIRE_CANONICALJSON_H
