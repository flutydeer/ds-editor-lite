#ifndef AUTOMATIONWIRE_PUBLICENUMS_H
#define AUTOMATIONWIRE_PUBLICENUMS_H

#include <QString>
#include <QStringList>

#include <optional>

namespace AutomationWire {

    enum class DocumentLifecycle {
#define AUTOMATION_WIRE_DOCUMENT_LIFECYCLE(symbol, wire) symbol,
#define AUTOMATION_WIRE_OBJECT_KIND(symbol, wire)
#include "PublicEnumDefinitions.inc"
#undef AUTOMATION_WIRE_OBJECT_KIND
#undef AUTOMATION_WIRE_DOCUMENT_LIFECYCLE
    };

    enum class PublicObjectKind {
#define AUTOMATION_WIRE_DOCUMENT_LIFECYCLE(symbol, wire)
#define AUTOMATION_WIRE_OBJECT_KIND(symbol, wire) symbol,
#include "PublicEnumDefinitions.inc"
#undef AUTOMATION_WIRE_OBJECT_KIND
#undef AUTOMATION_WIRE_DOCUMENT_LIFECYCLE
    };

    QString documentLifecycleName(DocumentLifecycle value);
    std::optional<DocumentLifecycle> documentLifecycleFromName(const QString &value);
    const QStringList &documentLifecycleValues();

    QString publicObjectKindName(PublicObjectKind value);
    std::optional<PublicObjectKind> publicObjectKindFromName(const QString &value);
    const QStringList &publicObjectKindValues();

}

#endif // AUTOMATIONWIRE_PUBLICENUMS_H
