#include "ConnectorRuntime.h"

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/ProductMetadata.h>

#include <QJsonArray>
#include <QCoreApplication>
#include <QDebug>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace DsConnector {
    namespace {
        constexpr auto MaxHandshakePages = 1024;
        constexpr auto MaxHandshakeItems = 100000;
        constexpr auto MaxHandshakeRetryAttempts = 4;
        constexpr auto InitialHandshakeRetryDelayMs = 100;
        constexpr auto MaximumHandshakeRetryDelayMs = 1600;
        constexpr qint64 MaximumSafeJsonInteger = 9007199254740991LL;

        enum class ToolEffect {
            ReadOnly,
            NonDestructive,
            Destructive,
        };

        enum class ToolRepeatability {
            Idempotent,
            NonIdempotent,
        };

        enum class ToolWorldAccess {
            ClosedWorld,
            OpenWorld,
        };

        struct HeaderBinding {
            QStringList path;
            QByteArray name;
            QString type;
        };

        bool collectHeaderBindings(const QJsonObject &inputSchema, QList<HeaderBinding> &bindings,
                                   QString &error) {
            bindings.clear();
            error.clear();
            QSet<QString> names;
            static const QRegularExpression token(QStringLiteral("^[!#$%&'*+.^_`|~0-9A-Za-z-]+$"));
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
                        (type != QStringLiteral("string") && type != QStringLiteral("integer") &&
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

        QJsonObject emptyObjectSchema() {
            return {
                {QStringLiteral("$schema"),
                 QStringLiteral("https://json-schema.org/draft/2020-12/schema")  },
                {QStringLiteral("type"),                 QStringLiteral("object")},
                {QStringLiteral("properties"),           QJsonObject{}           },
                {QStringLiteral("additionalProperties"), false                   },
            };
        }

        QJsonObject openObjectSchema() {
            return {
                {QStringLiteral("$schema"),
                 QStringLiteral("https://json-schema.org/draft/2020-12/schema")  },
                {QStringLiteral("type"),                 QStringLiteral("object")},
                {QStringLiteral("additionalProperties"), true                    },
            };
        }

        QJsonObject strictObjectSchema(QJsonObject properties, QJsonArray required = {}) {
            QJsonObject result{
                {QStringLiteral("$schema"),
                 QStringLiteral("https://json-schema.org/draft/2020-12/schema")  },
                {QStringLiteral("type"),                 QStringLiteral("object")},
                {QStringLiteral("properties"),           std::move(properties)   },
                {QStringLiteral("additionalProperties"), false                   },
            };
            if (!required.isEmpty())
                result.insert(QStringLiteral("required"), required);
            return result;
        }

        QJsonObject stringSchema() {
            return {
                {QStringLiteral("type"), QStringLiteral("string")}
            };
        }

        QJsonObject enumStringSchema(const QStringList &values) {
            QJsonArray entries;
            for (const auto &value : values)
                entries.append(value);
            return {
                {QStringLiteral("type"), QStringLiteral("string")},
                {QStringLiteral("enum"), entries                 },
            };
        }

        QJsonObject integerSchema(const int minimum = 0) {
            return {
                {QStringLiteral("type"),    QStringLiteral("integer")},
                {QStringLiteral("minimum"), minimum                  },
            };
        }

        QJsonObject stringArraySchema() {
            return {
                {QStringLiteral("type"),  QStringLiteral("array")},
                {QStringLiteral("items"), stringSchema()         },
            };
        }

        QJsonObject toolDescriptorSchema() {
            const QJsonObject boolean{
                {QStringLiteral("type"), QStringLiteral("boolean")},
            };
            auto annotations = strictObjectSchema(QJsonObject{
                {QStringLiteral("title"),           stringSchema()},
                {QStringLiteral("readOnlyHint"),    boolean       },
                {QStringLiteral("destructiveHint"), boolean       },
                {QStringLiteral("idempotentHint"),  boolean       },
                {QStringLiteral("openWorldHint"),   boolean       },
            });
            annotations.insert(QStringLiteral("additionalProperties"), true);
            auto result = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("name"),         stringSchema()            },
                    {QStringLiteral("title"),        stringSchema()            },
                    {QStringLiteral("description"),  stringSchema()            },
                    {QStringLiteral("inputSchema"),  openObjectSchema()        },
                    {QStringLiteral("outputSchema"), openObjectSchema()        },
                    {QStringLiteral("annotations"),  annotations               },
                    {QStringLiteral("icons"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                                 {QStringLiteral("items"), openObjectSchema()}}},
                    {QStringLiteral("_meta"),        openObjectSchema()        },
                    {QStringLiteral("availability"), stringSchema()            },
            },
                QJsonArray{QStringLiteral("name"), QStringLiteral("inputSchema")});
            result.insert(QStringLiteral("additionalProperties"), true);
            return result;
        }

        bool validToolDescriptor(const QJsonValue &value) {
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
            return !tool.contains(QStringLiteral("outputSchema")) ||
                   tool.value(QStringLiteral("outputSchema")).isObject();
        }

        QJsonObject statusSchema() {
            const auto connector = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("instance_id"),     stringSchema()},
                    {QStringLiteral("version"),         stringSchema()},
                    {QStringLiteral("executable_path"), stringSchema()},
                    {QStringLiteral("build_id"),        stringSchema()}
            },
                QJsonArray{QStringLiteral("instance_id"), QStringLiteral("version"),
                           QStringLiteral("executable_path"), QStringLiteral("build_id")});
            const auto editor = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("state"),
                     enumStringSchema(
                         {QStringLiteral("not_running"), QStringLiteral("editor_starting"),
                          QStringLiteral("server_disabled"), QStringLiteral("server_starting"),
                          QStringLiteral("server_ready"), QStringLiteral("server_stopping"),
                          QStringLiteral("editor_stopping"), QStringLiteral("error")})  },
                    {QStringLiteral("editor_instance_id"),  stringSchema()              },
                    {QStringLiteral("process_id"),          integerSchema()             },
                    {QStringLiteral("executable_path"),     stringSchema()              },
                    {QStringLiteral("application_version"), stringSchema()              },
                    {QStringLiteral("build_id"),            stringSchema()              },
                    {QStringLiteral("host_mode"),
                     enumStringSchema(
                         {QString(), QStringLiteral("gui"), QStringLiteral("headless")})},
                    {QStringLiteral("error"),               stringSchema()              },
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
                    {QStringLiteral("protocol_version"),   integerSchema()           },
                    {QStringLiteral("error"),              stringSchema()            },
            },
                QJsonArray{QStringLiteral("connected"), QStringLiteral("protocol_supported"),
                           QStringLiteral("protocol_version"), QStringLiteral("error")});
            const auto mcp = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("connected"),
                     QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")}}             },
                    {QStringLiteral("endpoint"),              stringSchema()                      },
                    {QStringLiteral("protocol_version"),
                     enumStringSchema(
                         {QString(), QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion),
                          QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion),
                          QString::fromLatin1(AutomationWire::Mcp::CompatibilityProtocolVersion)})},
                    {QStringLiteral("error"),                 stringSchema()                      },
                    {QStringLiteral("pending_request_count"), integerSchema()                     },
            },
                QJsonArray{QStringLiteral("connected"), QStringLiteral("endpoint"),
                           QStringLiteral("protocol_version"), QStringLiteral("error"),
                           QStringLiteral("pending_request_count")});
            const auto toolset = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("compatibility"),
                     enumStringSchema({QStringLiteral("not_loaded"), QStringLiteral("refreshing"),
                                       QStringLiteral("status_unavailable"),
                                       QStringLiteral("compatible"),
                                       QStringLiteral("contract_incompatible")})},
                    {QStringLiteral("connector_version"),  integerSchema(1)     },
                    {QStringLiteral("editor_version"),     integerSchema()      },
                    {QStringLiteral("compatible_count"),   integerSchema()      },
                    {QStringLiteral("incompatible_count"), integerSchema()      },
                    {QStringLiteral("unavailable_count"),  integerSchema()      },
            },
                QJsonArray{QStringLiteral("compatibility"), QStringLiteral("connector_version"),
                           QStringLiteral("editor_version"), QStringLiteral("compatible_count"),
                           QStringLiteral("incompatible_count"),
                           QStringLiteral("unavailable_count")});
            const auto exposure = strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("control_level"),
                     enumStringSchema({QStringLiteral("l0"), QStringLiteral("l1"),
                                       QStringLiteral("l2"), QStringLiteral("l3")})},
                    {QStringLiteral("includes"),             stringArraySchema()   },
                    {QStringLiteral("excludes"),             stringArraySchema()   },
                    {QStringLiteral("typed_tool_count"),     integerSchema()       },
                    {QStringLiteral("generic_target_count"), integerSchema()       },
                    {QStringLiteral("pending_selectors"),    stringArraySchema()   },
            },
                QJsonArray{QStringLiteral("control_level"), QStringLiteral("includes"),
                           QStringLiteral("excludes"), QStringLiteral("typed_tool_count"),
                           QStringLiteral("generic_target_count"),
                           QStringLiteral("pending_selectors")});
            return strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("connector"), connector},
                    {QStringLiteral("editor"),    editor   },
                    {QStringLiteral("bootstrap"), bootstrap},
                    {QStringLiteral("mcp"),       mcp      },
                    {QStringLiteral("toolset"),   toolset  },
                    {QStringLiteral("exposure"),  exposure }
            },
                QJsonArray{QStringLiteral("connector"), QStringLiteral("editor"),
                           QStringLiteral("bootstrap"), QStringLiteral("mcp"),
                           QStringLiteral("toolset"), QStringLiteral("exposure")});
        }

        QJsonObject toolSummarySchema() {
            const QJsonObject boolean{
                {QStringLiteral("type"), QStringLiteral("boolean")},
            };
            auto annotations = strictObjectSchema(QJsonObject{
                {QStringLiteral("readOnlyHint"),    boolean},
                {QStringLiteral("destructiveHint"), boolean},
                {QStringLiteral("idempotentHint"),  boolean},
                {QStringLiteral("openWorldHint"),   boolean},
            });
            annotations.insert(QStringLiteral("additionalProperties"), true);
            return strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("name"),                    stringSchema()  },
                    {QStringLiteral("title"),                   stringSchema()  },
                    {QStringLiteral("description"),             stringSchema()  },
                    {QStringLiteral("category"),                stringSchema()  },
                    {QStringLiteral("minimum_control_level"),   stringSchema()  },
                    {QStringLiteral("minimum_toolset_version"), integerSchema(1)},
                    {QStringLiteral("availability"),            stringSchema()  },
                    {QStringLiteral("annotations"),             annotations     },
            },
                QJsonArray{QStringLiteral("name"), QStringLiteral("category"),
                           QStringLiteral("minimum_control_level"),
                           QStringLiteral("minimum_toolset_version"),
                           QStringLiteral("availability")});
        }

        QJsonObject toolCollectionSchema(const bool paged) {
            QJsonObject properties{
                {QStringLiteral("toolset_version"), integerSchema()         },
                {QStringLiteral("tools"),
                 QJsonObject{{QStringLiteral("type"), QStringLiteral("array")},
                             {QStringLiteral("items"), toolSummarySchema()}}},
            };
            if (paged)
                properties.insert(QStringLiteral("next_cursor"), stringSchema());
            return strictObjectSchema(
                properties, QJsonArray{QStringLiteral("toolset_version"), QStringLiteral("tools")});
        }

        QJsonObject describeSchema() {
            return strictObjectSchema(
                QJsonObject{
                    {QStringLiteral("tool"),                toolDescriptorSchema()},
                    {QStringLiteral("toolset_version"),     integerSchema(1)      },
                    {QStringLiteral("typed_compatibility"), stringSchema()        },
                    {QStringLiteral("availability"),        stringSchema()        },
            },
                QJsonArray{QStringLiteral("tool"), QStringLiteral("toolset_version"),
                           QStringLiteral("typed_compatibility"), QStringLiteral("availability")});
        }

        QJsonObject bridgeTool(const QString &name, const QString &description,
                               const QJsonObject &inputSchema, const QJsonObject &outputSchema,
                               const ToolEffect effect, const ToolRepeatability repeatability,
                               const ToolWorldAccess worldAccess) {
            return {
                {QStringLiteral("name"),         name        },
                {QStringLiteral("title"),        name        },
                {QStringLiteral("description"),  description },
                {QStringLiteral("inputSchema"),  inputSchema },
                {QStringLiteral("outputSchema"), outputSchema},
                {QStringLiteral("annotations"),
                 QJsonObject{
                     {QStringLiteral("readOnlyHint"), effect == ToolEffect::ReadOnly},
                     {QStringLiteral("destructiveHint"), effect == ToolEffect::Destructive},
                     {QStringLiteral("idempotentHint"),
                      repeatability == ToolRepeatability::Idempotent},
                     {QStringLiteral("openWorldHint"), worldAccess == ToolWorldAccess::OpenWorld},
                 }                                           },
                {QStringLiteral("_meta"),
                 QJsonObject{
                     {QStringLiteral("org.openvpi.ds-editor-lite/tool"),
                      QJsonObject{
                          {QStringLiteral("minimum_toolset_version"), 1},
                          {QStringLiteral("category"), QStringLiteral("connector")},
                      }},
                 }                                           },
            };
        }

        QString jsonString(const QJsonObject &object, const QString &camel,
                           const QString &snake = {}) {
            auto value = object.value(camel).toString();
            if (value.isEmpty() && !snake.isEmpty())
                value = object.value(snake).toString();
            return value;
        }

        qint64 jsonInteger(const QJsonObject &object, const QString &camel, const QString &snake,
                           const qint64 fallback) {
            const auto value = object.contains(camel) ? object.value(camel) : object.value(snake);
            return value.isDouble() ? value.toInteger(fallback) : fallback;
        }

        QJsonValue jsonValue(const QJsonObject &object, const QString &camel,
                             const QString &snake) {
            return object.contains(camel) ? object.value(camel) : object.value(snake);
        }

        QJsonObject namespacedToolMetadata(const QJsonObject &tool) {
            return tool.value(QStringLiteral("_meta"))
                .toObject()
                .value(QStringLiteral("org.openvpi.ds-editor-lite/tool"))
                .toObject();
        }

        QJsonValue toolMetadataValue(const QJsonObject &tool, const QString &camel,
                                     const QString &snake) {
            auto value = jsonValue(tool, camel, snake);
            if (!value.isUndefined())
                return value;
            const auto metadata = namespacedToolMetadata(tool);
            value = metadata.value(snake);
            return value.isUndefined() ? metadata.value(camel) : value;
        }

        qint64 toolMetadataInteger(const QJsonObject &tool, const QString &camel,
                                   const QString &snake, const qint64 fallback) {
            const auto value = toolMetadataValue(tool, camel, snake);
            return value.isDouble() ? value.toInteger(fallback) : fallback;
        }

        QJsonObject toolDescriptor(const QJsonObject &tool) {
            QJsonObject result{
                {QStringLiteral("name"), ExposurePolicy::operationId(tool)},
                {QStringLiteral("inputSchema"),
                 jsonValue(tool, QStringLiteral("inputSchema"), QStringLiteral("input_schema"))},
            };
            for (const auto &key : {QStringLiteral("title"), QStringLiteral("description")}) {
                const auto value = tool.value(key);
                if (value.isString())
                    result.insert(key, value);
            }
            const auto outputSchema =
                jsonValue(tool, QStringLiteral("outputSchema"), QStringLiteral("output_schema"));
            if (outputSchema.isObject())
                result.insert(QStringLiteral("outputSchema"), outputSchema);
            if (tool.contains(QStringLiteral("annotations")))
                result.insert(QStringLiteral("annotations"),
                              tool.value(QStringLiteral("annotations")));
            if (tool.contains(QStringLiteral("icons")))
                result.insert(QStringLiteral("icons"), tool.value(QStringLiteral("icons")));

            auto meta = tool.value(QStringLiteral("_meta")).toObject();
            auto metadata = namespacedToolMetadata(tool);
            const auto synthesize = [&tool, &metadata](const QString &target, const QString &camel,
                                                       const QString &snake) {
                const auto value = jsonValue(tool, camel, snake);
                if (!value.isUndefined())
                    metadata.insert(target, value);
            };
            synthesize(QStringLiteral("minimum_toolset_version"),
                       QStringLiteral("minimumToolsetVersion"),
                       QStringLiteral("minimum_toolset_version"));
            synthesize(QStringLiteral("category"), QStringLiteral("category"),
                       QStringLiteral("category"));
            synthesize(QStringLiteral("minimum_control_level"),
                       QStringLiteral("minimumControlLevel"),
                       QStringLiteral("minimum_control_level"));
            synthesize(QStringLiteral("kind"), QStringLiteral("kind"), QStringLiteral("kind"));
            synthesize(QStringLiteral("sync_mode"), QStringLiteral("syncMode"),
                       QStringLiteral("sync_mode"));
            synthesize(QStringLiteral("host_availability"), QStringLiteral("hostAvailability"),
                       QStringLiteral("host_availability"));
            synthesize(QStringLiteral("value_sources"), QStringLiteral("valueSources"),
                       QStringLiteral("value_sources"));
            if (!metadata.isEmpty()) {
                meta.insert(QStringLiteral("org.openvpi.ds-editor-lite/tool"), metadata);
                result.insert(QStringLiteral("_meta"), meta);
            } else if (tool.value(QStringLiteral("_meta")).isObject()) {
                result.insert(QStringLiteral("_meta"), meta);
            }
            const auto availability = tool.value(QStringLiteral("availability"));
            if (availability.isString())
                result.insert(QStringLiteral("availability"), availability);
            return result;
        }

        QJsonObject toolSummary(const QJsonObject &tool) {
            QJsonObject result{
                {QStringLiteral("name"), ExposurePolicy::operationId(tool)},
                {QStringLiteral("category"),
                 toolMetadataValue(tool, QStringLiteral("category"), QStringLiteral("category"))
                     .toString(QStringLiteral("editor"))},
                {QStringLiteral("minimum_control_level"),
                 toolMetadataValue(tool, QStringLiteral("minimumControlLevel"),
                 QStringLiteral("minimum_control_level"))
                     .toString(QStringLiteral("l3"))},
                {QStringLiteral("minimum_toolset_version"),
                 toolMetadataInteger(tool, QStringLiteral("minimumToolsetVersion"),
                 QStringLiteral("minimum_toolset_version"), 1)},
                {QStringLiteral("availability"),
                 tool.value(QStringLiteral("availability")).toString(QStringLiteral("available"))},
            };
            for (const auto &key : {QStringLiteral("title"), QStringLiteral("description"),
                                    QStringLiteral("annotations")}) {
                if (tool.contains(key))
                    result.insert(key, tool.value(key));
            }
            return result;
        }

        QJsonObject callArguments(const QString &name, const QJsonObject &arguments) {
            return {
                {QStringLiteral("name"),      name     },
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
                   error == QStringLiteral("busy") || error == QStringLiteral("mcp_stopping") ||
                   error == QStringLiteral("request_timeout") ||
                   error == QStringLiteral("upstream_timeout") ||
                   error == QStringLiteral("upstream_transport_error");
        }

        bool shouldTryLegacyHandshake(const UpstreamResult &result) {
            if (result.protocolError) {
                switch (result.protocolError->code) {
                    case AutomationWire::Mcp::MethodNotFound:
                    case AutomationWire::Mcp::ServerNotInitialized:
                    case AutomationWire::Mcp::HeaderMismatch:
                    case AutomationWire::Mcp::MissingRequiredClientCapability:
                    case AutomationWire::Mcp::UnsupportedProtocolVersion:
                        return true;
                    default:
                        return false;
                }
            }
            return result.connectorError != QStringLiteral("too_many_requests") &&
                   result.connectorError != QStringLiteral("busy") &&
                   result.connectorError != QStringLiteral("mcp_stopping") &&
                   result.connectorError != QStringLiteral("request_timeout");
        }
    }

    ConnectorRuntime::ConnectorRuntime(ConnectorOptions options, QString bootstrapServiceName,
                                       QObject *parent)
        : QObject(parent), m_options(std::move(options)), m_exposure(m_options),
          m_instanceId(QUuid::createUuid().toString(QUuid::WithoutBraces)),
          m_version(QString::fromLatin1(LiteProductMetadata::Version)),
          m_bootstrap(
              new BootstrapWatcher(m_instanceId, m_version, std::move(bootstrapServiceName), this)),
          m_upstream(new UpstreamMcpClient(m_instanceId, m_version, this)),
          m_handshakeRetryTimer(new QTimer(this)) {
        clearToolCaches();
        m_unavailableCount = m_exposure.typedContracts().size();
        m_handshakeRetryTimer->setSingleShot(true);
        connect(m_bootstrap, &BootstrapWatcher::observationChanged, this,
                &ConnectorRuntime::bootstrapChanged);
        connect(m_upstream, &UpstreamMcpClient::sessionExpired, this, &ConnectorRuntime::reconnect);
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
        const auto editorStatus =
            hasSnapshot ? observation.snapshot->result : SingleInstanceAutomationStatus{};
        const auto editorState =
            hasSnapshot ? SingleInstanceProtocol::automationStateName(editorStatus.state)
                        : QStringLiteral("not_running");

        const auto editorVersion = jsonInteger(m_editorContract, QStringLiteral("toolsetVersion"),
                                               QStringLiteral("toolset_version"), 0);

        QJsonArray includes;
        QJsonArray excludes;
        for (const auto &selector : m_options.exposure.includes)
            includes.append(selector);
        for (const auto &selector : m_options.exposure.excludes)
            excludes.append(selector);
        QJsonArray pending;
        for (const auto &selector : m_pendingSelectorsCache)
            pending.append(selector);
        return {
            {QStringLiteral("connector"),
             QJsonObject{
                 {QStringLiteral("instance_id"), m_instanceId},
                 {QStringLiteral("version"), m_version},
                 {QStringLiteral("executable_path"), QCoreApplication::applicationFilePath()},
                 {QStringLiteral("build_id"), connectorBuildId()}}                             },
            {QStringLiteral("editor"),
             QJsonObject{{QStringLiteral("state"), editorState},
                         {QStringLiteral("editor_instance_id"), editorStatus.editorInstanceId},
                         {QStringLiteral("process_id"),
                          hasSnapshot ? observation.snapshot->primaryProcessId : 0},
                         {QStringLiteral("executable_path"), editorStatus.executablePath},
                         {QStringLiteral("application_version"), editorStatus.applicationVersion},
                         {QStringLiteral("build_id"), editorStatus.buildId},
                         {QStringLiteral("host_mode"), editorStatus.hostMode},
                         {QStringLiteral("error"), editorStatus.error}}                        },
            {QStringLiteral("bootstrap"),
             QJsonObject{
                 {QStringLiteral("connected"), observation.connected},
                 {QStringLiteral("protocol_supported"), observation.protocolSupported},
                 {QStringLiteral("protocol_version"), SingleInstanceProtocol::protocolVersion},
                 {QStringLiteral("error"), observation.error}}                                 },
            {QStringLiteral("mcp"),
             QJsonObject{{QStringLiteral("connected"), m_mcpConnected},
                         {QStringLiteral("endpoint"), m_upstream->endpoint().toString()},
                         {QStringLiteral("protocol_version"),
                          m_mcpConnected ? m_mcpProtocolVersion : QString()},
                         {QStringLiteral("error"), m_mcpError},
                         {QStringLiteral("pending_request_count"), m_upstream->pendingCount()}}},
            {QStringLiteral("toolset"),
             QJsonObject{{QStringLiteral("compatibility"), m_toolsetCompatibility},
                         {QStringLiteral("connector_version"),
                          static_cast<qint64>(AutomationWire::PublicToolsetVersion)},
                         {QStringLiteral("editor_version"), editorVersion},
                         {QStringLiteral("compatible_count"), m_compatibleCount},
                         {QStringLiteral("incompatible_count"), m_incompatibleCount},
                         {QStringLiteral("unavailable_count"), m_unavailableCount}}            },
            {QStringLiteral("exposure"),
             QJsonObject{
                 {QStringLiteral("control_level"),
                  AutomationWire::exposureLevelName(m_options.exposure.controlLevel)},
                 {QStringLiteral("includes"), includes},
                 {QStringLiteral("excludes"), excludes},
                 {QStringLiteral("typed_tool_count"), m_exposure.typedContracts().size()},
                 {QStringLiteral("generic_target_count"), m_filteredActualToolsCache.size()},
                 {QStringLiteral("pending_selectors"), pending}}                               },
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
            return forwardEditorTool(targetName,
                                     arguments.value(QStringLiteral("arguments")).toObject(),
                                     std::move(callback));
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
        if (m_editorContract.isEmpty()) {
            callback(connectorError(QStringLiteral("status_unavailable")));
            return 0;
        }
        const auto compatibility = compatibilityFor(*known);
        if (compatibility != QStringLiteral("compatible")) {
            callback(connectorError(compatibility));
            return 0;
        }
        return forwardEditorTool(name, arguments, std::move(callback));
    }

    bool ConnectorRuntime::cancel(const qint64 requestToken, const QString &reason) {
        return m_upstream->cancel(requestToken, reason);
    }

    const QStringList &ConnectorRuntime::bridgeToolNames() {
        static const QStringList names{
            QStringLiteral("connector.get_status"),  QStringLiteral("connector.reconnect"),
            QStringLiteral("editor.tools.list"),     QStringLiteral("editor.tools.search"),
            QStringLiteral("editor.tools.describe"), QStringLiteral("editor.tools.invoke"),
        };
        return names;
    }

    QJsonArray ConnectorRuntime::bridgeToolDefinitions() {
        static const QJsonArray definitions{
            bridgeTool(QStringLiteral("connector.get_status"),
                       QStringLiteral("Return connector, editor, bootstrap, MCP, toolset, and "
                                      "exposure facts."),
                       emptyObjectSchema(), statusSchema(), ToolEffect::ReadOnly,
                       ToolRepeatability::Idempotent, ToolWorldAccess::ClosedWorld),
            bridgeTool(QStringLiteral("connector.reconnect"),
                       QStringLiteral("Restart bootstrap discovery and the editor MCP handshake."),
                       emptyObjectSchema(), statusSchema(), ToolEffect::NonDestructive,
                       ToolRepeatability::NonIdempotent, ToolWorldAccess::ClosedWorld),
            bridgeTool(QStringLiteral("editor.tools.list"),
                       QStringLiteral("List current editor targets allowed by connector exposure."),
                       strictObjectSchema(QJsonObject{
                                                      {QStringLiteral("cursor"),
                            QJsonObject{{QStringLiteral("type"), QStringLiteral("string")}}},
                                                      {QStringLiteral("limit"),
                            QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                                        {QStringLiteral("minimum"), 1},
                                        {QStringLiteral("maximum"), 200}}},
                                                      }
                       ),
                       toolCollectionSchema(true), ToolEffect::ReadOnly,
                       ToolRepeatability::Idempotent, ToolWorldAccess::ClosedWorld),
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
                    QJsonArray{QStringLiteral("query")}
                       ),
                toolCollectionSchema(false), ToolEffect::ReadOnly, ToolRepeatability::Idempotent,
                ToolWorldAccess::ClosedWorld),
            bridgeTool(
                QStringLiteral("editor.tools.describe"),
                QStringLiteral("Describe one current editor target and its actual schemas."),
                strictObjectSchema(
                    QJsonObject{{QStringLiteral("name"),
                                 QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                             {QStringLiteral("minLength"), 1}}}},
                    QJsonArray{QStringLiteral("name")}
                       ),
                describeSchema(), ToolEffect::ReadOnly, ToolRepeatability::Idempotent,
                ToolWorldAccess::ClosedWorld),
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
                    QJsonArray{QStringLiteral("name"), QStringLiteral("arguments")}
                       ),
                QJsonObject{},
                       ToolEffect::Destructive, ToolRepeatability::NonIdempotent,
                ToolWorldAccess::ClosedWorld),
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
            clearEditorState(observation.error.isEmpty() ? QStringLiteral("bootstrap_disconnected")
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
        if (snapshot.result.state == SingleInstanceAutomationState::ServerReady) {
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
        m_handshakeRefreshPending = false;
        m_handshakeRetryAttempt = 0;
        m_upstream->clearEndpoint(error);
        m_actualTools = {};
        m_editorContract = {};
        clearToolCaches();
        m_toolsetCompatibility = QStringLiteral("not_loaded");
        m_compatibleCount = 0;
        m_incompatibleCount = 0;
        m_unavailableCount = m_exposure.typedContracts().size();
        m_mcpConnected = false;
        m_mcpProtocolVersion.clear();
        m_mcpError = error;
        m_upstream->setProtocolVersion(QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion));
    }

    void ConnectorRuntime::beginHandshake(const SingleInstanceAutomationSnapshot &snapshot) {
        const auto sameTarget =
            m_handshakeTarget && m_handshakeTarget->primaryProcessId == snapshot.primaryProcessId &&
            m_handshakeTarget->result.editorInstanceId == snapshot.result.editorInstanceId &&
            m_handshakeTarget->result.serverEndpoint == snapshot.result.serverEndpoint;
        if (!sameTarget) {
            ++m_handshakeEpoch;
            m_handshakeRetryTimer->stop();
            m_handshakeInProgress = false;
            m_handshakeRefreshPending = false;
            m_handshakeRetryAttempt = 0;
            m_upstream->clearEndpoint(QStringLiteral("editor_endpoint_changed"));
        }
        m_handshakeTarget = snapshot;
        if (m_handshakeInProgress) {
            m_handshakeRefreshPending = true;
            return;
        }
        m_handshakeRetryAttempt = 0;
        m_handshakeInProgress = true;
        startHandshakeAttempt();
    }

    void ConnectorRuntime::startHandshakeAttempt() {
        if (!m_handshakeInProgress || !m_handshakeTarget)
            return;
        const auto epoch = ++m_handshakeEpoch;
        m_mcpConnected = false;
        m_mcpError.clear();
        m_actualTools = {};
        m_editorContract = {};
        clearToolCaches();
        m_toolsetCompatibility = QStringLiteral("refreshing");
        m_compatibleCount = 0;
        m_incompatibleCount = 0;
        m_unavailableCount = m_exposure.typedContracts().size();
        m_mcpProtocolVersion.clear();
        m_upstream->clearEndpoint(QStringLiteral("refreshing"));
        QString endpointError;
        if (!m_upstream->setEndpoint(m_handshakeTarget->result.serverEndpoint, &endpointError)) {
            failHandshake(epoch,
                          QStringLiteral("invalid_discovered_endpoint: %1").arg(endpointError));
            return;
        }
        emit statusChanged();
        startModernHandshake(epoch);
    }

    void ConnectorRuntime::startModernHandshake(const quint64 epoch) {
        if (epoch != m_handshakeEpoch || !m_upstream->setProtocolVersion(QString::fromLatin1(
                                             AutomationWire::Mcp::ProtocolVersion))) {
            return;
        }
        m_upstream->send(
            QString::fromLatin1(AutomationWire::Mcp::DiscoverMethod), {},
            [this, epoch](const UpstreamResult result) {
                if (epoch != m_handshakeEpoch)
                    return;
                if (!result.succeeded()) {
                    if (shouldTryLegacyHandshake(result)) {
                        startLegacyHandshake(epoch);
                        return;
                    }
                    failHandshake(
                        epoch, handshakeError(result, QStringLiteral("upstream_discovery_failed")));
                    return;
                }
                const auto supportedVersions =
                    result.result.value(QStringLiteral("supportedVersions"));
                const auto capabilities = result.result.value(QStringLiteral("capabilities"));
                bool protocolSupported = supportedVersions.isArray();
                for (const auto &version : supportedVersions.toArray()) {
                    protocolSupported &= version.isString();
                }
                protocolSupported &= supportedVersions.toArray().contains(
                    QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion));
                if (!protocolSupported || !capabilities.isObject() ||
                    !capabilities.toObject().value(QStringLiteral("tools")).isObject()) {
                    failHandshake(epoch, QStringLiteral("invalid_upstream_discovery"));
                    return;
                }
                m_mcpProtocolVersion = QString::fromLatin1(AutomationWire::Mcp::ProtocolVersion);
                m_mcpConnected = true;
                requestToolsPage(epoch, {}, {}, {}, 0);
            },
            m_options.upstreamTimeoutMs);
    }

    void ConnectorRuntime::startLegacyHandshake(const quint64 epoch) {
        if (epoch != m_handshakeEpoch || !m_upstream->setProtocolVersion(QString::fromLatin1(
                                             AutomationWire::Mcp::LegacyProtocolVersion))) {
            return;
        }
        const QJsonObject params{
            {QStringLiteral("protocolVersion"),
             QString::fromLatin1(AutomationWire::Mcp::LegacyProtocolVersion)},
            {QStringLiteral("capabilities"),    QJsonObject{}               },
            {QStringLiteral("clientInfo"),
             AutomationWire::Mcp::ImplementationInfo{
                 .name = QString::fromLatin1(LiteProductMetadata::ConnectorProductName),
                 .version = m_version,
                 .description =
                     QStringLiteral("Local %1 MCP stdio connector (%2)")
                         .arg(QString::fromLatin1(LiteProductMetadata::ProductName), m_instanceId),
             }
                 .toJson()                                                  },
        };
        m_upstream->send(
            QString::fromLatin1(AutomationWire::Mcp::InitializeMethod), params,
            [this, epoch](const UpstreamResult result) {
                if (epoch != m_handshakeEpoch)
                    return;
                if (!result.succeeded()) {
                    failHandshake(epoch, handshakeError(
                                             result, QStringLiteral("upstream_initialize_failed")));
                    return;
                }
                const auto serverInfo = AutomationWire::Mcp::ImplementationInfo::fromJson(
                    result.result.value(QStringLiteral("serverInfo")));
                const auto capabilities = result.result.value(QStringLiteral("capabilities"));
                const auto negotiatedVersion =
                    result.result.value(QStringLiteral("protocolVersion")).toString();
                if (!AutomationWire::Mcp::isLegacyProtocolVersion(negotiatedVersion) ||
                    !m_upstream->adoptNegotiatedProtocolVersion(negotiatedVersion) ||
                    !capabilities.isObject() ||
                    !capabilities.toObject().value(QStringLiteral("tools")).isObject() ||
                    !serverInfo) {
                    failHandshake(epoch, QStringLiteral("invalid_upstream_initialize"));
                    return;
                }
                m_upstream->sendNotification(
                    QString::fromLatin1(AutomationWire::Mcp::InitializedNotification), {},
                    [this, epoch, negotiatedVersion](const UpstreamResult initialized) {
                        if (epoch != m_handshakeEpoch)
                            return;
                        if (!initialized.succeeded()) {
                            failHandshake(
                                epoch,
                                handshakeError(
                                    initialized,
                                    QStringLiteral("upstream_initialized_notification_failed")));
                            return;
                        }
                        m_mcpProtocolVersion = negotiatedVersion;
                        m_mcpConnected = true;
                        requestToolsPage(epoch, {}, {}, {}, 0);
                    },
                    m_options.upstreamTimeoutMs);
            },
            m_options.upstreamTimeoutMs);
    }

    void ConnectorRuntime::failHandshake(const quint64 epoch, const QString &error,
                                         const QString &toolsetCompatibility,
                                         const bool preserveMcpConnection) {
        if (epoch != m_handshakeEpoch)
            return;
        if (!preserveMcpConnection)
            m_mcpConnected = false;
        m_mcpError = error;
        m_toolsetCompatibility = toolsetCompatibility;
        emit statusChanged();
        if (retryableHandshakeError(error) && m_handshakeRetryAttempt < MaxHandshakeRetryAttempts) {
            const auto delay =
                std::min(MaximumHandshakeRetryDelayMs,
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
        const auto refresh = m_handshakeRefreshPending;
        m_handshakeRefreshPending = false;
        if (!refresh || !m_handshakeTarget)
            return;
        m_handshakeInProgress = true;
        startHandshakeAttempt();
    }

    void ConnectorRuntime::requestToolsPage(const quint64 epoch, const QString &cursor,
                                            QJsonArray accumulated, QSet<QString> seenCursors,
                                            const int pageCount) {
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
             seenCursors = std::move(seenCursors),
             pageCount](const UpstreamResult listResult) mutable {
                if (epoch != m_handshakeEpoch)
                    return;
                if (!listResult.succeeded()) {
                    failHandshake(
                        epoch,
                        handshakeError(listResult, QStringLiteral("upstream_tools_list_failed")),
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
                    const auto descriptorValid = validToolDescriptor(tool);
                    if (!descriptorValid || name.isEmpty() || names.contains(name)) {
                        qWarning().noquote()
                            << "Rejected upstream tool descriptor:" << name
                            << "valid=" << descriptorValid << "duplicate=" << names.contains(name);
                        failHandshake(epoch, QStringLiteral("invalid_upstream_tool_descriptor"),
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
                            << QStringLiteral("%1: rejected upstream tool %2: %3")
                                   .arg(QString::fromLatin1(
                                            LiteProductMetadata::ConnectorExecutableBasename),
                                        name, headerError);
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
                    requestToolsPage(epoch, next, std::move(accumulated), std::move(seenCursors),
                                     pageCount + 1);
                    return;
                }
                m_actualTools = std::move(accumulated);
                requestStatus(epoch);
            },
            m_options.upstreamTimeoutMs);
    }

    void ConnectorRuntime::requestStatus(const quint64 epoch) {
        if (epoch != m_handshakeEpoch)
            return;
        const auto statusEntry = std::find_if(
            m_actualTools.constBegin(), m_actualTools.constEnd(), [](const QJsonValue &entry) {
                return ExposurePolicy::operationId(entry.toObject()) ==
                       QStringLiteral("application.get_status");
            });
        if (statusEntry == m_actualTools.constEnd()) {
            finishHandshake(epoch,
                            UpstreamResult{.connectorError = QStringLiteral("status_unavailable")});
            return;
        }
        QHash<QByteArray, QByteArray> parameterHeaderValues;
        QString parameterHeaderError;
        const auto statusSchema =
            jsonValue((*statusEntry).toObject(), QStringLiteral("inputSchema"),
                      QStringLiteral("input_schema"));
        if (!statusSchema.isObject() ||
            !parameterHeaders(statusSchema.toObject(), {}, parameterHeaderValues,
                              parameterHeaderError)) {
            finishHandshake(epoch, UpstreamResult{.connectorError = QStringLiteral(
                                                      "invalid_tool_header_mapping")});
            return;
        }
        m_upstream->send(
            QString::fromLatin1(AutomationWire::Mcp::ToolsCallMethod),
            callArguments(QStringLiteral("application.get_status"), {}),
            [this, epoch](const UpstreamResult result) { finishHandshake(epoch, result); },
            m_options.upstreamTimeoutMs, parameterHeaderValues);
    }

    void ConnectorRuntime::finishHandshake(const quint64 epoch,
                                           const UpstreamResult &statusResult) {
        if (epoch != m_handshakeEpoch)
            return;
        const auto status = structuredContent(statusResult);
        const auto version = jsonInteger(status, QStringLiteral("toolsetVersion"),
                                         QStringLiteral("toolset_version"), 0);
        const auto controlLevel = status.value(QStringLiteral("control_level")).toString();
        const auto hostMode =
            jsonString(status, QStringLiteral("hostMode"), QStringLiteral("host_mode"));
        const auto editorInstanceId =
            QUuid::fromString(status.value(QStringLiteral("editor_instance_id")).toString());
        const auto rootValid = status.value(QStringLiteral("editor_instance_id")).isString() &&
                               !editorInstanceId.isNull() &&
                               status.value(QStringLiteral("host_mode")).isString() &&
                               status.value(QStringLiteral("control_level")).isString() &&
                               status.value(QStringLiteral("toolset_version")).isDouble() &&
                               status.value(QStringLiteral("documents")).isArray() &&
                               status.value(QStringLiteral("windows")).isArray();
        const auto editorControlLevel = AutomationWire::controlLevelFromName(controlLevel);
        const auto statusValid =
            statusResult.succeeded() &&
            !statusResult.result.value(QStringLiteral("isError")).toBool() && rootValid &&
            version >= 1 && version <= MaximumSafeJsonInteger && editorControlLevel.has_value() &&
            (hostMode == QStringLiteral("gui") || hostMode == QStringLiteral("headless"));
        if (!statusValid) {
            m_editorContract = {};
            rebuildToolCaches();
            const auto error =
                statusResult.succeeded() &&
                        !statusResult.result.value(QStringLiteral("isError")).toBool()
                    ? QStringLiteral("invalid_upstream_status_root")
                    : handshakeError(statusResult, QStringLiteral("status_unavailable"));
            failHandshake(epoch, error, QStringLiteral("status_unavailable"), true);
            return;
        }
        m_editorContract = {
            {QStringLiteral("toolset_version"), version     },
            {QStringLiteral("control_level"),   controlLevel},
            {QStringLiteral("host_mode"),       hostMode    },
        };
        rebuildToolCaches();

        m_compatibleCount = 0;
        m_incompatibleCount = 0;
        m_unavailableCount = 0;
        for (const auto &tool : m_exposure.typedContracts()) {
            const auto compatibility = compatibilityFor(tool);
            if (compatibility == QStringLiteral("compatible"))
                ++m_compatibleCount;
            else if (compatibility == QStringLiteral("contract_incompatible"))
                ++m_incompatibleCount;
            else
                ++m_unavailableCount;
        }
        m_toolsetCompatibility = m_incompatibleCount == 0 ? QStringLiteral("compatible")
                                                          : QStringLiteral("contract_incompatible");
        m_mcpError.clear();
        emit statusChanged();
        completeHandshakeCycle(epoch, true);
    }

    QString ConnectorRuntime::editorUnavailableCode() const {
        const auto &observation = m_bootstrap->observation();
        if (!observation.connected || !observation.snapshot)
            return QStringLiteral("editor_not_running");
        switch (observation.snapshot->result.state) {
            case SingleInstanceAutomationState::EditorStarting:
                return QStringLiteral("editor_starting");
            case SingleInstanceAutomationState::ServerDisabled:
                return QStringLiteral("server_disabled");
            case SingleInstanceAutomationState::ServerStarting:
                return QStringLiteral("server_starting");
            case SingleInstanceAutomationState::ServerStopping:
                return QStringLiteral("server_stopping");
            case SingleInstanceAutomationState::EditorStopping:
                return QStringLiteral("editor_not_connected");
            case SingleInstanceAutomationState::Error:
                return QStringLiteral("editor_error");
            case SingleInstanceAutomationState::ServerReady:
                return m_mcpConnected ? QString() : QStringLiteral("editor_not_connected");
        }
        return QStringLiteral("editor_not_connected");
    }

    QString ConnectorRuntime::actualAvailabilityCode(const QJsonObject &tool) const {
        const auto hostAvailability = toolMetadataValue(tool, QStringLiteral("hostAvailability"),
                                                        QStringLiteral("host_availability"))
                                          .toString();
        const auto &observation = m_bootstrap->observation();
        if (!hostAvailability.isEmpty() && hostAvailability != QStringLiteral("both") &&
            observation.snapshot && hostAvailability != observation.snapshot->result.hostMode) {
            return QStringLiteral("host_capability_unavailable");
        }
        const auto availability = tool.value(QStringLiteral("availability")).toString();
        if (!availability.isEmpty() && availability != QStringLiteral("available") &&
            availability != QStringLiteral("compatible")) {
            return availability;
        }
        return {};
    }

    void ConnectorRuntime::clearToolCaches() {
        m_actualToolIndex.clear();
        m_filteredActualToolsCache = {};
        m_pendingSelectorsCache.clear();
        m_filteredActualToolsSnapshot = QString::number(++m_toolCatalogGeneration);
    }

    void ConnectorRuntime::rebuildToolCaches() {
        clearToolCaches();
        for (const auto &entry : m_actualTools) {
            if (!entry.isObject())
                continue;
            const auto tool = entry.toObject();
            const auto id = ExposurePolicy::operationId(tool);
            if (!id.isEmpty())
                m_actualToolIndex.insert(id, tool);
        }
        m_pendingSelectorsCache = m_exposure.pendingSelectors(m_actualTools);

        QJsonArray available;
        for (const auto &entry : std::as_const(m_actualTools)) {
            const auto tool = entry.toObject();
            if (actualAvailabilityCode(tool).isEmpty())
                available.append(tool);
        }
        const auto allowed = m_exposure.filterActualTools(available);
        for (const auto &entry : allowed)
            m_filteredActualToolsCache.append(entry);
    }

    const QJsonArray &ConnectorRuntime::filteredActualTools() const {
        return m_filteredActualToolsCache;
    }

    QJsonObject ConnectorRuntime::actualTool(const QString &name) const {
        return m_actualToolIndex.value(name);
    }

    QString ConnectorRuntime::compatibilityFor(const AutomationWire::ToolContract &tool) const {
        const auto editorTool = actualTool(tool.operationId);
        if (editorTool.isEmpty()) {
            const auto unavailable = editorUnavailableCode();
            if (!unavailable.isEmpty())
                return unavailable;
            const auto &observation = m_bootstrap->observation();
            if (observation.snapshot && !AutomationWire::isToolAvailableOnHost(
                                            tool, observation.snapshot->result.hostMode)) {
                return QStringLiteral("host_capability_unavailable");
            }
            const auto controlLevel = AutomationWire::controlLevelFromName(
                m_editorContract.value(QStringLiteral("control_level")).toString());
            if (controlLevel &&
                (*controlLevel == AutomationWire::ControlLevel::Custom ||
                 !AutomationWire::presetIncludes(*controlLevel, tool.minimumControlLevel))) {
                return QStringLiteral("control_level_blocked");
            }
            return QStringLiteral("tool_unavailable");
        }
        const auto &observation = m_bootstrap->observation();
        if (observation.snapshot &&
            !AutomationWire::isToolAvailableOnHost(tool, observation.snapshot->result.hostMode)) {
            return QStringLiteral("host_capability_unavailable");
        }
        const auto availability = editorTool.value(QStringLiteral("availability")).toString();
        if (!availability.isEmpty() && availability != QStringLiteral("available") &&
            availability != QStringLiteral("compatible"))
            return availability;
        const auto hostAvailability =
            toolMetadataValue(editorTool, QStringLiteral("hostAvailability"),
                              QStringLiteral("host_availability"))
                .toString();
        if (!hostAvailability.isEmpty() && hostAvailability != QStringLiteral("both") &&
            observation.snapshot && hostAvailability != observation.snapshot->result.hostMode) {
            return QStringLiteral("host_capability_unavailable");
        }

        const auto editorToolsetVersion = jsonInteger(
            m_editorContract, QStringLiteral("toolsetVersion"), QStringLiteral("toolset_version"),
            static_cast<qint64>(AutomationWire::PublicToolsetVersion));
        const auto editorMinimum =
            toolMetadataInteger(editorTool, QStringLiteral("minimumToolsetVersion"),
                                QStringLiteral("minimum_toolset_version"), 1);
        const auto versionCompatible =
            static_cast<qint64>(AutomationWire::PublicToolsetVersion) >= editorMinimum &&
            editorToolsetVersion >= static_cast<qint64>(tool.minimumToolsetVersion);
        return versionCompatible ? QStringLiteral("compatible")
                                 : QStringLiteral("contract_incompatible");
    }

    bool ConnectorRuntime::targetAllowed(const QJsonObject &tool, const QString &name) const {
        if (!tool.isEmpty()) {
            return m_exposure.allowsTarget(name, ExposurePolicy::category(tool),
                                           ExposurePolicy::minimumControlLevel(tool));
        }
        const auto *known = AutomationWire::findPublicTool(name);
        return known && m_exposure.allowsKnownTool(*known);
    }

    ToolCallOutcome ConnectorRuntime::connectorError(const QString &code,
                                                     const QString &message) const {
        QJsonObject structured{
            {QStringLiteral("code"), code}
        };
        structured.insert(QStringLiteral("message"), message.isEmpty() ? code : message);
        return {AutomationWire::Mcp::makeToolCallResult(structured, true)};
    }

    qint64 ConnectorRuntime::forwardEditorTool(const QString &name, const QJsonObject &arguments,
                                               ToolCallCallback callback) {
        const auto unavailable = editorUnavailableCode();
        if (!unavailable.isEmpty()) {
            callback(connectorError(unavailable));
            return 0;
        }
        const auto *known = AutomationWire::findPublicTool(name);
        const auto editorTool = actualTool(name);
        const auto command =
            known ? known->kind == AutomationWire::OperationKind::Command
                  : toolMetadataValue(editorTool, QStringLiteral("kind"), QStringLiteral("kind"))
                            .toString() == QStringLiteral("command");
        QHash<QByteArray, QByteArray> parameterHeaderValues;
        QString parameterHeaderError;
        const auto inputSchema =
            jsonValue(editorTool, QStringLiteral("inputSchema"), QStringLiteral("input_schema"));
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
                    const auto message = command && result.outcomeUnknown ? result.connectorError
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
        const auto &tools = filteredActualTools();
        const auto &snapshot = m_filteredActualToolsSnapshot;
        if (snapshot.isEmpty())
            return connectorError(QStringLiteral("cursor_unavailable"));

        const auto cursorText = arguments.value(QStringLiteral("cursor")).toString();
        qint64 offset = 0;
        if (!cursorText.isEmpty()) {
            const auto parsed = m_editorToolsCursorCodec.parse(
                cursorText, QStringLiteral("connector-editor-tools-list/v1"), snapshot);
            if (!parsed.valid())
                return connectorError(QStringLiteral("invalid_cursor"));
            offset = *parsed.offset;
        }
        if (offset < 0 || offset > tools.size())
            return connectorError(QStringLiteral("invalid_cursor"));
        const auto limit = arguments.value(QStringLiteral("limit")).toInt(100);
        QJsonArray page;
        for (auto index = offset; index < tools.size() && index < offset + limit; ++index)
            page.append(toolSummary(tools.at(index).toObject()));
        QJsonObject structured{
            {QStringLiteral("toolset_version"),
             jsonInteger(m_editorContract, QStringLiteral("toolsetVersion"),
             QStringLiteral("toolset_version"), 0)},
            {QStringLiteral("tools"), page},
        };
        if (offset + page.size() < tools.size()) {
            const auto nextCursor = m_editorToolsCursorCodec.issue(
                QStringLiteral("connector-editor-tools-list/v1"), snapshot, offset + page.size());
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
            const auto haystack =
                QStringLiteral("%1\n%2\n%3\n%4")
                    .arg(ExposurePolicy::operationId(tool),
                         tool.value(QStringLiteral("title")).toString(),
                         tool.value(QStringLiteral("description")).toString(), category);
            if (!haystack.contains(query, Qt::CaseInsensitive))
                continue;
            matches.append(toolSummary(tool));
            if (matches.size() >= limit)
                break;
        }
        const QJsonObject structured{
            {QStringLiteral("toolset_version"),
             jsonInteger(m_editorContract, QStringLiteral("toolsetVersion"),
             QStringLiteral("toolset_version"), 0)},
            {QStringLiteral("tools"), matches},
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
            {QStringLiteral("toolset_version"),
             jsonInteger(m_editorContract, QStringLiteral("toolsetVersion"),
             QStringLiteral("toolset_version"), 0)},
            {QStringLiteral("typed_compatibility"),
             known ? compatibilityFor(*known) : QStringLiteral("generic_only")},
            {QStringLiteral("availability"), availability},
        };
        return {AutomationWire::Mcp::makeToolCallResult(structured)};
    }

}
