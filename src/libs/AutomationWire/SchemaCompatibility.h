#ifndef AUTOMATIONWIRE_SCHEMACOMPATIBILITY_H
#define AUTOMATIONWIRE_SCHEMACOMPATIBILITY_H

#include <QJsonValue>
#include <QString>

namespace AutomationWire {

    enum class SchemaCompatibilityStatus {
        Compatible,
        Incompatible,
        Unproven,
        InvalidSchema,
    };

    struct SchemaCompatibilityResult {
        SchemaCompatibilityStatus status = SchemaCompatibilityStatus::Unproven;
        QString reason;

        bool compatible() const {
            return status == SchemaCompatibilityStatus::Compatible;
        }
    };

    struct ToolSchemaCompatibility {
        SchemaCompatibilityResult input;
        SchemaCompatibilityResult output;

        bool compatible() const {
            return input.compatible() && output.compatible();
        }
    };

    // Proves that every value produced by sourceSchema is accepted by targetSchema.
    // Unsupported or ambiguous proofs return Unproven and are never treated as compatible.
    SchemaCompatibilityResult proveSchemaSubset(const QJsonValue &sourceSchema,
                                                 const QJsonValue &targetSchema);

    // Connector requests flow connectorInput -> editorInput; results flow
    // editorOutput -> connectorOutput.
    ToolSchemaCompatibility checkToolSchemaCompatibility(const QJsonValue &connectorInput,
                                                         const QJsonValue &editorInput,
                                                         const QJsonValue &editorOutput,
                                                         const QJsonValue &connectorOutput);

}

#endif // AUTOMATIONWIRE_SCHEMACOMPATIBILITY_H
