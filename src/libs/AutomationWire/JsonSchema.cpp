#include "JsonSchema.h"

#include "CanonicalJson.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <functional>

namespace AutomationWire {
    namespace {
        constexpr int MaxSchemaDepth = JsonResourceLimits{}.maximumDepth;
        constexpr int MaxIssues = 64;

        const QSet<QString> &supportedKeywords() {
            static const QSet<QString> keywords{
                QStringLiteral("$schema"),
                QStringLiteral("$id"),
                QStringLiteral("$anchor"),
                QStringLiteral("$defs"),
                QStringLiteral("$ref"),
                QStringLiteral("$comment"),
                QStringLiteral("title"),
                QStringLiteral("description"),
                QStringLiteral("default"),
                QStringLiteral("examples"),
                QStringLiteral("deprecated"),
                QStringLiteral("readOnly"),
                QStringLiteral("writeOnly"),
                QStringLiteral("type"),
                QStringLiteral("properties"),
                QStringLiteral("required"),
                QStringLiteral("additionalProperties"),
                QStringLiteral("items"),
                QStringLiteral("enum"),
                QStringLiteral("const"),
                QStringLiteral("minimum"),
                QStringLiteral("maximum"),
                QStringLiteral("exclusiveMinimum"),
                QStringLiteral("exclusiveMaximum"),
                QStringLiteral("multipleOf"),
                QStringLiteral("minItems"),
                QStringLiteral("maxItems"),
                QStringLiteral("uniqueItems"),
                QStringLiteral("minLength"),
                QStringLiteral("maxLength"),
                QStringLiteral("pattern"),
                QStringLiteral("format"),
                QStringLiteral("minProperties"),
                QStringLiteral("maxProperties"),
                QStringLiteral("oneOf"),
                QStringLiteral("x-mcp-header"),
            };
            return keywords;
        }

        const QSet<QString> &supportedTypes() {
            static const QSet<QString> types{
                QStringLiteral("null"),   QStringLiteral("boolean"), QStringLiteral("object"),
                QStringLiteral("array"),  QStringLiteral("number"),  QStringLiteral("integer"),
                QStringLiteral("string"),
            };
            return types;
        }

        QString pointerToken(QString value) {
            value.replace(QStringLiteral("~"), QStringLiteral("~0"));
            value.replace(QStringLiteral("/"), QStringLiteral("~1"));
            return value;
        }

        QString childPath(const QString &base, const QString &child) {
            return base + QStringLiteral("/") + pointerToken(child);
        }

        bool isFiniteNumber(const QJsonValue &value) {
            return value.isDouble() && std::isfinite(value.toDouble());
        }

        bool isIntegerNumber(const QJsonValue &value) {
            return isFiniteNumber(value) && std::trunc(value.toDouble()) == value.toDouble() &&
                   std::abs(value.toDouble()) <= static_cast<double>(MaximumSafeJsonInteger);
        }

        void addIssue(QList<SchemaIssue> &issues, const SchemaIssueCode code,
                      const QString &instancePath, const QString &schemaPath,
                      const QString &message) {
            if (issues.size() >= MaxIssues)
                return;
            issues.append({code, instancePath, schemaPath, message});
        }

