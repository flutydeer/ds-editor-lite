#include "ConnectorRuntime.h"

#include <lite/AutomationWire/CanonicalJson.h>

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/SchemaCompatibility.h>
#include <lite/ProductMetadata.h>

#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace DsConnector {
    namespace {
        constexpr auto MaxHandshakePages = 1024;
        constexpr auto MaxHandshakeItems = 100000;
        constexpr auto MaxHandshakeRetryAttempts = 4;
        constexpr auto InitialHandshakeRetryDelayMs = 100;
        constexpr auto MaximumHandshakeRetryDelayMs = 1600;
        constexpr qint64 MaximumSafeJsonInteger = 9007199254740991LL;

        struct HeaderBinding {
            QStringList path;
            QByteArray name;
            QString type;
        };

        bool collectHeaderBindings(const QJsonObject &inputSchema,
                                   QList<HeaderBinding> &bindings, QString &error) {
            bindings.clear();
            error.clear();
            QSet<QString> names;
            static const QRegularExpression token(
                QStringLiteral("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$"));
            std::function<bool(const QJsonValue &, bool, const QStringList &)> visit;
            visit = [&](const QJsonValue &value, const bool staticallyReachable,
                        const QStringList &path) {
                if (value.isArray()) {
                    for (const auto &entry : value.toArray()) {
                        if (!visit(entry, false, path))
                            return false;
                    }
                    return true;
                }
                if (!value.isObject())
                    return true;
                const auto schema = value.toObject();
                if (schema.contains(QStringLiteral("x-mcp-header"))) {
                    const auto header = schema.value(QStringLiteral("x-mcp-header"));
                    const auto type = schema.value(QStringLiteral("type")).toString();
                    if (!staticallyReachable || path.isEmpty() || !header.isString() ||
                        !token.match(header.toString()).hasMatch() ||
                        (type != QStringLiteral("string") &&
                         type != QStringLiteral("integer") &&
                         type != QStringLiteral("boolean")) ||
                        names.contains(header.toString().toCaseFolded())) {
                        error = QStringLiteral("invalid x-mcp-header annotation");
                        return false;
                    }
                    names.insert(header.toString().toCaseFolded());
                    bindings.append({path, header.toString().toLatin1(), type});
                }
                for (auto it = schema.constBegin(); it != schema.constEnd(); ++it) {
                    if (it.key() == QStringLiteral("x-mcp-header"))
                        continue;
                    if (it.key() == QStringLiteral("properties") && it.value().isObject()) {
                        const auto properties = it.value().toObject();
                        for (auto property = properties.constBegin();
                             property != properties.constEnd(); ++property) {
                            auto propertyPath = path;
                            propertyPath.append(property.key());
                            if (!visit(property.value(), staticallyReachable, propertyPath))
                                return false;
                        }
                    } else if (!visit(it.value(), false, path)) {
                        return false;
                    }
                }
                return true;
            };
            return visit(inputSchema, true, {});
        }

        bool parameterHeaders(const QJsonObject &inputSchema, const QJsonObject &arguments,
                              QHash<QByteArray, QByteArray> &headers, QString &error) {
            QList<HeaderBinding> bindings;
            if (!collectHeaderBindings(inputSchema, bindings, error))
                return false;
            headers.clear();
            for (const auto &binding : std::as_const(bindings)) {
                QJsonValue value = arguments;
                for (const auto &component : binding.path) {
                    if (!value.isObject()) {
                        value = QJsonValue(QJsonValue::Undefined);
                        break;
                    }
                    value = value.toObject().value(component);
                }
                if (value.isUndefined() || value.isNull())
                    continue;
                QString text;
                if (binding.type == QStringLiteral("string") && value.isString()) {
                    text = value.toString();
                } else if (binding.type == QStringLiteral("boolean") && value.isBool()) {
                    text = value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
                } else if (binding.type == QStringLiteral("integer") && value.isDouble()) {
                    const auto integer = value.toInteger(MaximumSafeJsonInteger + 1);
                    if (integer < -MaximumSafeJsonInteger || integer > MaximumSafeJsonInteger) {
                        error = QStringLiteral("x-mcp-header integer is outside the safe range");
                        return false;
                    }
                    text = QString::number(integer);
                } else {
                    error = QStringLiteral("x-mcp-header argument has the wrong primitive type");
                    return false;
                }
                headers.insert("Mcp-Param-" + binding.name,
                               AutomationWire::Mcp::encodeHeaderValue(text).toUtf8());
            }
            return true;
        }

        QString connectorBuildId() {
#ifdef LITE_GIT_LAST_COMMIT_HASH
            return QString::fromLatin1(LITE_GIT_LAST_COMMIT_HASH);
#else
            return QString::fromLatin1(LiteProductMetadata::Version);
#endif
        }

        const AutomationWire::PublicManifest &connectorManifest() {
            static const auto manifest =
                AutomationWire::buildPublicManifest(AutomationWire::AutomationProfile::L2);
            return manifest;
        }

        QJsonObject emptyObjectSchema() {
            return {
                {QStringLiteral("$schema"),
                 QStringLiteral("https://json-schema.org/draft/2020-12/schema")},
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), QJsonObject{}},
                {QStringLiteral("additionalProperties"), false},
            };
        }

        QJsonObject openObjectSchema() {
            return {
                {QStringLiteral("$schema"),
                 QStringLiteral("https://json-schema.org/draft/2020-12/schema")},
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("additionalProperties"), true},
            };
        }

        QJsonObject strictObjectSchema(QJsonObject properties, QJsonArray required = {}) {
            QJsonObject result{
                {QStringLiteral("$schema"),
                 QStringLiteral("https://json-schema.org/draft/2020-12/schema")},
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("properties"), std::move(properties)},
                {QStringLiteral("additionalProperties"), false},
            };
            if (!required.isEmpty())
                result.insert(QStringLiteral("required"), required);
            return result;
        }

        QJsonObject stringSchema() {
            return {{QStringLiteral("type"), QStringLiteral("string")}};
        }

        QJsonObject enumStringSchema(const QStringList &values) {
            QJsonArray entries;
            for (const auto &value : values)
                entries.append(value);
            return {
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), entries},
            };
        }

        QJsonObject digestSchema() {
            return {
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("pattern"), QStringLiteral("^(?:|sha256:[0-9a-f]{64})$")},
            };
        }

        QJsonObject integerSchema(const int minimum = 0) {
            return {
                {QStringLiteral("type"), QStringLiteral("integer")},
                {QStringLiteral("minimum"), minimum},
            };
        }

        QJsonObject stringArraySchema() {
            return {
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), stringSchema()},
            };
        }

        QJsonObject schemaReference(const QString &name) {
            return {{QStringLiteral("$ref"), QStringLiteral("#/$defs/%1").arg(name)}};
        }

        QJsonObject jsonValueMetaSchema() {
            const auto self = schemaReference(QStringLiteral("json_value"));
            return {
                {QStringLiteral("oneOf"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("null")}},
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}},
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}},
                     stringSchema(),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                 {QStringLiteral("items"), self}},
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                                 {QStringLiteral("additionalProperties"), self}},
                 }},
            };
        }

        QJsonObject jsonSchemaMetaSchema() {
            const auto schema = schemaReference(QStringLiteral("schema"));
            const auto value = schemaReference(QStringLiteral("json_value"));
            const QJsonObject typeName{
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"),
                 QJsonArray{QStringLiteral("null"), QStringLiteral("boolean"),
                            QStringLiteral("object"), QStringLiteral("array"),
                            QStringLiteral("number"), QStringLiteral("integer"),
                            QStringLiteral("string")}},
            };
            const QJsonObject typeNames{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), typeName},
                {QStringLiteral("minItems"), 1},
                {QStringLiteral("uniqueItems"), true},
            };
            const QJsonObject schemaMap{
                {QStringLiteral("type"), QStringLiteral("object")},
                {QStringLiteral("additionalProperties"), schema},
            };
            const QJsonObject schemaArray{
                {QStringLiteral("type"), QStringLiteral("array")},
                {QStringLiteral("items"), schema},
                {QStringLiteral("minItems"), 1},
            };
            const auto schemaObject = strictObjectSchema(QJsonObject{
                {QStringLiteral("$schema"), stringSchema()},
                {QStringLiteral("$id"), stringSchema()},
                {QStringLiteral("$anchor"), stringSchema()},
                {QStringLiteral("$defs"), schemaMap},
                {QStringLiteral("$ref"), stringSchema()},
                {QStringLiteral("$comment"), stringSchema()},
                {QStringLiteral("title"), stringSchema()},
                {QStringLiteral("description"), stringSchema()},
                {QStringLiteral("default"), value},
                {QStringLiteral("examples"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                             {QStringLiteral("items"), value}}},
                {QStringLiteral("deprecated"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                {QStringLiteral("readOnly"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                {QStringLiteral("writeOnly"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                {QStringLiteral("type"),
                 QJsonObject{{QStringLiteral("oneOf"), QJsonArray{typeName, typeNames}}}},
                {QStringLiteral("properties"), schemaMap},
                {QStringLiteral("required"), stringArraySchema()},
                {QStringLiteral("additionalProperties"), schema},
                {QStringLiteral("items"), schema},
                {QStringLiteral("prefixItems"), schemaArray},
                {QStringLiteral("contains"), schema},
                {QStringLiteral("enum"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                             {QStringLiteral("items"), value},
                             {QStringLiteral("minItems"), 1}}},
                {QStringLiteral("const"), value},
                {QStringLiteral("minimum"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                {QStringLiteral("maximum"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                {QStringLiteral("exclusiveMinimum"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                {QStringLiteral("exclusiveMaximum"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                {QStringLiteral("multipleOf"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("number")}}},
                {QStringLiteral("minItems"), integerSchema()},
                {QStringLiteral("maxItems"), integerSchema()},
                {QStringLiteral("uniqueItems"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                {QStringLiteral("minLength"), integerSchema()},
                {QStringLiteral("maxLength"), integerSchema()},
                {QStringLiteral("pattern"), stringSchema()},
                {QStringLiteral("format"), stringSchema()},
                {QStringLiteral("minProperties"), integerSchema()},
                {QStringLiteral("maxProperties"), integerSchema()},
                {QStringLiteral("oneOf"), schemaArray},
                {QStringLiteral("anyOf"), schemaArray},
                {QStringLiteral("allOf"), schemaArray},
                {QStringLiteral("not"), schema},
                {QStringLiteral("if"), schema},
                {QStringLiteral("then"), schema},
                {QStringLiteral("else"), schema},
                {QStringLiteral("x-mcp-header"), stringSchema()},
            });
            return {
                {QStringLiteral("oneOf"),
                 QJsonArray{
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}},
                     schemaObject,
                 }},
            };
        }

        void attachSchemaDefinitions(QJsonObject &schema) {
            schema.insert(
                QStringLiteral("$defs"),
                QJsonObject{{QStringLiteral("json_value"), jsonValueMetaSchema()},
                            {QStringLiteral("schema"), jsonSchemaMetaSchema()}});
        }

        QJsonObject toolDescriptorSchema() {
            const QJsonObject boolean{
                {QStringLiteral("type"), QStringLiteral("boolean")},
            };
            auto annotations = strictObjectSchema(QJsonObject{
                {QStringLiteral("title"), stringSchema()},
                {QStringLiteral("readOnlyHint"), boolean},
                {QStringLiteral("destructiveHint"), boolean},
                {QStringLiteral("idempotentHint"), boolean},
                {QStringLiteral("openWorldHint"), boolean},
                {QStringLiteral("toolsetVersion"), integerSchema(1)},
                {QStringLiteral("minimumCompatibleVersion"), integerSchema(1)},
                {QStringLiteral("category"), stringSchema()},
            });
            annotations.insert(QStringLiteral("additionalProperties"), true);
            auto result = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("name"), stringSchema()},
                    {QStringLiteral("title"), stringSchema()},
                    {QStringLiteral("description"), stringSchema()},
                    {QStringLiteral("inputSchema"),
                     schemaReference(QStringLiteral("schema"))},
                    {QStringLiteral("outputSchema"),
                     schemaReference(QStringLiteral("schema"))},
                    {QStringLiteral("annotations"), annotations},
                    {QStringLiteral("icons"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                 {QStringLiteral("items"), openObjectSchema()}}},
                    {QStringLiteral("_meta"), openObjectSchema()},
                    {QStringLiteral("availability"), stringSchema()},
                },
                QJsonArray{QStringLiteral("name"), QStringLiteral("inputSchema")});
            result.insert(QStringLiteral("additionalProperties"), true);
            return result;
        }

        QJsonObject toolDescriptorValidationSchema() {
            auto result = toolDescriptorSchema();
            attachSchemaDefinitions(result);
            return result;
        }

        bool validJsonSchema(const QJsonValue &value, QHash<QByteArray, bool> &cache) {
            const auto key = QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact);
            const auto found = cache.constFind(key);
            if (found != cache.constEnd())
                return found.value();
            const auto valid = AutomationWire::checkJsonSchema(value).valid();
            cache.insert(key, valid);
            return valid;
        }

        bool validToolDescriptor(const QJsonValue &value,
                                 QHash<QByteArray, bool> &schemaValidationCache) {
            if (!value.isObject())
                return false;
            const auto tool = value.toObject();
            if (!tool.value(QStringLiteral("name")).isString() ||
                tool.value(QStringLiteral("name")).toString().isEmpty() ||
                !tool.value(QStringLiteral("inputSchema")).isObject() ||
                tool.value(QStringLiteral("inputSchema"))
                        .toObject()
                        .value(QStringLiteral("type")) != QStringLiteral("object") ||
                (tool.contains(QStringLiteral("title")) &&
                 !tool.value(QStringLiteral("title")).isString()) ||
                (tool.contains(QStringLiteral("description")) &&
                 !tool.value(QStringLiteral("description")).isString()) ||
                (tool.contains(QStringLiteral("annotations")) &&
                 !tool.value(QStringLiteral("annotations")).isObject()) ||
                (tool.contains(QStringLiteral("icons")) &&
                 !tool.value(QStringLiteral("icons")).isArray()) ||
                (tool.contains(QStringLiteral("_meta")) &&
                 !tool.value(QStringLiteral("_meta")).isObject()) ||
                (tool.contains(QStringLiteral("availability")) &&
                 !tool.value(QStringLiteral("availability")).isString())) {
                return false;
            }
            if (!validJsonSchema(tool.value(QStringLiteral("inputSchema")),
                                 schemaValidationCache)) {
                return false;
            }
            return !tool.contains(QStringLiteral("outputSchema")) ||
                   (tool.value(QStringLiteral("outputSchema")).isObject() &&
                    validJsonSchema(tool.value(QStringLiteral("outputSchema")),
                                    schemaValidationCache));
        }

        bool validManifestOperation(const QJsonValue &value,
                                    QHash<QByteArray, bool> &schemaValidationCache) {
            if (!value.isObject())
                return false;
            const auto operation = value.toObject();
            const QStringList requiredStrings{
                QStringLiteral("operation_id"), QStringLiteral("title"),
                QStringLiteral("description"), QStringLiteral("category"),
                QStringLiteral("kind"), QStringLiteral("minimum_profile"),
                QStringLiteral("sync_mode"),
            };
            for (const auto &key : requiredStrings) {
                if (!operation.value(key).isString() || operation.value(key).toString().isEmpty())
                    return false;
            }
            if (operation.value(QStringLiteral("version")).toInteger(0) < 1 ||
                operation.value(QStringLiteral("minimum_compatible_version")).toInteger(0) < 1 ||
                !operation.value(QStringLiteral("value_sources")).isArray()) {
                return false;
            }
            return validJsonSchema(operation.value(QStringLiteral("input_schema")),
                                   schemaValidationCache) &&
                   validJsonSchema(operation.value(QStringLiteral("output_schema")),
                                   schemaValidationCache);
        }

        QJsonObject statusSchema() {
            const auto connector = strictObjectSchema(
                QJsonObject{{QStringLiteral("instance_id"), stringSchema()},
                            {QStringLiteral("version"), stringSchema()},
                            {QStringLiteral("executable_path"), stringSchema()},
                            {QStringLiteral("build_id"), stringSchema()}},
                QJsonArray{QStringLiteral("instance_id"), QStringLiteral("version"),
                           QStringLiteral("executable_path"), QStringLiteral("build_id")});
            const auto editor = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("state"),
                     enumStringSchema({QStringLiteral("not_running"),
                                       QStringLiteral("starting"),
                                       QStringLiteral("mcp_disabled"),
                                       QStringLiteral("mcp_starting"),
                                       QStringLiteral("mcp_ready"),
                                       QStringLiteral("mcp_stopping"),
                                       QStringLiteral("editor_stopping"),
                                       QStringLiteral("error")})},
                    {QStringLiteral("editor_instance_id"), stringSchema()},
                    {QStringLiteral("process_id"), integerSchema()},
                    {QStringLiteral("executable_path"), stringSchema()},
                    {QStringLiteral("application_version"), stringSchema()},
                    {QStringLiteral("build_id"), stringSchema()},
                    {QStringLiteral("host_mode"),
                     enumStringSchema({QString(), QStringLiteral("gui"),
                                       QStringLiteral("headless")})},
                    {QStringLiteral("error"), stringSchema()},
                },
                QJsonArray{QStringLiteral("state"), QStringLiteral("editor_instance_id"),
                           QStringLiteral("process_id"), QStringLiteral("executable_path"),
                           QStringLiteral("application_version"), QStringLiteral("build_id"),
                           QStringLiteral("host_mode"), QStringLiteral("error")});
            const auto bootstrap = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("connected"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                    {QStringLiteral("protocol_supported"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                    {QStringLiteral("protocol_version"), integerSchema()},
                    {QStringLiteral("error"), stringSchema()},
                },
                QJsonArray{QStringLiteral("connected"),
                           QStringLiteral("protocol_supported"),
                           QStringLiteral("protocol_version"), QStringLiteral("error")});
            const auto mcp = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("connected"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}},
                    {QStringLiteral("endpoint"), stringSchema()},
                    {QStringLiteral("protocol_version"),
                     enumStringSchema({QString(), QString::fromLatin1(
                                                       AutomationWire::Mcp::ProtocolVersion)})},
                    {QStringLiteral("error"), stringSchema()},
                    {QStringLiteral("pending_request_count"), integerSchema()},
                },
                QJsonArray{QStringLiteral("connected"), QStringLiteral("endpoint"),
                           QStringLiteral("protocol_version"), QStringLiteral("error"),
                           QStringLiteral("pending_request_count")});
            const auto manifest = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("compatibility"),
                     enumStringSchema({QStringLiteral("not_loaded"),
                                       QStringLiteral("refreshing"),
                                       QStringLiteral("manifest_unavailable"),
                                       QStringLiteral("compatible"),
                                       QStringLiteral("compatible_subset"),
                                       QStringLiteral("contract_incompatible")})},
                    {QStringLiteral("connector_toolset_version"), integerSchema(1)},
                    {QStringLiteral("editor_toolset_version"), integerSchema()},
                    {QStringLiteral("connector_digest"), digestSchema()},
                    {QStringLiteral("editor_digest"), digestSchema()},
                    {QStringLiteral("compatible_count"), integerSchema()},
                    {QStringLiteral("incompatible_count"), integerSchema()},
                    {QStringLiteral("unavailable_count"), integerSchema()},
                },
                QJsonArray{QStringLiteral("compatibility"),
                           QStringLiteral("connector_toolset_version"),
                           QStringLiteral("editor_toolset_version"),
                           QStringLiteral("connector_digest"),
                           QStringLiteral("editor_digest"),
                           QStringLiteral("compatible_count"),
                           QStringLiteral("incompatible_count"),
                           QStringLiteral("unavailable_count")});
            const auto exposure = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("profile"),
                     enumStringSchema({QStringLiteral("l0"), QStringLiteral("l1"),
                                       QStringLiteral("l2"), QStringLiteral("l3")})},
                    {QStringLiteral("includes"), stringArraySchema()},
                    {QStringLiteral("excludes"), stringArraySchema()},
                    {QStringLiteral("typed_tool_count"), integerSchema()},
                    {QStringLiteral("generic_target_count"), integerSchema()},
                    {QStringLiteral("pending_selectors"), stringArraySchema()},
                },
                QJsonArray{QStringLiteral("profile"), QStringLiteral("includes"),
                           QStringLiteral("excludes"), QStringLiteral("typed_tool_count"),
                           QStringLiteral("generic_target_count"),
                           QStringLiteral("pending_selectors")});
            return strictObjectSchema(
                QJsonObject{{QStringLiteral("connector"), connector},
                            {QStringLiteral("editor"), editor},
                            {QStringLiteral("bootstrap"), bootstrap},
                            {QStringLiteral("mcp"), mcp},
                            {QStringLiteral("manifest"), manifest},
                            {QStringLiteral("exposure"), exposure}},
                QJsonArray{QStringLiteral("connector"), QStringLiteral("editor"),
                           QStringLiteral("bootstrap"), QStringLiteral("mcp"),
                           QStringLiteral("manifest"), QStringLiteral("exposure")});
        }

        QJsonObject toolCollectionSchema(const bool paged) {
            QJsonObject properties{
                {QStringLiteral("toolset_version"), integerSchema()},
                {QStringLiteral("tools"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                             {QStringLiteral("items"), toolDescriptorSchema()}}},
                {QStringLiteral("manifest_digest"), stringSchema()},
            };
            if (paged)
                properties.insert(QStringLiteral("next_cursor"), stringSchema());
            auto result = strictObjectSchema(
                properties,
                QJsonArray{QStringLiteral("toolset_version"), QStringLiteral("tools"),
                           QStringLiteral("manifest_digest")});
            attachSchemaDefinitions(result);
            return result;
        }

        QJsonObject describeSchema() {
            auto result = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("tool"), toolDescriptorSchema()},
                    {QStringLiteral("version"), integerSchema()},
                    {QStringLiteral("minimum_compatible_version"), integerSchema(1)},
                    {QStringLiteral("input_schema"),
                     schemaReference(QStringLiteral("schema"))},
                    {QStringLiteral("output_schema"),
                     schemaReference(QStringLiteral("schema"))},
                    {QStringLiteral("manifest_digest"), stringSchema()},
                    {QStringLiteral("typed_compatibility"), stringSchema()},
                    {QStringLiteral("availability"), stringSchema()},
                    {QStringLiteral("minimum_profile"), stringSchema()},
                    {QStringLiteral("category"), stringSchema()},
                },
                QJsonArray{QStringLiteral("tool"), QStringLiteral("version"),
                           QStringLiteral("minimum_compatible_version"),
                           QStringLiteral("input_schema"), QStringLiteral("manifest_digest"),
                           QStringLiteral("typed_compatibility"),
                           QStringLiteral("availability"),
                           QStringLiteral("minimum_profile"), QStringLiteral("category")});
            attachSchemaDefinitions(result);
            return result;
        }

        QJsonObject bridgeTool(const QString &name, const QString &description,
                               const QJsonObject &inputSchema,
                               const QJsonObject &outputSchema) {
            return {
                {QStringLiteral("name"), name},
                {QStringLiteral("title"), name},
                {QStringLiteral("description"), description},
                {QStringLiteral("inputSchema"), inputSchema},
                {QStringLiteral("outputSchema"), outputSchema},
                {QStringLiteral("annotations"),
                 QJsonObject{
                     {QStringLiteral("readOnlyHint"),
                      name != QStringLiteral("connector.reconnect") &&
                          name != QStringLiteral("editor.tools.invoke")},
                     {QStringLiteral("toolsetVersion"),
                      static_cast<qint64>(AutomationWire::PublicToolsetVersion)},
                     {QStringLiteral("minimumCompatibleVersion"),
                      static_cast<qint64>(AutomationWire::PublicMinimumCompatibleVersion)},
                     {QStringLiteral("category"), QStringLiteral("connector")},
                 }},
            };
        }

        QString jsonString(const QJsonObject &object, const QString &camel,
                           const QString &snake = {}) {
            auto value = object.value(camel).toString();
            if (value.isEmpty() && !snake.isEmpty())
                value = object.value(snake).toString();
            return value;
        }

        qint64 jsonInteger(const QJsonObject &object, const QString &camel,
                           const QString &snake, const qint64 fallback) {
            const auto value = object.contains(camel) ? object.value(camel) : object.value(snake);
            return value.isDouble() ? value.toInteger(fallback) : fallback;
        }

        QJsonValue jsonValue(const QJsonObject &object, const QString &camel,
                             const QString &snake) {
            return object.contains(camel) ? object.value(camel) : object.value(snake);
        }

        QJsonObject toolDescriptor(const QJsonObject &tool) {
            auto annotations = tool.value(QStringLiteral("annotations")).toObject();
            const auto category = ExposurePolicy::category(tool);
            if (!category.isEmpty())
                annotations.insert(QStringLiteral("category"), category);
            QJsonObject result{
                {QStringLiteral("name"), ExposurePolicy::operationId(tool)},
                {QStringLiteral("inputSchema"),
                 jsonValue(tool, QStringLiteral("inputSchema"),
                           QStringLiteral("input_schema"))},
            };
            for (const auto &key : {QStringLiteral("title"), QStringLiteral("description")}) {
                const auto value = tool.value(key);
                if (value.isString())
                    result.insert(key, value);
            }
            const auto outputSchema = jsonValue(
                tool, QStringLiteral("outputSchema"), QStringLiteral("output_schema"));
            if (outputSchema.isObject())
                result.insert(QStringLiteral("outputSchema"), outputSchema);
            if (!annotations.isEmpty() || tool.contains(QStringLiteral("annotations")))
                result.insert(QStringLiteral("annotations"), annotations);
            for (const auto &key : {QStringLiteral("icons"), QStringLiteral("_meta")}) {
                if (tool.contains(key))
                    result.insert(key, tool.value(key));
            }
            const auto availability = tool.value(QStringLiteral("availability"));
            if (availability.isString())
                result.insert(QStringLiteral("availability"), availability);
            return result;
        }

        QJsonObject callArguments(const QString &name, const QJsonObject &arguments) {
            return {
                {QStringLiteral("name"), name},
                {QStringLiteral("arguments"), arguments},
            };
        }

        QJsonObject structuredContent(const UpstreamResult &result) {
            const auto value = result.result.value(QStringLiteral("structuredContent"));
            return value.isObject() ? value.toObject() : QJsonObject{};
        }

        QString handshakeError(const UpstreamResult &result, const QString &fallback) {
            if (!result.connectorError.isEmpty())
                return result.connectorError;
            if (result.protocolError)
                return result.protocolError->message;
            return fallback;
        }

        bool retryableHandshakeError(const QString &error) {
            return error == QStringLiteral("too_many_requests") ||
                   error == QStringLiteral("busy") ||
                   error == QStringLiteral("mcp_stopping") ||
                   error == QStringLiteral("request_timeout") ||
                   error == QStringLiteral("upstream_timeout") ||
                   error == QStringLiteral("upstream_transport_error");
        }
    }

    ConnectorRuntime::ConnectorRuntime(ConnectorOptions options, QString bootstrapServiceName,
                                       QObject *parent)
        : QObject(parent), m_options(std::move(options)), m_exposure(m_options),
          m_instanceId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
          m_version(QString::fromLatin1(LiteProductMetadata::Version)),
          m_bootstrap(new BootstrapWatcher(m_instanceId, m_version,
                                           std::move(bootstrapServiceName), this)),
          m_upstream(new UpstreamMcpClient(m_instanceId, m_version, this)),
          m_handshakeRetryTimer(new QTimer(this)) {
        m_handshakeRetryTimer->setSingleShot(true);
        connect(m_bootstrap, &BootstrapWatcher::observationChanged, this,
                &ConnectorRuntime::bootstrapChanged);
        connect(m_handshakeRetryTimer, &QTimer::timeout, this, [this] {
            if (m_handshakeInProgress && m_handshakeTarget)
                startHandshakeAttempt();
        });
    }

    void ConnectorRuntime::start() {
        m_bootstrap->start();
    }

    void ConnectorRuntime::stop() {
        m_bootstrap->stop();
        clearEditorState(QStringLiteral("connector_stopping"));
    }

    void ConnectorRuntime::reconnect() {
        clearEditorState(QStringLiteral("reconnecting"));
        m_bootstrap->reconnect();
    }

    QString ConnectorRuntime::instanceId() const {
        return m_instanceId;
    }

    QJsonObject ConnectorRuntime::status() const {
        const auto &observation = m_bootstrap->observation();
        const auto hasSnapshot = observation.connected && observation.snapshot.has_value();
        const auto editorStatus = hasSnapshot ? observation.snapshot->result
                                              : SingleInstanceAutomationStatus{};
        const auto editorState =
            hasSnapshot ? SingleInstanceProtocol::automationStateName(editorStatus.state)
                        : QStringLiteral("not_running");

        const auto &connectorContract = connectorManifest();
        const auto editorVersion = jsonInteger(
            m_manifest, QStringLiteral("toolsetVersion"), QStringLiteral("toolset_version"), 0);
        const auto editorDigest =
            jsonString(m_manifest, QStringLiteral("digest"), QStringLiteral("manifest_digest"));

        QJsonArray includes;
        QJsonArray excludes;
        for (const auto &selector : m_options.exposure.includes)
            includes.append(selector);
        for (const auto &selector : m_options.exposure.excludes)
            excludes.append(selector);
        QJsonArray pending;
        for (const auto &selector : m_exposure.pendingSelectors(m_actualTools))
            pending.append(selector);
        int compatibleCount = 0;
        int incompatibleCount = 0;
        int unavailableCount = 0;
        for (const auto &tool : m_exposure.typedContracts()) {
            const auto compatibility = compatibilityFor(tool);
            if (compatibility == QStringLiteral("compatible"))
                ++compatibleCount;
            else if (compatibility == QStringLiteral("contract_incompatible"))
                ++incompatibleCount;
            else
                ++unavailableCount;
        }

        return {
            {QStringLiteral("connector"),
             QJsonObject{{QStringLiteral("instance_id"), m_instanceId},
                         {QStringLiteral("version"), m_version},
                         {QStringLiteral("executable_path"),
                          QCoreApplication::applicationFilePath()},
                         {QStringLiteral("build_id"), connectorBuildId()}}},
            {QStringLiteral("editor"),
             QJsonObject{{QStringLiteral("state"), editorState},
                         {QStringLiteral("editor_instance_id"), editorStatus.editorInstanceId},
                         {QStringLiteral("process_id"),
                          hasSnapshot ? observation.snapshot->primaryProcessId : 0},
                         {QStringLiteral("executable_path"), editorStatus.executablePath},
                         {QStringLiteral("application_version"), editorStatus.applicationVersion},
                         {QStringLiteral("build_id"), editorStatus.buildId},
                         {QStringLiteral("host_mode"), editorStatus.hostMode},
                         {QStringLiteral("error"), editorStatus.error}}},
            {QStringLiteral("bootstrap"),
             QJsonObject{{QStringLiteral("connected"), observation.connected},
                         {QStringLiteral("protocol_supported"), observation.protocolSupported},
                         {QStringLiteral("protocol_version"),
                          SingleInstanceProtocol::protocolVersion},
                         {QStringLiteral("error"), observation.error}}},
            {QStringLiteral("mcp"),
             QJsonObject{{QStringLiteral("connected"), m_mcpConnected},
                         {QStringLiteral("endpoint"), m_upstream->endpoint().toString()},
                         {QStringLiteral("protocol_version"),
                          m_mcpConnected
                              ? QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion)
                              : QString()},
                         {QStringLiteral("error"), m_mcpError},
                         {QStringLiteral("pending_request_count"), m_upstream->pendingCount()}}},
            {QStringLiteral("manifest"),
             QJsonObject{{QStringLiteral("compatibility"), m_manifestCompatibility},
                         {QStringLiteral("connector_toolset_version"),
                          static_cast<qint64>(AutomationWire::PublicToolsetVersion)},
                         {QStringLiteral("editor_toolset_version"), editorVersion},
                         {QStringLiteral("connector_digest"), connectorContract.digest},
                         {QStringLiteral("editor_digest"), editorDigest},
                         {QStringLiteral("compatible_count"), compatibleCount},
                         {QStringLiteral("incompatible_count"), incompatibleCount},
                         {QStringLiteral("unavailable_count"), unavailableCount}}},
            {QStringLiteral("exposure"),
             QJsonObject{{QStringLiteral("profile"),
                          AutomationWire::exposureProfileName(m_options.exposure.profile)},
                         {QStringLiteral("includes"), includes},
                         {QStringLiteral("excludes"), excludes},
                         {QStringLiteral("typed_tool_count"), m_exposure.typedContracts().size()},
                         {QStringLiteral("generic_target_count"), filteredActualTools().size()},
                         {QStringLiteral("pending_selectors"), pending}}},
        };
    }

    QJsonArray ConnectorRuntime::downstreamTools() const {
        auto result = bridgeToolDefinitions();
        for (const auto &contract : m_exposure.typedContracts())
            result.append(contract.toMcpToolJson());
        return result;
    }

    const ExposurePolicy &ConnectorRuntime::exposurePolicy() const {
        return m_exposure;
    }

    qint64 ConnectorRuntime::callTool(const QString &name, const QJsonObject &arguments,
                                      ToolCallCallback callback) {
        if (name == QStringLiteral("connector.get_status")) {
            callback({AutomationWire::Mcp::makeToolCallResult(status())});
            return 0;
        }
        if (name == QStringLiteral("connector.reconnect")) {
            reconnect();
            callback({AutomationWire::Mcp::makeToolCallResult(status())});
            return 0;
        }
        if (name == QStringLiteral("editor.tools.list")) {
            callback(listActualTools(arguments));
            return 0;
        }
        if (name == QStringLiteral("editor.tools.search")) {
            callback(searchActualTools(arguments));
            return 0;
        }
        if (name == QStringLiteral("editor.tools.describe")) {
            callback(describeActualTool(arguments));
            return 0;
        }
        if (name == QStringLiteral("editor.tools.invoke")) {
            const auto targetName = arguments.value(QStringLiteral("name")).toString();
            const auto target = actualTool(targetName);
            if (!targetAllowed(target, targetName)) {
                callback(connectorError(QStringLiteral("connector_tool_filtered")));
                return 0;
            }
            if (target.isEmpty()) {
                const auto *knownTarget = AutomationWire::findPublicTool(targetName);
                callback(connectorError(knownTarget ? compatibilityFor(*knownTarget)
                                                    : QStringLiteral("tool_unavailable")));
                return 0;
            }
            const auto availability = actualAvailabilityCode(target);
            if (!availability.isEmpty()) {
                callback(connectorError(availability));
                return 0;
            }
            const auto targetArguments =
                arguments.value(QStringLiteral("arguments")).toObject();
            const auto inputSchema = jsonValue(target, QStringLiteral("inputSchema"),
                                               QStringLiteral("input_schema"));
            if (!inputSchema.isObject() ||
                !AutomationWire::validateJsonValue(targetArguments, inputSchema).valid()) {
                callback(connectorError(QStringLiteral("invalid_editor_tool_arguments")));
                return 0;
            }
            const auto outputSchema = jsonValue(target, QStringLiteral("outputSchema"),
                                                QStringLiteral("output_schema"));
            return forwardEditorTool(
                targetName, targetArguments,
                [this, outputSchema, callback = std::move(callback)](
                    ToolCallOutcome outcome) mutable {
                    if (!outcome.protocolError &&
                        !outcome.result.value(QStringLiteral("isError")).toBool() &&
                        outputSchema.isObject() &&
                        !AutomationWire::validateJsonValue(
                             outcome.result.value(QStringLiteral("structuredContent")),
                             outputSchema)
                             .valid()) {
                        outcome = connectorError(
                            QStringLiteral("invalid_upstream_output"),
                            QStringLiteral("Tool result does not match outputSchema"));
                    }
                    callback(std::move(outcome));
                });
        }

        const auto *known = AutomationWire::findPublicTool(name);
        if (!known) {
            callback(connectorError(QStringLiteral("tool_unavailable")));
            return 0;
        }
        if (!m_exposure.allowsKnownTool(*known)) {
            callback(connectorError(QStringLiteral("connector_tool_filtered")));
            return 0;
        }
        const auto unavailable = editorUnavailableCode();
        if (!unavailable.isEmpty()) {
            callback(connectorError(unavailable));
            return 0;
        }
        if (m_manifest.isEmpty()) {
            callback(connectorError(QStringLiteral("manifest_unavailable")));
            return 0;
        }
        const auto compatibility = compatibilityFor(*known);
        if (compatibility != QStringLiteral("compatible")) {
            callback(connectorError(compatibility));
            return 0;
        }
        return forwardEditorTool(name, arguments, std::move(callback));
    }

    bool ConnectorRuntime::cancel(const qint64 requestToken) {
        return m_upstream->cancel(requestToken);
    }

    const QStringList &ConnectorRuntime::bridgeToolNames() {
        static const QStringList names{
            QStringLiteral("connector.get_status"), QStringLiteral("connector.reconnect"),
            QStringLiteral("editor.tools.list"),    QStringLiteral("editor.tools.search"),
            QStringLiteral("editor.tools.describe"), QStringLiteral("editor.tools.invoke"),
        };
        return names;
    }

    QJsonArray ConnectorRuntime::bridgeToolDefinitions() {
        static const QJsonArray definitions{
            bridgeTool(QStringLiteral("connector.get_status"),
                       QStringLiteral("Return connector, editor, bootstrap, MCP, manifest, and "
                                      "exposure facts."),
                       emptyObjectSchema(), statusSchema()),
            bridgeTool(QStringLiteral("connector.reconnect"),
                       QStringLiteral("Restart bootstrap discovery and the editor MCP handshake."),
                       emptyObjectSchema(), statusSchema()),
            bridgeTool(
                QStringLiteral("editor.tools.list"),
                QStringLiteral("List current editor targets allowed by connector exposure."),
                strictObjectSchema(
                    QJsonObject{
                        {QStringLiteral("cursor"),
                         QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                        {QStringLiteral("limit"),
                         QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                                     {QStringLiteral("minimum"), 1},
                                     {QStringLiteral("maximum"), 200}}},
                    }),
                toolCollectionSchema(true)),
            bridgeTool(
                QStringLiteral("editor.tools.search"),
                QStringLiteral("Search current editor targets by name, description, and category."),
                strictObjectSchema(
                    QJsonObject{
                        {QStringLiteral("query"),
                         QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                     {QStringLiteral("minLength"), 1}}},
                        {QStringLiteral("category"),
                         QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                        {QStringLiteral("limit"),
                         QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                                     {QStringLiteral("minimum"), 1},
                                     {QStringLiteral("maximum"), 200}}},
                    },
                    QJsonArray{QStringLiteral("query")}),
                toolCollectionSchema(false)),
            bridgeTool(
                QStringLiteral("editor.tools.describe"),
                QStringLiteral("Describe one current editor target and its actual schemas."),
                strictObjectSchema(
                    QJsonObject{{QStringLiteral("name"),
                                 QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                             {QStringLiteral("minLength"), 1}}}},
                    QJsonArray{QStringLiteral("name")}),
                describeSchema()),
            bridgeTool(
                QStringLiteral("editor.tools.invoke"),
                QStringLiteral("Invoke one current editor target through its actual schema."),
                strictObjectSchema(
                    QJsonObject{
                        {QStringLiteral("name"),
                         QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                     {QStringLiteral("minLength"), 1}}},
                        {QStringLiteral("arguments"),
                         QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                                     {QStringLiteral("additionalProperties"), true}}},
                    },
                    QJsonArray{QStringLiteral("name"), QStringLiteral("arguments")}),
                QJsonObject{}),
        };
        return definitions;
    }

    std::optional<QJsonObject> ConnectorRuntime::findBridgeTool(const QString &name) {
        for (const auto &entry : bridgeToolDefinitions()) {
            if (entry.toObject().value(QStringLiteral("name")).toString() == name)
                return entry.toObject();
        }
        return std::nullopt;
    }

    void ConnectorRuntime::bootstrapChanged() {
        const auto &observation = m_bootstrap->observation();
        if (!observation.connected) {
            clearEditorState(observation.error.isEmpty()
                                 ? QStringLiteral("bootstrap_disconnected")
                                 : observation.error);
            emit statusChanged();
            return;
        }
        if (!observation.snapshot) {
            emit statusChanged();
            return;
        }
        if (observation.snapshotSequence == m_handledSnapshotSequence)
            return;
        m_handledSnapshotSequence = observation.snapshotSequence;

        const auto &snapshot = *observation.snapshot;
        if (m_editorInstanceId != snapshot.result.editorInstanceId) {
            m_editorInstanceId = snapshot.result.editorInstanceId;
            clearEditorState(QStringLiteral("editor_instance_changed"));
        }
        if (snapshot.result.state == SingleInstanceAutomationState::McpReady) {
            beginHandshake(snapshot);
        } else {
            clearEditorState(editorUnavailableCode());
        }
        emit statusChanged();
    }

    void ConnectorRuntime::clearEditorState(const QString &error) {
        ++m_handshakeEpoch;
        m_handshakeRetryTimer->stop();
        m_handshakeTarget.reset();
        m_handshakeInProgress = false;
        m_handshakeFollowUp = false;
        m_handshakeRefreshPending = false;
        m_handshakeRetryAttempt = 0;
        m_upstream->clearEndpoint(error);
        m_actualTools = {};
        m_manifest = {};
        m_manifestCompatibility = QStringLiteral("not_loaded");
        m_mcpConnected = false;
        m_mcpError = error;
    }

    void ConnectorRuntime::beginHandshake(const SingleInstanceAutomationSnapshot &snapshot) {
        const auto sameTarget = m_handshakeTarget &&
                                m_handshakeTarget->primaryProcessId == snapshot.primaryProcessId &&
                                m_handshakeTarget->result.editorInstanceId ==
                                    snapshot.result.editorInstanceId &&
                                m_handshakeTarget->result.mcpEndpoint == snapshot.result.mcpEndpoint;
        if (!sameTarget) {
            ++m_handshakeEpoch;
            m_handshakeRetryTimer->stop();
            m_handshakeInProgress = false;
            m_handshakeFollowUp = false;
            m_handshakeRefreshPending = false;
            m_handshakeRetryAttempt = 0;
            m_upstream->clearEndpoint(QStringLiteral("editor_endpoint_changed"));
        }
        m_handshakeTarget = snapshot;
        if (m_handshakeInProgress) {
            if (!m_handshakeFollowUp)
                m_handshakeRefreshPending = true;
            return;
        }
        m_handshakeRetryAttempt = 0;
        m_handshakeInProgress = true;
        m_handshakeFollowUp = false;
        startHandshakeAttempt();
    }

    void ConnectorRuntime::startHandshakeAttempt() {
        if (!m_handshakeInProgress || !m_handshakeTarget)
            return;
        const auto epoch = ++m_handshakeEpoch;
        m_mcpConnected = false;
        m_mcpError.clear();
        m_actualTools = {};
        m_manifest = {};
        m_schemaValidationCache.clear();
        m_manifestCompatibility = QStringLiteral("refreshing");
        m_upstream->clearEndpoint(QStringLiteral("refreshing"));
        QString endpointError;
        if (!m_upstream->setEndpoint(m_handshakeTarget->result.mcpEndpoint, &endpointError)) {
            failHandshake(
                epoch, QStringLiteral("invalid_discovered_endpoint: %1").arg(endpointError));
            return;
        }
        emit statusChanged();
        m_upstream->send(QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {},
                         [this, epoch](const UpstreamResult result) {
                             if (epoch != m_handshakeEpoch)
                                 return;
                             if (!result.succeeded()) {
                                 failHandshake(epoch,
                                               handshakeError(result, QStringLiteral(
                                                                          "upstream_discovery_failed")));
                                 return;
                             }
                             const auto supportedVersions =
                                 result.result.value(QStringLiteral("supportedVersions"));
                             const auto capabilities =
                                 result.result.value(QStringLiteral("capabilities"));
                             bool protocolSupported = supportedVersions.isArray();
                             for (const auto &version : supportedVersions.toArray()) {
                                 protocolSupported &= version.isString();
                             }
                             protocolSupported &=
                                 supportedVersions.toArray().contains(QString::fromLatin1(
                                     AutomationWire::Mcp::ProtocolVersion));
                             if (!protocolSupported || !capabilities.isObject() ||
                                 !capabilities.toObject()
                                      .value(QStringLiteral("tools"))
                                      .isObject()) {
                                 failHandshake(epoch,
                                               QStringLiteral("invalid_upstream_discovery"));
                                 return;
                             }
                             m_mcpConnected = true;
                             requestToolsPage(epoch, {}, {}, {}, 0);
                         },
                         m_options.upstreamTimeoutMs);
    }

    void ConnectorRuntime::failHandshake(const quint64 epoch, const QString &error,
                                         const QString &manifestCompatibility,
                                         const bool preserveMcpConnection) {
        if (epoch != m_handshakeEpoch)
            return;
        if (!preserveMcpConnection)
            m_mcpConnected = false;
        m_mcpError = error;
        m_manifestCompatibility = manifestCompatibility;
        emit statusChanged();
        if (retryableHandshakeError(error) &&
            m_handshakeRetryAttempt < MaxHandshakeRetryAttempts) {
            const auto delay = std::min(
                MaximumHandshakeRetryDelayMs,
                InitialHandshakeRetryDelayMs * (1 << m_handshakeRetryAttempt));
            ++m_handshakeRetryAttempt;
            m_handshakeRetryTimer->start(delay);
            return;
        }
        completeHandshakeCycle(epoch, false);
    }

    void ConnectorRuntime::completeHandshakeCycle(const quint64 epoch, const bool succeeded) {
        if (epoch != m_handshakeEpoch)
            return;
        m_handshakeRetryTimer->stop();
        m_handshakeInProgress = false;
        if (succeeded)
            m_handshakeRetryAttempt = 0;
        const auto refresh = m_handshakeRefreshPending && !m_handshakeFollowUp;
        m_handshakeRefreshPending = false;
        m_handshakeFollowUp = false;
        if (!refresh || !m_handshakeTarget)
            return;
        m_handshakeInProgress = true;
        m_handshakeFollowUp = true;
        startHandshakeAttempt();
    }

    void ConnectorRuntime::requestToolsPage(const quint64 epoch, const QString &cursor,
                                            QJsonArray accumulated,
                                            QSet<QString> seenCursors, const int pageCount) {
        if (epoch != m_handshakeEpoch)
            return;
        if (pageCount >= MaxHandshakePages) {
            failHandshake(epoch, QStringLiteral("upstream_tools_pagination_limit"),
                          QStringLiteral("not_loaded"), true);
            return;
        }
        QJsonObject params;
        if (!cursor.isEmpty())
            params.insert(QStringLiteral("cursor"), cursor);
        m_upstream->send(
            QString::fromLatin1(AutomationWire::Mcp::ToolsListMethod), params,
            [this, epoch, accumulated = std::move(accumulated),
             seenCursors = std::move(seenCursors), pageCount](
                const UpstreamResult listResult) mutable {
                if (epoch != m_handshakeEpoch)
                    return;
                if (!listResult.succeeded()) {
                    failHandshake(epoch,
                                  handshakeError(listResult,
                                                 QStringLiteral("upstream_tools_list_failed")),
                                  QStringLiteral("not_loaded"), true);
                    return;
                }
                const auto toolsValue = listResult.result.value(QStringLiteral("tools"));
                if (!toolsValue.isArray()) {
                    failHandshake(epoch, QStringLiteral("invalid_upstream_tools_page"),
                                  QStringLiteral("not_loaded"), true);
                    return;
                }
                QSet<QString> names;
                for (const auto &tool : std::as_const(accumulated))
                    names.insert(tool.toObject().value(QStringLiteral("name")).toString());
                for (const auto &tool : toolsValue.toArray()) {
                    const auto name = tool.toObject().value(QStringLiteral("name")).toString();
                    if (!validToolDescriptor(tool, m_schemaValidationCache) || name.isEmpty() ||
                        names.contains(name)) {
                        failHandshake(epoch,
                                      QStringLiteral("invalid_upstream_tool_descriptor"),
                                      QStringLiteral("not_loaded"), true);
                        return;
                    }
                    names.insert(name);
                    QList<HeaderBinding> headerBindings;
                    QString headerError;
                    if (!collectHeaderBindings(
                            tool.toObject().value(QStringLiteral("inputSchema")).toObject(),
                            headerBindings, headerError)) {
                        qWarning().noquote()
                            << QStringLiteral("DsConnectorLite: rejected upstream tool %1: %2")
                                   .arg(name, headerError);
                        continue;
                    }
                    accumulated.append(tool);
                }
                if (accumulated.size() > MaxHandshakeItems) {
                    failHandshake(epoch, QStringLiteral("upstream_tools_pagination_limit"),
                                  QStringLiteral("not_loaded"), true);
                    return;
                }
                auto nextValue = listResult.result.value(QStringLiteral("nextCursor"));
                if (nextValue.isUndefined())
                    nextValue = listResult.result.value(QStringLiteral("next_cursor"));
                if (!nextValue.isUndefined() && !nextValue.isNull() && !nextValue.isString()) {
                    failHandshake(epoch, QStringLiteral("invalid_upstream_tools_cursor"),
                                  QStringLiteral("not_loaded"), true);
                    return;
                }
                const auto next = nextValue.toString();
                if (!next.isEmpty()) {
                    if (seenCursors.contains(next)) {
                        failHandshake(epoch, QStringLiteral("upstream_tools_cursor_cycle"),
                                      QStringLiteral("not_loaded"), true);
                        return;
                    }
                    seenCursors.insert(next);
                    requestToolsPage(epoch, next, std::move(accumulated),
                                     std::move(seenCursors), pageCount + 1);
                    return;
                }
                m_actualTools = std::move(accumulated);
                requestManifest(epoch);
            },
            m_options.upstreamTimeoutMs);
    }

    void ConnectorRuntime::requestManifest(const quint64 epoch) {
        if (actualTool(QStringLiteral("automation.get_manifest")).isEmpty()) {
            finishHandshake(epoch);
            return;
        }
        requestManifestPage(epoch, {}, {}, {}, 0);
    }

    void ConnectorRuntime::requestManifestPage(const quint64 epoch, const QString &cursor,
                                               QJsonObject accumulated,
                                               QSet<QString> seenCursors,
                                               const int pageCount) {
        if (epoch != m_handshakeEpoch)
            return;
        if (pageCount >= MaxHandshakePages) {
            failHandshake(epoch, QStringLiteral("upstream_manifest_pagination_limit"),
                          QStringLiteral("manifest_unavailable"), true);
            return;
        }
        QJsonObject arguments;
        if (!cursor.isEmpty())
            arguments.insert(QStringLiteral("cursor"), cursor);
        QHash<QByteArray, QByteArray> parameterHeaderValues;
        QString parameterHeaderError;
        const auto manifestSchema = jsonValue(
            actualTool(QStringLiteral("automation.get_manifest")),
            QStringLiteral("inputSchema"), QStringLiteral("input_schema"));
        if (!manifestSchema.isObject() ||
            !parameterHeaders(manifestSchema.toObject(), arguments, parameterHeaderValues,
                              parameterHeaderError)) {
            finishHandshake(epoch,
                            UpstreamResult{.connectorError =
                                               QStringLiteral("invalid_tool_header_mapping")});
            return;
        }
        m_upstream->send(
            QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
            callArguments(QStringLiteral("automation.get_manifest"), arguments),
            [this, epoch, accumulated = std::move(accumulated),
             seenCursors = std::move(seenCursors), pageCount](
                const UpstreamResult result) mutable {
                if (epoch != m_handshakeEpoch)
                    return;
                const auto page = structuredContent(result);
                const auto operationsValue = page.value(QStringLiteral("operations"));
                const auto failManifest = [this, epoch](const QString &error) {
                    finishHandshake(epoch, UpstreamResult{.connectorError = error});
                };
                if (!result.succeeded() ||
                    result.result.value(QStringLiteral("isError")).toBool() ||
                    page.isEmpty() || !operationsValue.isArray()) {
                    finishHandshake(epoch, result);
                    return;
                }
                const auto version = jsonInteger(
                    page, QStringLiteral("toolsetVersion"),
                    QStringLiteral("toolset_version"), 0);
                const auto digest = jsonString(page, QStringLiteral("digest"),
                                               QStringLiteral("manifest_digest"));
                const auto profile = page.value(QStringLiteral("profile")).toString();
                const auto hostMode = jsonString(page, QStringLiteral("hostMode"),
                                                 QStringLiteral("host_mode"));
                if (version < 1 || digest.size() != 71 ||
                    !digest.startsWith(QStringLiteral("sha256:")) ||
                    !AutomationWire::automationProfileFromName(profile) ||
                    (hostMode != QStringLiteral("gui") &&
                     hostMode != QStringLiteral("headless")) ||
                    !page.value(QStringLiteral("extensions")).isObject()) {
                    failManifest(QStringLiteral("invalid_upstream_manifest_root"));
                    return;
                }

                if (pageCount == 0) {
                    accumulated = page;
                    accumulated.insert(QStringLiteral("operations"), QJsonArray{});
                } else {
                    const auto expectedVersion = jsonInteger(
                        accumulated, QStringLiteral("toolsetVersion"),
                        QStringLiteral("toolset_version"), -1);
                    const auto pageVersion = jsonInteger(
                        page, QStringLiteral("toolsetVersion"),
                        QStringLiteral("toolset_version"), -2);
                    const auto expectedDigest = jsonString(
                        accumulated, QStringLiteral("digest"),
                        QStringLiteral("manifest_digest"));
                    const auto pageDigest = jsonString(
                        page, QStringLiteral("digest"), QStringLiteral("manifest_digest"));
                    if (expectedVersion != pageVersion || expectedDigest != pageDigest ||
                        accumulated.value(QStringLiteral("profile")) !=
                            page.value(QStringLiteral("profile")) ||
                        jsonString(accumulated, QStringLiteral("hostMode"),
                                   QStringLiteral("host_mode")) != hostMode ||
                        accumulated.value(QStringLiteral("extensions")) !=
                            page.value(QStringLiteral("extensions"))) {
                        failManifest(QStringLiteral("inconsistent_upstream_manifest_page"));
                        return;
                    }
                }

                auto operations = accumulated.value(QStringLiteral("operations")).toArray();
                QSet<QString> operationIds;
                for (const auto &operation : std::as_const(operations)) {
                    operationIds.insert(
                        operation.toObject().value(QStringLiteral("operation_id")).toString());
                }
                for (const auto &operation : operationsValue.toArray()) {
                    const auto operationId =
                        operation.toObject().value(QStringLiteral("operation_id")).toString();
                    if (!validManifestOperation(operation, m_schemaValidationCache) ||
                        operationIds.contains(operationId)) {
                        failManifest(QStringLiteral("invalid_upstream_manifest_operation"));
                        return;
                    }
                    operationIds.insert(operationId);
                    operations.append(operation);
                }
                if (operations.size() > MaxHandshakeItems) {
                    failManifest(QStringLiteral("upstream_manifest_pagination_limit"));
                    return;
                }
                accumulated.insert(QStringLiteral("operations"), operations);

                auto nextValue = page.value(QStringLiteral("next_cursor"));
                if (nextValue.isUndefined())
                    nextValue = page.value(QStringLiteral("nextCursor"));
                if (!nextValue.isUndefined() && !nextValue.isNull() && !nextValue.isString()) {
                    failManifest(QStringLiteral("invalid_upstream_manifest_cursor"));
                    return;
                }
                const auto next = nextValue.toString();
                if (!next.isEmpty()) {
                    if (seenCursors.contains(next)) {
                        failManifest(QStringLiteral("upstream_manifest_cursor_cycle"));
                        return;
                    }
                    seenCursors.insert(next);
                    requestManifestPage(epoch, next, std::move(accumulated),
                                        std::move(seenCursors), pageCount + 1);
                    return;
                }
                accumulated.remove(QStringLiteral("next_cursor"));
                accumulated.remove(QStringLiteral("nextCursor"));
                UpstreamResult complete;
                complete.result = AutomationWire::Mcp::makeToolCallResult(accumulated);
                finishHandshake(epoch, complete);
            },
            m_options.upstreamTimeoutMs, parameterHeaderValues);
    }

    void ConnectorRuntime::finishHandshake(const quint64 epoch,
                                           const UpstreamResult &manifestResult) {
        if (epoch != m_handshakeEpoch)
            return;
        const auto manifest = structuredContent(manifestResult);
        if (!manifestResult.succeeded() ||
            manifestResult.result.value(QStringLiteral("isError")).toBool() ||
            manifest.isEmpty()) {
            m_manifest = {};
            failHandshake(epoch,
                          handshakeError(manifestResult,
                                         QStringLiteral("manifest_unavailable")),
                          QStringLiteral("manifest_unavailable"), true);
            return;
        }
        m_manifest = manifest;

        int compatibleCount = 0;
        int consideredCount = 0;
        for (const auto &tool : m_exposure.typedContracts()) {
            ++consideredCount;
            if (compatibilityFor(tool) == QStringLiteral("compatible"))
                ++compatibleCount;
        }
        const auto editorVersion = jsonInteger(
            m_manifest, QStringLiteral("toolsetVersion"), QStringLiteral("toolset_version"), 0);
        const auto editorDigest =
            jsonString(m_manifest, QStringLiteral("digest"), QStringLiteral("manifest_digest"));
        const auto &connectorContract = connectorManifest();
        if (consideredCount == 0) {
            m_manifestCompatibility = QStringLiteral("compatible_subset");
        } else if (compatibleCount == consideredCount &&
            editorVersion == static_cast<qint64>(AutomationWire::PublicToolsetVersion) &&
            !editorDigest.isEmpty() && editorDigest == connectorContract.digest) {
            m_manifestCompatibility = QStringLiteral("compatible");
        } else if (compatibleCount > 0) {
            m_manifestCompatibility = QStringLiteral("compatible_subset");
        } else {
            m_manifestCompatibility = QStringLiteral("contract_incompatible");
        }
        m_mcpError.clear();
        emit statusChanged();
        completeHandshakeCycle(epoch, true);
    }

    QString ConnectorRuntime::editorUnavailableCode() const {
        const auto &observation = m_bootstrap->observation();
        if (!observation.connected || !observation.snapshot)
            return QStringLiteral("editor_not_running");
        switch (observation.snapshot->result.state) {
            case SingleInstanceAutomationState::Starting:
                return QStringLiteral("editor_starting");
            case SingleInstanceAutomationState::McpDisabled:
                return QStringLiteral("mcp_disabled");
            case SingleInstanceAutomationState::McpStarting:
                return QStringLiteral("mcp_starting");
            case SingleInstanceAutomationState::McpStopping:
                return QStringLiteral("mcp_stopping");
            case SingleInstanceAutomationState::EditorStopping:
                return QStringLiteral("editor_not_connected");
            case SingleInstanceAutomationState::Error:
                return QStringLiteral("editor_error");
            case SingleInstanceAutomationState::McpReady:
                return m_mcpConnected ? QString() : QStringLiteral("editor_not_connected");
        }
        return QStringLiteral("editor_not_connected");
    }

    QString ConnectorRuntime::actualAvailabilityCode(const QJsonObject &tool) const {
        const auto availability = tool.value(QStringLiteral("availability")).toString();
        if (!availability.isEmpty() && availability != QStringLiteral("available") &&
            availability != QStringLiteral("compatible")) {
            return availability;
        }
        const auto hostAvailability =
            tool.value(QStringLiteral("host_availability")).toString();
        const auto &observation = m_bootstrap->observation();
        if (!hostAvailability.isEmpty() && hostAvailability != QStringLiteral("both") &&
            observation.snapshot && hostAvailability != observation.snapshot->result.hostMode) {
            return QStringLiteral("host_unavailable");
        }
        return {};
    }

    QJsonArray ConnectorRuntime::filteredActualTools() const {
        QJsonArray enriched;
        for (const auto &entry : m_actualTools) {
            if (!entry.isObject())
                continue;
            auto tool = entry.toObject();
            const auto id = ExposurePolicy::operationId(tool);
            const auto manifest = manifestOperation(id);
            for (auto it = manifest.constBegin(); it != manifest.constEnd(); ++it) {
                if (!tool.contains(it.key()))
                    tool.insert(it.key(), it.value());
            }
            if (actualAvailabilityCode(tool).isEmpty())
                enriched.append(tool);
        }
        const auto allowed = m_exposure.filterActualTools(enriched);
        QJsonArray result;
        for (const auto &entry : allowed)
            result.append(toolDescriptor(entry.toObject()));
        return result;
    }

    QJsonObject ConnectorRuntime::actualTool(const QString &name) const {
        for (const auto &entry : m_actualTools) {
            const auto tool = entry.toObject();
            if (ExposurePolicy::operationId(tool) == name) {
                auto result = tool;
                const auto manifest = manifestOperation(name);
                for (auto it = manifest.constBegin(); it != manifest.constEnd(); ++it) {
                    if (!result.contains(it.key()))
                        result.insert(it.key(), it.value());
                }
                return result;
            }
        }
        return {};
    }

    QJsonObject ConnectorRuntime::manifestOperation(const QString &name) const {
        for (const auto &entry : m_manifest.value(QStringLiteral("operations")).toArray()) {
            const auto operation = entry.toObject();
            if (ExposurePolicy::operationId(operation) == name)
                return operation;
        }
        return {};
    }

    QString ConnectorRuntime::compatibilityFor(
        const AutomationWire::ToolContract &tool) const {
        const auto editorTool = actualTool(tool.operationId);
        if (editorTool.isEmpty()) {
            const auto unavailable = editorUnavailableCode();
            if (!unavailable.isEmpty())
                return unavailable;
            const auto profile = AutomationWire::automationProfileFromName(
                m_manifest.value(QStringLiteral("profile")).toString());
            if (profile && (*profile == AutomationWire::AutomationProfile::Custom ||
                            !AutomationWire::presetIncludes(*profile, tool.minimumProfile))) {
                return QStringLiteral("profile_blocked");
            }
            const auto &observation = m_bootstrap->observation();
            if (observation.snapshot &&
                observation.snapshot->result.hostMode == QStringLiteral("headless")) {
                return QStringLiteral("host_unavailable");
            }
            return QStringLiteral("tool_unavailable");
        }
        const auto availability = editorTool.value(QStringLiteral("availability")).toString();
        if (!availability.isEmpty() && availability != QStringLiteral("available") &&
            availability != QStringLiteral("compatible"))
            return availability;
        const auto hostAvailability =
            editorTool.value(QStringLiteral("host_availability")).toString();
        const auto &observation = m_bootstrap->observation();
        if (!hostAvailability.isEmpty() && hostAvailability != QStringLiteral("both") &&
            observation.snapshot && hostAvailability != observation.snapshot->result.hostMode) {
            return QStringLiteral("host_unavailable");
        }

        const auto editorVersion = jsonInteger(
            editorTool, QStringLiteral("version"), QStringLiteral("version"),
            jsonInteger(m_manifest, QStringLiteral("toolsetVersion"),
                        QStringLiteral("toolset_version"),
                        static_cast<qint64>(AutomationWire::PublicToolsetVersion)));
        const auto editorMinimum = jsonInteger(
            editorTool, QStringLiteral("minimumCompatibleVersion"),
            QStringLiteral("minimum_compatible_version"), 1);
        const auto versionCompatible =
            static_cast<qint64>(AutomationWire::PublicToolsetVersion) >= editorMinimum &&
            editorVersion >= static_cast<qint64>(AutomationWire::PublicMinimumCompatibleVersion);
        if (!versionCompatible)
            return QStringLiteral("contract_incompatible");

        const auto editorInput = jsonValue(editorTool, QStringLiteral("inputSchema"),
                                           QStringLiteral("input_schema"));
        const auto editorOutput = jsonValue(editorTool, QStringLiteral("outputSchema"),
                                            QStringLiteral("output_schema"));
        if (!editorInput.isObject() || !editorOutput.isObject())
            return QStringLiteral("contract_incompatible");
        const auto schemas = AutomationWire::checkToolSchemaCompatibility(
            tool.inputSchema, editorInput, editorOutput, tool.outputSchema);
        return schemas.compatible() ? QStringLiteral("compatible")
                                    : QStringLiteral("contract_incompatible");
    }

    bool ConnectorRuntime::targetAllowed(const QJsonObject &tool, const QString &name) const {
        if (!tool.isEmpty()) {
            return m_exposure.allowsTarget(name, ExposurePolicy::category(tool),
                                           ExposurePolicy::minimumProfile(tool));
        }
        const auto *known = AutomationWire::findPublicTool(name);
        return known && m_exposure.allowsKnownTool(*known);
    }

    ToolCallOutcome ConnectorRuntime::connectorError(const QString &code,
                                                     const QString &message) const {
        QJsonObject structured{{QStringLiteral("code"), code}};
        structured.insert(QStringLiteral("message"),
                          message.isEmpty() ? code : message);
        return {AutomationWire::Mcp::makeToolCallResult(structured, true)};
    }

    qint64 ConnectorRuntime::forwardEditorTool(const QString &name,
                                               const QJsonObject &arguments,
                                               ToolCallCallback callback) {
        const auto unavailable = editorUnavailableCode();
        if (!unavailable.isEmpty()) {
            callback(connectorError(unavailable));
            return 0;
        }
        const auto *known = AutomationWire::findPublicTool(name);
        const auto manifest = manifestOperation(name);
        const auto command = known ? known->kind == AutomationWire::OperationKind::Command
                                   : manifest.value(QStringLiteral("kind")).toString() ==
                                         QStringLiteral("command");
        QHash<QByteArray, QByteArray> parameterHeaderValues;
        QString parameterHeaderError;
        const auto inputSchema = jsonValue(actualTool(name), QStringLiteral("inputSchema"),
                                           QStringLiteral("input_schema"));
        if (!inputSchema.isObject() ||
            !parameterHeaders(inputSchema.toObject(), arguments, parameterHeaderValues,
                              parameterHeaderError)) {
            callback(connectorError(QStringLiteral("invalid_tool_header_mapping"),
                                    parameterHeaderError));
            return 0;
        }
        return m_upstream->send(
            QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
            callArguments(name, arguments),
            [this, callback = std::move(callback), command](const UpstreamResult result) mutable {
                if (result.protocolError) {
                    callback({{}, result.protocolError});
                    return;
                }
                if (!result.connectorError.isEmpty()) {
                    const auto code = command && result.outcomeUnknown
                                          ? QStringLiteral("outcome_unknown")
                                          : result.connectorError;
                    const auto message = command && result.outcomeUnknown
                                             ? result.connectorError
                                             : result.connectorErrorMessage.isEmpty()
                                                   ? result.connectorError
                                                   : result.connectorErrorMessage;
                    callback(connectorError(code, message));
                    return;
                }
                callback({result.result});
            },
            m_options.upstreamTimeoutMs, parameterHeaderValues);
    }

    ToolCallOutcome ConnectorRuntime::listActualTools(const QJsonObject &arguments) const {
        const auto tools = filteredActualTools();
        QString digestError;
        const auto snapshotDigest = AutomationWire::sha256Digest(tools, &digestError);
        if (!digestError.isEmpty())
            return connectorError(QStringLiteral("cursor_unavailable"));

        const auto cursorText = arguments.value(QStringLiteral("cursor")).toString();
        qint64 offset = 0;
        if (!cursorText.isEmpty()) {
            const auto parsed = m_editorToolsCursorCodec.parse(
                cursorText, QStringLiteral("connector-editor-tools-list/v1"), snapshotDigest);
            if (!parsed.valid())
                return connectorError(QStringLiteral("invalid_cursor"));
            offset = *parsed.offset;
        }
        if (offset < 0 || offset > tools.size())
            return connectorError(QStringLiteral("invalid_cursor"));
        const auto limit = arguments.value(QStringLiteral("limit")).toInt(100);
        QJsonArray page;
        for (auto index = offset; index < tools.size() && index < offset + limit; ++index)
            page.append(tools.at(index));
        QJsonObject structured{
            {QStringLiteral("toolset_version"),
             jsonInteger(m_manifest, QStringLiteral("toolsetVersion"),
                         QStringLiteral("toolset_version"), 0)},
            {QStringLiteral("tools"), page},
            {QStringLiteral("manifest_digest"),
             jsonString(m_manifest, QStringLiteral("digest"),
                        QStringLiteral("manifest_digest"))},
        };
        if (offset + page.size() < tools.size()) {
            const auto nextCursor = m_editorToolsCursorCodec.issue(
                QStringLiteral("connector-editor-tools-list/v1"), snapshotDigest,
                offset + page.size());
            if (nextCursor.isEmpty())
                return connectorError(QStringLiteral("cursor_unavailable"));
            structured.insert(QStringLiteral("next_cursor"), nextCursor);
        }
        return {AutomationWire::Mcp::makeToolCallResult(structured)};
    }

    ToolCallOutcome ConnectorRuntime::searchActualTools(const QJsonObject &arguments) const {
        const auto query = arguments.value(QStringLiteral("query")).toString();
        const auto categoryFilter = arguments.value(QStringLiteral("category")).toString();
        const auto limit = arguments.value(QStringLiteral("limit")).toInt(50);
        QJsonArray matches;
        for (const auto &entry : filteredActualTools()) {
            const auto tool = entry.toObject();
            const auto category = ExposurePolicy::category(tool);
            if (!categoryFilter.isEmpty() && category != categoryFilter)
                continue;
            const auto haystack = QStringLiteral("%1\n%2\n%3\n%4")
                                      .arg(ExposurePolicy::operationId(tool),
                                           tool.value(QStringLiteral("title")).toString(),
                                           tool.value(QStringLiteral("description")).toString(),
                                           category);
            if (!haystack.contains(query, Qt::CaseInsensitive))
                continue;
            matches.append(tool);
            if (matches.size() >= limit)
                break;
        }
        const QJsonObject structured{
            {QStringLiteral("toolset_version"),
             jsonInteger(m_manifest, QStringLiteral("toolsetVersion"),
                         QStringLiteral("toolset_version"), 0)},
            {QStringLiteral("tools"), matches},
            {QStringLiteral("manifest_digest"),
             jsonString(m_manifest, QStringLiteral("digest"),
                        QStringLiteral("manifest_digest"))},
        };
        return {AutomationWire::Mcp::makeToolCallResult(structured)};
    }

    ToolCallOutcome ConnectorRuntime::describeActualTool(const QJsonObject &arguments) const {
        const auto name = arguments.value(QStringLiteral("name")).toString();
        const auto tool = actualTool(name);
        if (!targetAllowed(tool, name))
            return connectorError(QStringLiteral("connector_tool_filtered"));
        const auto *known = AutomationWire::findPublicTool(name);
        if (tool.isEmpty())
            return connectorError(known ? compatibilityFor(*known)
                                        : QStringLiteral("tool_unavailable"));
        const auto unavailable = actualAvailabilityCode(tool);
        if (!unavailable.isEmpty())
            return connectorError(unavailable);
        auto availability = tool.value(QStringLiteral("availability")).toString();
        if (availability.isEmpty())
            availability = QStringLiteral("available");
        const QJsonObject structured{
            {QStringLiteral("tool"), toolDescriptor(tool)},
            {QStringLiteral("version"),
             jsonInteger(tool, QStringLiteral("version"), QStringLiteral("version"), 0)},
            {QStringLiteral("minimum_compatible_version"),
             jsonInteger(tool, QStringLiteral("minimumCompatibleVersion"),
                         QStringLiteral("minimum_compatible_version"), 1)},
            {QStringLiteral("input_schema"),
             jsonValue(tool, QStringLiteral("inputSchema"), QStringLiteral("input_schema"))},
            {QStringLiteral("output_schema"),
             jsonValue(tool, QStringLiteral("outputSchema"), QStringLiteral("output_schema"))},
            {QStringLiteral("manifest_digest"),
             jsonString(m_manifest, QStringLiteral("digest"),
                        QStringLiteral("manifest_digest"))},
            {QStringLiteral("typed_compatibility"),
             known ? compatibilityFor(*known) : QStringLiteral("generic_only")},
            {QStringLiteral("availability"), availability},
            {QStringLiteral("minimum_profile"), ExposurePolicy::minimumProfile(tool)},
            {QStringLiteral("category"), ExposurePolicy::category(tool)},
        };
        return {AutomationWire::Mcp::makeToolCallResult(structured)};
    }

}
