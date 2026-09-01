#ifndef AUTOMATIONWIRE_PUBLICVALUEDOMAINS_H
#define AUTOMATIONWIRE_PUBLICVALUEDOMAINS_H

#include <QJsonArray>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace AutomationWire {

    enum class PublicValueDomain {
#define AUTOMATION_WIRE_PUBLIC_VALUE_DOMAIN(symbol, wire) symbol,
#include "PublicValueDomainDefinitions.inc"
#undef AUTOMATION_WIRE_PUBLIC_VALUE_DOMAIN
        Count,
    };

    QString publicValueDomainName(PublicValueDomain domain);
    const QJsonArray &publicValueDomainValues(PublicValueDomain domain);
    QStringList publicStringValueDomainValues(PublicValueDomain domain);
    bool publicValueDomainContains(PublicValueDomain domain, const QJsonValue &value);

}

#endif // AUTOMATIONWIRE_PUBLICVALUEDOMAINS_H
