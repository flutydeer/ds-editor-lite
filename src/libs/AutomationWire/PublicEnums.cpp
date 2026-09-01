#include "PublicEnums.h"

namespace AutomationWire {

    QString documentLifecycleName(const DocumentLifecycle value) {
        switch (value) {
#define AUTOMATION_WIRE_DOCUMENT_LIFECYCLE(symbol, wire)                                      \
    case DocumentLifecycle::symbol:                                                           \
        return QStringLiteral(wire);
#define AUTOMATION_WIRE_OBJECT_KIND(symbol, wire)
#include "PublicEnumDefinitions.inc"
#undef AUTOMATION_WIRE_OBJECT_KIND
#undef AUTOMATION_WIRE_DOCUMENT_LIFECYCLE
        }
        return {};
    }

    std::optional<DocumentLifecycle> documentLifecycleFromName(const QString &value) {
#define AUTOMATION_WIRE_DOCUMENT_LIFECYCLE(symbol, wire)                                      \
    if (value == QStringLiteral(wire))                                                         \
        return DocumentLifecycle::symbol;
#define AUTOMATION_WIRE_OBJECT_KIND(symbol, wire)
#include "PublicEnumDefinitions.inc"
#undef AUTOMATION_WIRE_OBJECT_KIND
#undef AUTOMATION_WIRE_DOCUMENT_LIFECYCLE
        return std::nullopt;
    }

    const QStringList &documentLifecycleValues() {
        static const QStringList values{
#define AUTOMATION_WIRE_DOCUMENT_LIFECYCLE(symbol, wire) QStringLiteral(wire),
#define AUTOMATION_WIRE_OBJECT_KIND(symbol, wire)
#include "PublicEnumDefinitions.inc"
#undef AUTOMATION_WIRE_OBJECT_KIND
#undef AUTOMATION_WIRE_DOCUMENT_LIFECYCLE
        };
        return values;
    }

    QString publicObjectKindName(const PublicObjectKind value) {
        switch (value) {
#define AUTOMATION_WIRE_DOCUMENT_LIFECYCLE(symbol, wire)
#define AUTOMATION_WIRE_OBJECT_KIND(symbol, wire)                                              \
    case PublicObjectKind::symbol:                                                             \
        return QStringLiteral(wire);
#include "PublicEnumDefinitions.inc"
#undef AUTOMATION_WIRE_OBJECT_KIND
#undef AUTOMATION_WIRE_DOCUMENT_LIFECYCLE
        }
        return {};
    }

    std::optional<PublicObjectKind> publicObjectKindFromName(const QString &value) {
#define AUTOMATION_WIRE_DOCUMENT_LIFECYCLE(symbol, wire)
#define AUTOMATION_WIRE_OBJECT_KIND(symbol, wire)                                              \
    if (value == QStringLiteral(wire))                                                         \
        return PublicObjectKind::symbol;
#include "PublicEnumDefinitions.inc"
#undef AUTOMATION_WIRE_OBJECT_KIND
#undef AUTOMATION_WIRE_DOCUMENT_LIFECYCLE
        return std::nullopt;
    }

    const QStringList &publicObjectKindValues() {
        static const QStringList values{
#define AUTOMATION_WIRE_DOCUMENT_LIFECYCLE(symbol, wire)
#define AUTOMATION_WIRE_OBJECT_KIND(symbol, wire) QStringLiteral(wire),
#include "PublicEnumDefinitions.inc"
#undef AUTOMATION_WIRE_OBJECT_KIND
#undef AUTOMATION_WIRE_DOCUMENT_LIFECYCLE
        };
        return values;
    }

}
