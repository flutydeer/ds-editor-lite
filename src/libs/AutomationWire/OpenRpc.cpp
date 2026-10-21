#include "OpenRpc.h"

#include "JsonSchema.h"

#include <QMap>

namespace AutomationWire {
    namespace {
        constexpr auto Draft7 = "http://json-schema.org/draft-07/schema#";

        QString pointerToken(QString value) {
            return value.replace(u'~', QStringLiteral("~0")).replace(u'/', QStringLiteral("~1"));
        }

        class SchemaExport {
        public:
            QJsonValue convert(const QJsonValue &schema, const QString &base) {
                const auto checked = checkJsonSchema(schema);
                if (!checked.valid()) {
                    error = checked.issues.first().message;
                    return {};
                }
                m_paths.clear();
                auto converted = convertNodes(schema, QStringLiteral("#"), base);
                rewriteReferences(converted);
                return converted;
            }

            QString error;

        private:
            QJsonValue convertNodes(const QJsonValue &schema, const QString &source,
                                    const QString &target) {
                m_paths.insert(source, target);
                if (!schema.isObject())
                    return schema;
                auto result = schema.toObject();
                if (result.contains(QStringLiteral("$id")) ||
                    result.contains(QStringLiteral("$anchor"))) {
                    error = QStringLiteral("OpenRPC export does not support schema IDs or anchors");
                }
                if (result.contains(QStringLiteral("$schema")))
                    result.insert(QStringLiteral("$schema"), QString::fromLatin1(Draft7));
                for (const auto &keyword :
                     {QStringLiteral("properties"), QStringLiteral("$defs")}) {
                    if (!result.contains(keyword))
                        continue;
                    const auto destination = keyword == QStringLiteral("$defs")
                                                 ? QStringLiteral("definitions")
                                                 : keyword;
                    const auto children = result.take(keyword).toObject();
                    QJsonObject converted;
                    for (auto it = children.begin(); it != children.end(); ++it) {
                        const auto token = pointerToken(it.key());
                        converted.insert(it.key(),
                                         convertNodes(it.value(),
                                                      source + u'/' + keyword + u'/' + token,
                                                      target + u'/' + destination + u'/' + token));
                    }
                    result.insert(destination, converted);
                }
                for (const auto &keyword :
                     {QStringLiteral("items"), QStringLiteral("additionalProperties")}) {
                    if (result.contains(keyword))
                        result.insert(keyword,
                                      convertNodes(result.value(keyword), source + u'/' + keyword,
                                                   target + u'/' + keyword));
                }
                if (result.contains(QStringLiteral("oneOf"))) {
                    const auto branches = result.value(QStringLiteral("oneOf")).toArray();
                    QJsonArray converted;
                    for (qsizetype i = 0; i < branches.size(); ++i) {
                        const auto suffix = QStringLiteral("/oneOf/%1").arg(i);
                        converted.append(
                            convertNodes(branches[i], source + suffix, target + suffix));
                    }
                    result.insert(QStringLiteral("oneOf"), converted);
                }
                // Draft 7 ignores siblings of $ref. Keep their paths intact for local references.
                if (result.contains(QStringLiteral("$ref")) && result.size() > 1) {
                    const auto reference = result.take(QStringLiteral("$ref"));
                    result.insert(QStringLiteral("allOf"),
                                  QJsonArray{QJsonObject{{QStringLiteral("$ref"), reference}}});
                }
                return result;
            }

