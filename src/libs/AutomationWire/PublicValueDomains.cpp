#include "PublicValueDomains.h"

#include <QHash>

namespace AutomationWire {

    QString publicValueDomainName(const PublicValueDomain domain) {
        switch (domain) {
#define AUTOMATION_WIRE_PUBLIC_VALUE_DOMAIN(symbol, wire)                                      \
    case PublicValueDomain::symbol:                                                            \
        return QStringLiteral(wire);
#include "PublicValueDomainDefinitions.inc"
#undef AUTOMATION_WIRE_PUBLIC_VALUE_DOMAIN
        }
        return {};
    }

    const QJsonArray &publicValueDomainValues(const PublicValueDomain domain) {
        static const auto values = [] {
            QHash<PublicValueDomain, QJsonArray> result;
#define AUTOMATION_WIRE_PUBLIC_STRING_VALUE(domain, symbol, wire)                              \
    result[PublicValueDomain::domain].append(QStringLiteral(wire));
#define AUTOMATION_WIRE_PUBLIC_INTEGER_VALUE(domain, symbol, value)                            \
    result[PublicValueDomain::domain].append(value);
#include "PublicValueDefinitions.inc"
#undef AUTOMATION_WIRE_PUBLIC_INTEGER_VALUE
#undef AUTOMATION_WIRE_PUBLIC_STRING_VALUE
            return result;
        }();
        static const QJsonArray empty;
        const auto it = values.constFind(domain);
        return it == values.cend() ? empty : it.value();
    }

    QStringList publicStringValueDomainValues(const PublicValueDomain domain) {
        QStringList result;
        for (const auto &value : publicValueDomainValues(domain)) {
            if (value.isString())
                result.append(value.toString());
        }
        return result;
    }

    bool publicValueDomainContains(const PublicValueDomain domain, const QJsonValue &value) {
        return publicValueDomainValues(domain).contains(value);
    }

}