        SchemaValidationResult checkResourceLimits(const QJsonValue &value,
                                                   const JsonResourceLimits &limits,
                                                   const bool enforceSafeIntegers) {
            SchemaValidationResult result;
            if (limits.maximumStringCodeUnits < 0 || limits.maximumArrayItems < 0 ||
                limits.maximumObjectProperties < 0 || limits.maximumNodes < 1 ||
                limits.maximumDepth < 0) {
                addIssue(result.issues, SchemaIssueCode::InvalidSchema, {}, {},
                         QStringLiteral("JSON resource limits must be non-negative"));
                return result;
            }

            struct PendingValue {
                QJsonValue value;
                QString path;
                int depth = 0;
            };

            QList<PendingValue> pending{
                {value, QString(), 0}
            };
            qint64 nodes = 0;
            while (!pending.isEmpty() && result.issues.size() < MaxIssues) {
                const auto current = pending.takeLast();
                if (++nodes > limits.maximumNodes) {
                    addIssue(result.issues, SchemaIssueCode::LimitExceeded, current.path, {},
                             QStringLiteral("JSON value exceeds the node limit"));
                    break;
                }
                if (current.depth > limits.maximumDepth) {
                    addIssue(result.issues, SchemaIssueCode::LimitExceeded, current.path, {},
                             QStringLiteral("JSON value exceeds the nesting-depth limit"));
                    continue;
                }
                if (current.value.isUndefined()) {
                    addIssue(result.issues, SchemaIssueCode::ValidationFailed, current.path, {},
                             QStringLiteral("Undefined is not a JSON value"));
                    continue;
                }
                if (current.value.isDouble() && !std::isfinite(current.value.toDouble())) {
                    addIssue(result.issues, SchemaIssueCode::ValidationFailed, current.path, {},
                             QStringLiteral("JSON numbers must be finite"));
                    continue;
                }
                if (current.value.isDouble() && enforceSafeIntegers &&
                    std::trunc(current.value.toDouble()) == current.value.toDouble() &&
                    std::abs(current.value.toDouble()) >
                        static_cast<double>(MaximumSafeJsonInteger)) {
                    addIssue(
                        result.issues, SchemaIssueCode::LimitExceeded, current.path, {},
                        QStringLiteral("JSON integer is outside the interoperable safe range"));
                    continue;
                }
                if (current.value.isString()) {
                    if (current.value.toString().size() > limits.maximumStringCodeUnits) {
                        addIssue(result.issues, SchemaIssueCode::LimitExceeded, current.path, {},
                                 QStringLiteral("JSON string exceeds the length limit"));
                    }
                    continue;
                }
                if (current.value.isArray()) {
                    const auto array = current.value.toArray();
                    if (array.size() > limits.maximumArrayItems) {
                        addIssue(result.issues, SchemaIssueCode::LimitExceeded, current.path, {},
                                 QStringLiteral("JSON array exceeds the item limit"));
                        continue;
                    }
                    for (qsizetype index = 0; index < array.size(); ++index) {
                        pending.append({array.at(index),
                                        childPath(current.path, QString::number(index)),
                                        current.depth + 1});
                    }
                    continue;
                }
                if (current.value.isObject()) {
                    const auto object = current.value.toObject();
                    if (object.size() > limits.maximumObjectProperties) {
                        addIssue(result.issues, SchemaIssueCode::LimitExceeded, current.path, {},
                                 QStringLiteral("JSON object exceeds the property limit"));
                        continue;
                    }
                    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                        if (it.key().size() > limits.maximumStringCodeUnits) {
                            addIssue(result.issues, SchemaIssueCode::LimitExceeded, current.path,
                                     {},
                                     QStringLiteral("JSON object key exceeds the length limit"));
                            continue;
                        }
                        pending.append(
                            {it.value(), childPath(current.path, it.key()), current.depth + 1});
                    }
                }
            }
            return result;
        }

        bool decodePointerToken(const QString &encoded, QString &decoded) {
            decoded.clear();
            for (qsizetype index = 0; index < encoded.size(); ++index) {
                if (encoded.at(index) != u'~') {
                    decoded.append(encoded.at(index));
                    continue;
                }
                if (index + 1 >= encoded.size())
                    return false;
                const auto escaped = encoded.at(++index);
                if (escaped == u'0')
                    decoded.append(u'~');
                else if (escaped == u'1')
                    decoded.append(u'/');
                else
                    return false;
            }
            return true;
        }

        std::optional<QJsonValue> resolveSchemaReference(const QJsonValue &root,
                                                         const QString &reference) {
            if (reference == QStringLiteral("#"))
                return root;
            if (!reference.startsWith(QStringLiteral("#/")))
                return std::nullopt;

            QJsonValue current = root;
            const auto components = reference.sliced(2).split(u'/');
            for (qsizetype index = 0; index < components.size();) {
                if (!current.isObject())
                    return std::nullopt;

                QString keyword;
                if (!decodePointerToken(components.at(index), keyword))
                    return std::nullopt;
                const auto schema = current.toObject();
                if (!schema.contains(keyword))
                    return std::nullopt;

                if (keyword == QStringLiteral("items") ||
                    keyword == QStringLiteral("additionalProperties")) {
                    current = schema.value(keyword);
                    ++index;
                    continue;
                }

                if (keyword == QStringLiteral("$defs") || keyword == QStringLiteral("properties")) {
                    if (++index >= components.size() || !schema.value(keyword).isObject())
                        return std::nullopt;
                    QString name;
                    if (!decodePointerToken(components.at(index), name))
                        return std::nullopt;
                    const auto entries = schema.value(keyword).toObject();
                    if (!entries.contains(name))
                        return std::nullopt;
                    current = entries.value(name);
                    ++index;
                    continue;
                }

                if (keyword == QStringLiteral("oneOf")) {
                    if (++index >= components.size() || !schema.value(keyword).isArray())
                        return std::nullopt;
                    static const QRegularExpression arrayIndexExpression(
                        QStringLiteral("^(?:0|[1-9][0-9]*)$"));
                    if (!arrayIndexExpression.match(components.at(index)).hasMatch())
                        return std::nullopt;
                    bool ok = false;
                    const auto branchIndex = components.at(index).toLongLong(&ok);
                    const auto branches = schema.value(keyword).toArray();
                    if (!ok || branchIndex < 0 || branchIndex >= branches.size())
                        return std::nullopt;
                    current = branches.at(branchIndex);
                    ++index;
                    continue;
                }

                return std::nullopt;
            }

            if (!current.isBool() && !current.isObject())
                return std::nullopt;
            return current;
        }