            void rewriteReferences(QJsonValue &schema) {
                if (!schema.isObject())
                    return;
                auto object = schema.toObject();
                if (object.contains(QStringLiteral("$ref"))) {
                    const auto reference = object.value(QStringLiteral("$ref")).toString();
                    const auto found = m_paths.constFind(reference);
                    if (found == m_paths.cend()) {
                        error = QStringLiteral("OpenRPC cannot resolve schema reference: %1")
                                    .arg(reference);
                    } else {
                        object.insert(QStringLiteral("$ref"), found.value());
                    }
                }
                for (const auto &keyword :
                     {QStringLiteral("properties"), QStringLiteral("definitions")}) {
                    if (!object.contains(keyword))
                        continue;
                    auto children = object.value(keyword).toObject();
                    for (auto it = children.begin(); it != children.end(); ++it) {
                        QJsonValue child = it.value();
                        rewriteReferences(child);
                        it.value() = child;
                    }
                    object.insert(keyword, children);
                }
                for (const auto &keyword :
                     {QStringLiteral("items"), QStringLiteral("additionalProperties")}) {
                    if (!object.contains(keyword))
                        continue;
                    auto child = object.value(keyword);
                    rewriteReferences(child);
                    object.insert(keyword, child);
                }
                for (const auto &keyword : {QStringLiteral("oneOf"), QStringLiteral("allOf")}) {
                    if (!object.contains(keyword))
                        continue;
                    auto children = object.value(keyword).toArray();
                    for (qsizetype i = 0; i < children.size(); ++i) {
                        QJsonValue child = children[i];
                        rewriteReferences(child);
                        children[i] = child;
                    }
                    object.insert(keyword, children);
                }
                schema = object;
            }

            QMap<QString, QString> m_paths;
        };

        struct ParameterDescription {
            QJsonArray alternatives;
        };

        QSet<QString> collectParameters(const QJsonObject &schema, const QString &path,
                                        QMap<QString, ParameterDescription> &parameters) {
            const auto properties = schema.value(QStringLiteral("properties")).toObject();
            for (auto it = properties.begin(); it != properties.end(); ++it) {
                parameters[it.key()].alternatives.append(QJsonObject{
                    {QStringLiteral("$ref"),
                     path + QStringLiteral("/properties/") + pointerToken(it.key())}
                });
            }
            QSet<QString> required;
            for (const auto &name : schema.value(QStringLiteral("required")).toArray())
                required.insert(name.toString());
            const auto branches = schema.value(QStringLiteral("oneOf")).toArray();
            QSet<QString> common;
            for (qsizetype i = 0; i < branches.size(); ++i) {
                const auto branchRequired = collectParameters(
                    branches[i].toObject(), path + QStringLiteral("/oneOf/%1").arg(i), parameters);
                if (i == 0)
                    common = branchRequired;
                else
                    common.intersect(branchRequired);
            }
            required.unite(common);
            return required;
        }

        QJsonArray methodParameters(const QJsonObject &schema, const QString &base) {
            QMap<QString, ParameterDescription> parameters;
            const auto required = collectParameters(schema, base, parameters);
            QJsonArray result;
            for (const auto requiredGroup : {true, false}) {
                for (auto it = parameters.begin(); it != parameters.end(); ++it) {
                    if (required.contains(it.key()) != requiredGroup)
                        continue;
                    const auto &alternatives = it.value().alternatives;
                    result.append(QJsonObject{
                        {QStringLiteral("name"),     it.key()                      },
                        {QStringLiteral("required"), requiredGroup                 },
                        {QStringLiteral("schema"),
                         alternatives.size() == 1
                             ? alternatives.first()
                             : QJsonObject{{QStringLiteral("anyOf"), alternatives}}},
                    });
                }
            }
            return result;
        }
    } // namespace