        bool validateTypeDeclaration(const QJsonValue &type, QList<SchemaIssue> &issues,
                                     const QString &schemaPath) {
            QSet<QString> seen;
            const auto checkType = [&](const QJsonValue &entry, const QString &entryPath) {
                if (!entry.isString() || !supportedTypes().contains(entry.toString())) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {}, entryPath,
                             QStringLiteral("Schema type must be a supported type name"));
                    return false;
                }
                if (seen.contains(entry.toString())) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {}, entryPath,
                             QStringLiteral("Schema type names must be unique"));
                    return false;
                }
                seen.insert(entry.toString());
                return true;
            };

            if (type.isString())
                return checkType(type, schemaPath);
            if (!type.isArray() || type.toArray().isEmpty()) {
                addIssue(issues, SchemaIssueCode::InvalidSchema, {}, schemaPath,
                         QStringLiteral("Schema type must be a string or a non-empty array"));
                return false;
            }
            bool ok = true;
            const auto types = type.toArray();
            for (qsizetype index = 0; index < types.size(); ++index) {
                ok &= checkType(types.at(index), childPath(schemaPath, QString::number(index)));
            }
            return ok;
        }

        bool checkSchemaNode(const QJsonValue &schema, const QJsonValue &root,
                             QList<SchemaIssue> &issues, const QString &schemaPath,
                             const int depth) {
            if (depth > MaxSchemaDepth) {
                addIssue(issues, SchemaIssueCode::LimitExceeded, {}, schemaPath,
                         QStringLiteral("Schema nesting exceeds the supported limit"));
                return false;
            }
            if (schema.isBool())
                return true;
            if (!schema.isObject()) {
                addIssue(issues, SchemaIssueCode::InvalidSchema, {}, schemaPath,
                         QStringLiteral("A JSON Schema must be an object or boolean"));
                return false;
            }

            bool ok = true;
            const auto object = schema.toObject();
            for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
                if (!supportedKeywords().contains(it.key())) {
                    addIssue(issues, SchemaIssueCode::UnsupportedKeyword, {},
                             childPath(schemaPath, it.key()),
                             QStringLiteral("Unsupported JSON Schema keyword: %1").arg(it.key()));
                    ok = false;
                }
            }

            if (object.contains(QStringLiteral("$schema"))) {
                const auto dialect = object.value(QStringLiteral("$schema"));
                if (!dialect.isString() ||
                    dialect.toString() != QString::fromLatin1(JsonSchema202012)) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, QStringLiteral("$schema")),
                             QStringLiteral("Only JSON Schema 2020-12 is supported"));
                    ok = false;
                }
            }
            for (const auto &annotation :
                 {QStringLiteral("$id"), QStringLiteral("$anchor"), QStringLiteral("$comment"),
                  QStringLiteral("title"), QStringLiteral("description"),
                  QStringLiteral("x-mcp-header")}) {
                if (object.contains(annotation) && !object.value(annotation).isString()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, annotation),
                             QStringLiteral("Schema annotation must be a string"));
                    ok = false;
                }
            }
            for (const auto &annotation : {QStringLiteral("deprecated"), QStringLiteral("readOnly"),
                                           QStringLiteral("writeOnly")}) {
                if (object.contains(annotation) && !object.value(annotation).isBool()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, annotation),
                             QStringLiteral("Schema annotation must be a boolean"));
                    ok = false;
                }
            }
            if (object.contains(QStringLiteral("examples")) &&
                !object.value(QStringLiteral("examples")).isArray()) {
                addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                         childPath(schemaPath, QStringLiteral("examples")),
                         QStringLiteral("examples must be an array"));
                ok = false;
            }
            if (object.contains(QStringLiteral("type"))) {
                ok &= validateTypeDeclaration(object.value(QStringLiteral("type")), issues,
                                              childPath(schemaPath, QStringLiteral("type")));
            }

            if (object.contains(QStringLiteral("$ref"))) {
                const auto reference = object.value(QStringLiteral("$ref"));
                if (!reference.isString() || !resolveSchemaReference(root, reference.toString())) {
                    addIssue(issues, SchemaIssueCode::UnresolvedReference, {},
                             childPath(schemaPath, QStringLiteral("$ref")),
                             QStringLiteral(
                                 "Only resolvable local JSON Pointer references are supported"));
                    ok = false;
                }
            }

            if (object.contains(QStringLiteral("$defs"))) {
                const auto definitions = object.value(QStringLiteral("$defs"));
                if (!definitions.isObject()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, QStringLiteral("$defs")),
                             QStringLiteral("$defs must be an object"));
                    ok = false;
                } else {
                    const auto definitionObject = definitions.toObject();
                    for (auto it = definitionObject.constBegin(); it != definitionObject.constEnd();
                         ++it) {
                        ok &= checkSchemaNode(
                            it.value(), root, issues,
                            childPath(childPath(schemaPath, QStringLiteral("$defs")), it.key()),
                            depth + 1);
                    }
                }
            }

            if (object.contains(QStringLiteral("properties"))) {
                const auto properties = object.value(QStringLiteral("properties"));
                if (!properties.isObject()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, QStringLiteral("properties")),
                             QStringLiteral("properties must be an object"));
                    ok = false;
                } else {
                    const auto propertyObject = properties.toObject();
                    for (auto it = propertyObject.constBegin(); it != propertyObject.constEnd();
                         ++it) {
                        ok &= checkSchemaNode(
                            it.value(), root, issues,
                            childPath(childPath(schemaPath, QStringLiteral("properties")),
                                      it.key()),
                            depth + 1);
                    }
                }
            }

            if (object.contains(QStringLiteral("required"))) {
                const auto required = object.value(QStringLiteral("required"));
                QSet<QString> seen;
                if (!required.isArray()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, QStringLiteral("required")),
                             QStringLiteral("required must be an array"));
                    ok = false;
                } else {
                    const auto requiredArray = required.toArray();
                    for (qsizetype index = 0; index < requiredArray.size(); ++index) {
                        const auto value = requiredArray.at(index);
                        if (!value.isString() || seen.contains(value.toString())) {
                            addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                                     childPath(childPath(schemaPath, QStringLiteral("required")),
                                               QString::number(index)),
                                     QStringLiteral("required entries must be unique strings"));
                            ok = false;
                        } else {
                            seen.insert(value.toString());
                        }
                    }
                }
            }

            for (const auto &keyword :
                 {QStringLiteral("additionalProperties"), QStringLiteral("items")}) {
                if (!object.contains(keyword))
                    continue;
                const auto value = object.value(keyword);
                if (!value.isBool() && !value.isObject()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, keyword),
                             QStringLiteral("%1 must contain a schema").arg(keyword));
                    ok = false;
                } else {
                    ok &= checkSchemaNode(value, root, issues, childPath(schemaPath, keyword),
                                          depth + 1);
                }
            }

            if (object.contains(QStringLiteral("enum"))) {
                const auto values = object.value(QStringLiteral("enum"));
                if (!values.isArray() || values.toArray().isEmpty()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, QStringLiteral("enum")),
                             QStringLiteral("enum must be a non-empty array"));
                    ok = false;
                } else {
                    const auto array = values.toArray();
                    QSet<QByteArray> seen;
                    for (const auto &entry : array) {
                        QString canonicalError;
                        const auto encoded = canonicalJson(entry, &canonicalError);
                        if (!canonicalError.isEmpty() || seen.contains(encoded)) {
                            addIssue(
                                issues, SchemaIssueCode::InvalidSchema, {},
                                childPath(schemaPath, QStringLiteral("enum")),
                                QStringLiteral("enum values must be unique and canonicalizable"));
                            ok = false;
                            break;
                        }
                        seen.insert(encoded);
                    }
                }
            }

            for (const auto &keyword :
                 {QStringLiteral("minimum"), QStringLiteral("maximum"),
                  QStringLiteral("exclusiveMinimum"), QStringLiteral("exclusiveMaximum"),
                  QStringLiteral("multipleOf")}) {
                if (object.contains(keyword) && !isFiniteNumber(object.value(keyword))) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, keyword),
                             QStringLiteral("%1 must be a finite number").arg(keyword));
                    ok = false;
                }
            }
            if (object.contains(QStringLiteral("multipleOf")) &&
                object.value(QStringLiteral("multipleOf")).toDouble() <= 0.0) {
                addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                         childPath(schemaPath, QStringLiteral("multipleOf")),
                         QStringLiteral("multipleOf must be greater than zero"));
                ok = false;
            }

            for (const auto &keyword :
                 {QStringLiteral("minItems"), QStringLiteral("maxItems"),
                  QStringLiteral("minLength"), QStringLiteral("maxLength"),
                  QStringLiteral("minProperties"), QStringLiteral("maxProperties")}) {
                if (!object.contains(keyword))
                    continue;
                const auto value = object.value(keyword);
                if (!isIntegerNumber(value) || value.toDouble() < 0.0) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, keyword),
                             QStringLiteral("%1 must be a non-negative integer").arg(keyword));
                    ok = false;
                }
            }
            if (object.contains(QStringLiteral("uniqueItems")) &&
                !object.value(QStringLiteral("uniqueItems")).isBool()) {
                addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                         childPath(schemaPath, QStringLiteral("uniqueItems")),
                         QStringLiteral("uniqueItems must be a boolean"));
                ok = false;
            }
            if (object.contains(QStringLiteral("pattern"))) {
                const auto pattern = object.value(QStringLiteral("pattern"));
                const QRegularExpression expression(pattern.toString());
                if (!pattern.isString() || !expression.isValid()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, QStringLiteral("pattern")),
                             QStringLiteral("pattern must be a valid regular expression"));
                    ok = false;
                }
            }
            if (object.contains(QStringLiteral("format"))) {
                const auto format = object.value(QStringLiteral("format"));
                if (!format.isString() || (format.toString() != QStringLiteral("uuid") &&
                                           format.toString() != QStringLiteral("uri"))) {
                    addIssue(issues, SchemaIssueCode::UnsupportedKeyword, {},
                             childPath(schemaPath, QStringLiteral("format")),
                             QStringLiteral("Only uuid and uri formats are supported"));
                    ok = false;
                }
            }
            if (object.contains(QStringLiteral("oneOf"))) {
                const auto branches = object.value(QStringLiteral("oneOf"));
                if (!branches.isArray() || branches.toArray().isEmpty()) {
                    addIssue(issues, SchemaIssueCode::InvalidSchema, {},
                             childPath(schemaPath, QStringLiteral("oneOf")),
                             QStringLiteral("oneOf must be a non-empty array"));
                    ok = false;
                } else {
                    const auto array = branches.toArray();
                    for (qsizetype index = 0; index < array.size(); ++index) {
                        ok &= checkSchemaNode(
                            array.at(index), root, issues,
                            childPath(childPath(schemaPath, QStringLiteral("oneOf")),
                                      QString::number(index)),
                            depth + 1);
                    }
                }
            }
            return ok;
        }

        bool typeMatches(const QJsonValue &instance, const QString &type) {
            if (type == QStringLiteral("null"))
                return instance.isNull();
            if (type == QStringLiteral("boolean"))
                return instance.isBool();
            if (type == QStringLiteral("object"))
                return instance.isObject();
            if (type == QStringLiteral("array"))
                return instance.isArray();
            if (type == QStringLiteral("number"))
                return isFiniteNumber(instance);
            if (type == QStringLiteral("integer"))
                return isIntegerNumber(instance);
            if (type == QStringLiteral("string"))
                return instance.isString();
            return false;
        }

        bool declaredTypeMatches(const QJsonValue &instance, const QJsonValue &type) {
            if (type.isString())
                return typeMatches(instance, type.toString());
            const auto types = type.toArray();
            return std::any_of(types.constBegin(), types.constEnd(), [&](const QJsonValue &entry) {
                return typeMatches(instance, entry.toString());
            });
        }

        bool valueInArray(const QJsonValue &value, const QJsonArray &array) {
            return std::any_of(
                array.constBegin(), array.constEnd(),
                [&](const QJsonValue &candidate) { return canonicalJsonEqual(value, candidate); });
        }

        bool validateInstanceNode(const QJsonValue &instance, const QJsonValue &schema,
                                  const QJsonValue &root, QList<SchemaIssue> &issues,
                                  const QString &instancePath, const QString &schemaPath,
                                  const int depth, QSet<QString> &activeReferences) {
            if (depth > MaxSchemaDepth) {
                addIssue(issues, SchemaIssueCode::LimitExceeded, instancePath, schemaPath,
                         QStringLiteral("Validation nesting exceeds the supported limit"));
                return false;
            }
            if (schema.isBool()) {
                if (schema.toBool())
                    return true;
                addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath, schemaPath,
                         QStringLiteral("The false schema rejects every value"));
                return false;
            }

            const auto object = schema.toObject();
            bool ok = true;
            if (object.contains(QStringLiteral("$ref"))) {
                const auto reference = object.value(QStringLiteral("$ref")).toString();
                const auto resolved = resolveSchemaReference(root, reference);
                if (!resolved) {
                    addIssue(issues, SchemaIssueCode::UnresolvedReference, instancePath,
                             childPath(schemaPath, QStringLiteral("$ref")),
                             QStringLiteral("The local schema reference cannot be resolved"));
                    return false;
                }
                const auto referenceKey =
                    QString::number(reference.size()) + u':' + reference + instancePath;
                if (activeReferences.contains(referenceKey)) {
                    addIssue(
                        issues, SchemaIssueCode::LimitExceeded, instancePath,
                        childPath(schemaPath, QStringLiteral("$ref")),
                        QStringLiteral("Cyclic schema reference does not consume the instance"));
                    return false;
                }
                activeReferences.insert(referenceKey);
                ok &= validateInstanceNode(instance, *resolved, root, issues, instancePath,
                                           reference, depth + 1, activeReferences);
                activeReferences.remove(referenceKey);
            }

            if (object.contains(QStringLiteral("type")) &&
                !declaredTypeMatches(instance, object.value(QStringLiteral("type")))) {
                addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                         childPath(schemaPath, QStringLiteral("type")),
                         QStringLiteral("Value does not match the declared type"));
                return false;
            }
            if (object.contains(QStringLiteral("const")) &&
                !canonicalJsonEqual(instance, object.value(QStringLiteral("const")))) {
                addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                         childPath(schemaPath, QStringLiteral("const")),
                         QStringLiteral("Value does not match const"));
                ok = false;
            }
            if (object.contains(QStringLiteral("enum")) &&
                !valueInArray(instance, object.value(QStringLiteral("enum")).toArray())) {
                addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                         childPath(schemaPath, QStringLiteral("enum")),
                         QStringLiteral("Value is not a member of enum"));
                ok = false;
            }

            if (isFiniteNumber(instance)) {
                const auto number = instance.toDouble();
                const auto compare = [&](const QString &keyword, const auto predicate,
                                         const QString &message) {
                    if (object.contains(keyword) &&
                        !predicate(number, object.value(keyword).toDouble())) {
                        addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                                 childPath(schemaPath, keyword), message);
                        ok = false;
                    }
                };
                compare(QStringLiteral("minimum"), std::greater_equal<double>(),
                        QStringLiteral("Number is below minimum"));
                compare(QStringLiteral("maximum"), std::less_equal<double>(),
                        QStringLiteral("Number is above maximum"));
                compare(QStringLiteral("exclusiveMinimum"), std::greater<double>(),
                        QStringLiteral("Number is not above exclusiveMinimum"));
                compare(QStringLiteral("exclusiveMaximum"), std::less<double>(),
                        QStringLiteral("Number is not below exclusiveMaximum"));
                if (object.contains(QStringLiteral("multipleOf"))) {
                    const auto divisor = object.value(QStringLiteral("multipleOf")).toDouble();
                    const auto quotient = number / divisor;
                    if (std::abs(quotient - std::round(quotient)) > 1e-9) {
                        addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                                 childPath(schemaPath, QStringLiteral("multipleOf")),
                                 QStringLiteral("Number is not a multipleOf value"));
                        ok = false;
                    }
                }
            }

            if (instance.isString()) {
                const auto string = instance.toString();
                const auto length = string.toUcs4().size();
                if (object.contains(QStringLiteral("minLength")) &&
                    length < object.value(QStringLiteral("minLength")).toInteger()) {
                    addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                             childPath(schemaPath, QStringLiteral("minLength")),
                             QStringLiteral("String is shorter than minLength"));
                    ok = false;
                }
                if (object.contains(QStringLiteral("maxLength")) &&
                    length > object.value(QStringLiteral("maxLength")).toInteger()) {
                    addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                             childPath(schemaPath, QStringLiteral("maxLength")),
                             QStringLiteral("String is longer than maxLength"));
                    ok = false;
                }
                if (object.contains(QStringLiteral("pattern")) &&
                    !QRegularExpression(object.value(QStringLiteral("pattern")).toString())
                         .match(string)
                         .hasMatch()) {
                    addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                             childPath(schemaPath, QStringLiteral("pattern")),
                             QStringLiteral("String does not match pattern"));
                    ok = false;
                }
                if (object.value(QStringLiteral("format")).toString() == QStringLiteral("uuid")) {
                    static const QRegularExpression uuidExpression(
                        QStringLiteral("^[0-9A-Fa-f]{8}-[0-9A-Fa-f]{4}-[0-9A-Fa-f]{4}-"
                                       "[0-9A-Fa-f]{4}-[0-9A-Fa-f]{12}$"));
                    if (!uuidExpression.match(string).hasMatch()) {
                        addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                                 childPath(schemaPath, QStringLiteral("format")),
                                 QStringLiteral("String is not a UUID"));
                        ok = false;
                    }
                } else if (object.value(QStringLiteral("format")).toString() ==
                           QStringLiteral("uri")) {
                    const QUrl url(string, QUrl::StrictMode);
                    if (!url.isValid() || url.isRelative()) {
                        addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                                 childPath(schemaPath, QStringLiteral("format")),
                                 QStringLiteral("String is not an absolute URI"));
                        ok = false;
                    }
                }
            }

            if (instance.isArray()) {
                const auto array = instance.toArray();
                if (object.contains(QStringLiteral("minItems")) &&
                    array.size() < object.value(QStringLiteral("minItems")).toInteger()) {
                    addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                             childPath(schemaPath, QStringLiteral("minItems")),
                             QStringLiteral("Array has fewer than minItems"));
                    ok = false;
                }
                if (object.contains(QStringLiteral("maxItems")) &&
                    array.size() > object.value(QStringLiteral("maxItems")).toInteger()) {
                    addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                             childPath(schemaPath, QStringLiteral("maxItems")),
                             QStringLiteral("Array has more than maxItems"));
                    ok = false;
                }
                if (object.value(QStringLiteral("uniqueItems")).toBool()) {
                    QSet<QByteArray> seen;
                    for (const auto &entry : array) {
                        QString canonicalError;
                        const auto encoded = canonicalJson(entry, &canonicalError);
                        if (!canonicalError.isEmpty() || seen.contains(encoded)) {
                            addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                                     childPath(schemaPath, QStringLiteral("uniqueItems")),
                                     QStringLiteral("Array values are not unique"));
                            ok = false;
                            break;
                        }
                        seen.insert(encoded);
                    }
                }
                if (object.contains(QStringLiteral("items"))) {
                    for (qsizetype index = 0; index < array.size(); ++index) {
                        ok &= validateInstanceNode(
                            array.at(index), object.value(QStringLiteral("items")), root, issues,
                            childPath(instancePath, QString::number(index)),
                            childPath(schemaPath, QStringLiteral("items")), depth + 1,
                            activeReferences);
                    }
                }
            }

            if (instance.isObject()) {
                const auto instanceObject = instance.toObject();
                const auto properties = object.value(QStringLiteral("properties")).toObject();
                const auto required = object.value(QStringLiteral("required")).toArray();
                if (object.contains(QStringLiteral("minProperties")) &&
                    instanceObject.size() <
                        object.value(QStringLiteral("minProperties")).toInteger()) {
                    addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                             childPath(schemaPath, QStringLiteral("minProperties")),
                             QStringLiteral("Object has fewer than minProperties"));
                    ok = false;
                }
                if (object.contains(QStringLiteral("maxProperties")) &&
                    instanceObject.size() >
                        object.value(QStringLiteral("maxProperties")).toInteger()) {
                    addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                             childPath(schemaPath, QStringLiteral("maxProperties")),
                             QStringLiteral("Object has more than maxProperties"));
                    ok = false;
                }
                for (const auto &requiredValue : required) {
                    if (!instanceObject.contains(requiredValue.toString())) {
                        addIssue(issues, SchemaIssueCode::ValidationFailed,
                                 childPath(instancePath, requiredValue.toString()),
                                 childPath(schemaPath, QStringLiteral("required")),
                                 QStringLiteral("Required property is missing"));
                        ok = false;
                    }
                }
                for (auto it = instanceObject.constBegin(); it != instanceObject.constEnd(); ++it) {
                    if (properties.contains(it.key())) {
                        ok &= validateInstanceNode(
                            it.value(), properties.value(it.key()), root, issues,
                            childPath(instancePath, it.key()),
                            childPath(childPath(schemaPath, QStringLiteral("properties")),
                                      it.key()),
                            depth + 1, activeReferences);
                        continue;
                    }
                    const auto additional =
                        object.contains(QStringLiteral("additionalProperties"))
                            ? object.value(QStringLiteral("additionalProperties"))
                            : QJsonValue(true);
                    if (additional.isBool() && !additional.toBool()) {
                        addIssue(issues, SchemaIssueCode::ValidationFailed,
                                 childPath(instancePath, it.key()),
                                 childPath(schemaPath, QStringLiteral("additionalProperties")),
                                 QStringLiteral("Additional property is not allowed"));
                        ok = false;
                    } else if (additional.isObject()) {
                        ok &= validateInstanceNode(
                            it.value(), additional, root, issues, childPath(instancePath, it.key()),
                            childPath(schemaPath, QStringLiteral("additionalProperties")),
                            depth + 1, activeReferences);
                    }
                }
            }

            if (object.contains(QStringLiteral("oneOf"))) {
                int matches = 0;
                const auto branches = object.value(QStringLiteral("oneOf")).toArray();
                for (qsizetype index = 0; index < branches.size(); ++index) {
                    QList<SchemaIssue> branchIssues;
                    auto branchReferences = activeReferences;
                    validateInstanceNode(instance, branches.at(index), root, branchIssues,
                                         instancePath,
                                         childPath(childPath(schemaPath, QStringLiteral("oneOf")),
                                                   QString::number(index)),
                                         depth + 1, branchReferences);
                    if (branchIssues.isEmpty())
                        ++matches;
                }
                if (matches != 1) {
                    addIssue(issues, SchemaIssueCode::ValidationFailed, instancePath,
                             childPath(schemaPath, QStringLiteral("oneOf")),
                             QStringLiteral("Value must match exactly one oneOf branch"));
                    ok = false;
                }
            }
            return ok;
        }

        QJsonArray stringsToArray(const QStringList &values) {
            QJsonArray result;
            for (const auto &value : values)
                result.append(value);
            return result;
        }
    }

    QJsonObject JsonSchema::document(QJsonObject root, const QJsonObject &definitions) {
        root.insert(QStringLiteral("$schema"), QString::fromLatin1(JsonSchema202012));
        if (!definitions.isEmpty())
            root.insert(QStringLiteral("$defs"), definitions);
        return root;
    }

    QJsonObject JsonSchema::object(const QJsonObject &properties, const QStringList &required,
                                   const bool additionalProperties) {
        QJsonObject result{
            {QStringLiteral("type"),                 QStringLiteral("object")},
            {QStringLiteral("properties"),           properties              },
            {QStringLiteral("additionalProperties"), additionalProperties    },
        };
        if (!required.isEmpty())
            result.insert(QStringLiteral("required"), stringsToArray(required));
        return result;
    }

    QJsonObject JsonSchema::objectWithAdditionalSchema(const QJsonObject &properties,
                                                       const QStringList &required,
                                                       const QJsonValue &additionalSchema) {
        auto result = object(properties, required, false);
        result.insert(QStringLiteral("additionalProperties"), additionalSchema);
        return result;
    }

    QJsonObject JsonSchema::array(const QJsonValue &items, const std::optional<qint64> minimumItems,
                                  const std::optional<qint64> maximumItems) {
        QJsonObject result{
            {QStringLiteral("type"),  QStringLiteral("array")},
            {QStringLiteral("items"), items                  },
        };
        if (minimumItems)
            result.insert(QStringLiteral("minItems"), *minimumItems);
        if (maximumItems)
            result.insert(QStringLiteral("maxItems"), *maximumItems);
        return result;
    }

    QJsonObject JsonSchema::string(const QStringList &values,
                                   const std::optional<qint64> minimumLength,
                                   const std::optional<qint64> maximumLength) {
        QJsonObject result{
            {QStringLiteral("type"), QStringLiteral("string")}
        };
        if (!values.isEmpty())
            result.insert(QStringLiteral("enum"), stringsToArray(values));
        if (minimumLength)
            result.insert(QStringLiteral("minLength"), *minimumLength);
        if (maximumLength)
            result.insert(QStringLiteral("maxLength"), *maximumLength);
        return result;
    }

    QJsonObject JsonSchema::integer(const std::optional<double> minimum,
                                    const std::optional<double> maximum) {
        QJsonObject result{
            {QStringLiteral("type"), QStringLiteral("integer")}
        };
        if (minimum)
            result.insert(QStringLiteral("minimum"), *minimum);
        if (maximum)
            result.insert(QStringLiteral("maximum"), *maximum);
        return result;
    }

    QJsonObject JsonSchema::number(const std::optional<double> minimum,
                                   const std::optional<double> maximum) {
        QJsonObject result{
            {QStringLiteral("type"), QStringLiteral("number")}
        };
        if (minimum)
            result.insert(QStringLiteral("minimum"), *minimum);
        if (maximum)
            result.insert(QStringLiteral("maximum"), *maximum);
        return result;
    }

    QJsonObject JsonSchema::boolean() {
        return {
            {QStringLiteral("type"), QStringLiteral("boolean")}
        };
    }

    QJsonObject JsonSchema::null() {
        return {
            {QStringLiteral("type"), QStringLiteral("null")}
        };
    }

    QJsonObject JsonSchema::enumeration(const QJsonArray &values) {
        return {
            {QStringLiteral("enum"), values}
        };
    }

    QJsonObject JsonSchema::constant(const QJsonValue &value) {
        return {
            {QStringLiteral("const"), value}
        };
    }

    QJsonObject JsonSchema::oneOf(const QJsonArray &schemas) {
        return {
            {QStringLiteral("oneOf"), schemas}
        };
    }

    QJsonObject JsonSchema::reference(const QString &pointer) {
        return {
            {QStringLiteral("$ref"), pointer}
        };
    }

    SchemaValidationResult checkJsonSchema(const QJsonValue &schema) {
        auto result = checkResourceLimits(schema, {}, false);
        if (!result.valid())
            return result;
        checkSchemaNode(schema, schema, result.issues, QStringLiteral("#"), 0);
        return result;
    }

    SchemaValidationResult checkJsonResourceLimits(const QJsonValue &value,
                                                   const JsonResourceLimits &limits) {
        return checkResourceLimits(value, limits, true);
    }

    SchemaValidationResult validateJsonValue(const QJsonValue &instance, const QJsonValue &schema) {
        auto result = checkJsonSchema(schema);
        if (!result.valid())
            return result;
        result = checkJsonResourceLimits(instance);
        if (!result.valid())
            return result;
        QSet<QString> activeReferences;
        validateInstanceNode(instance, schema, schema, result.issues, QString(),
                             QStringLiteral("#"), 0, activeReferences);
        return result;
    }

}