    std::optional<QJsonObject> makeOpenRpcDocument(const QList<ToolContract> &contracts,
                                                   const OpenRpcDocumentInfo &info,
                                                   QString *error) {
        if (error)
            error->clear();
        QJsonObject schemas;
        QJsonArray methods;
        for (const auto &contract : contracts) {
            const auto inputName = contract.operationId + QStringLiteral(".params");
            const auto outputName = contract.operationId + QStringLiteral(".result");
            const auto inputPath =
                QStringLiteral("#/components/schemas/") + pointerToken(inputName);
            const auto outputPath =
                QStringLiteral("#/components/schemas/") + pointerToken(outputName);
            SchemaExport exporter;
            const auto input = exporter.convert(contract.inputSchema, inputPath);
            const auto output = exporter.convert(contract.outputSchema, outputPath);
            if (!exporter.error.isEmpty()) {
                if (error)
                    *error = contract.operationId + QStringLiteral(": ") + exporter.error;
                return std::nullopt;
            }
            schemas.insert(inputName, input);
            schemas.insert(outputName, output);
            const auto mcp = contract.toMcpToolJson();
            methods.append(QJsonObject{
                {QStringLiteral("name"), contract.operationId},
                {QStringLiteral("summary"), contract.title},
                {QStringLiteral("description"), contract.description},
                {QStringLiteral("tags"),
                 QJsonArray{QJsonObject{{QStringLiteral("name"), contract.category}}}},
                {QStringLiteral("paramStructure"), QStringLiteral("by-name")},
                {QStringLiteral("params"), methodParameters(input.toObject(), inputPath)},
                {QStringLiteral("result"),
                 QJsonObject{{QStringLiteral("name"), QStringLiteral("result")},
                             {QStringLiteral("schema"),
                              QJsonObject{{QStringLiteral("$ref"), outputPath}}}}},
                {QStringLiteral("errors"),
                 QJsonArray{QJsonObject{
                     {QStringLiteral("code"), -32000},
                     {QStringLiteral("message"), QStringLiteral("Automation operation failed")},
                     {QStringLiteral("x-ds-description"),
                      QStringLiteral(
                          "The response message describes the failure. error.data.code identifies "
                          "the automation error. Invalid arguments use -32602, internal errors use "
                          "-32603.")}}}},
                {QStringLiteral("x-ds-input-schema"), contract.inputSchema},
                {QStringLiteral("x-ds-tool"),
                 mcp.value(QStringLiteral("_meta"))
                     .toObject()
                     .value(QStringLiteral("org.openvpi.ds-editor-lite/tool"))},
                {QStringLiteral("x-ds-annotations"), contract.annotations},
            });
        }
        methods.append(QJsonObject{
            {QStringLiteral("name"),              QString::fromLatin1(OpenRpcDiscoverMethod)},
            {QStringLiteral("summary"),
             QStringLiteral("Describe the currently available JSON-RPC API")                },
            {QStringLiteral("paramStructure"),    QStringLiteral("by-name")                 },
            {QStringLiteral("params"),            QJsonArray{}                              },
            {QStringLiteral("x-ds-input-schema"), JsonSchema::document(JsonSchema::object())},
            {QStringLiteral("result"),
             QJsonObject{{QStringLiteral("name"), QStringLiteral("OpenRPC document")},
                         {QStringLiteral("schema"),
                          QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                                      {QStringLiteral("required"),
                                       QJsonArray{QStringLiteral("openrpc"), QStringLiteral("info"),
                                                  QStringLiteral("methods")}}}}}            },
        });
        QJsonObject document{
            {QStringLiteral("openrpc"),              QString::fromLatin1(OpenRpcVersion)                                       },
            {QStringLiteral("info"),                 QJsonObject{{QStringLiteral("title"), info.title},
                                                 {QStringLiteral("version"), info.version}}},
            {QStringLiteral("methods"),              methods                                                                   },
            {QStringLiteral("components"),           QJsonObject{{QStringLiteral("schemas"), schemas}}                         },
            {QStringLiteral("x-ds-toolset-version"), static_cast<qint64>(PublicToolsetVersion)                                 },
            {QStringLiteral("x-ds-jsonrpc-profile"),
             QJsonObject{
                 {QStringLiteral("batch"), false},
                 {QStringLiteral("notifications"), false},
                 {QStringLiteral("id"),
                  JsonSchema::oneOf(QJsonArray{
                      JsonSchema::string(),
                      JsonSchema::integer(-MaximumSafeJsonInteger, MaximumSafeJsonInteger)})},
                 {QStringLiteral("description"),
                  QStringLiteral(
                      "One request object per HTTP POST. params must be an object or omitted. "
                      "Missing or null IDs and batch arrays return -32600. Protocol and operation "
                      "errors use JSON-RPC error responses. HTTP admission failures use HTTP "
                      "status codes.")}}                                                                                       },
        };
        if (!info.endpoint.isEmpty()) {
            document.insert(QStringLiteral("servers"),
                            QJsonArray{
                                QJsonObject{{QStringLiteral("name"), QStringLiteral("local")},
                                            {QStringLiteral("url"), info.endpoint}}
            });
        }
        return document;
    }

} // namespace AutomationWire
