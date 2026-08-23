#include "SchemaCompatibility.h"

#include "CanonicalJson.h"
#include "JsonSchema.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace AutomationWire {
    namespace {
        constexpr int MaxProofDepth = 128;

        struct Bound {
            double value = 0.0;
            bool inclusive = true;
            bool present = false;
        };

        SchemaCompatibilityResult compatible() {
            return {SchemaCompatibilityStatus::Compatible, {}};
        }

        SchemaCompatibilityResult incompatible(const QString &reason) {
            return {SchemaCompatibilityStatus::Incompatible, reason};
        }

        SchemaCompatibilityResult unproven(const QString &reason) {
            return {SchemaCompatibilityStatus::Unproven, reason};
        }

        SchemaCompatibilityResult invalidSchema(const QString &reason) {
            return {SchemaCompatibilityStatus::InvalidSchema, reason};
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

        std::optional<QJsonValue> resolveReference(const QJsonValue &root,
                                                   const QString &reference) {
            if (reference == QStringLiteral("#"))
                return root;
            if (!reference.startsWith(QStringLiteral("#/")))
                return std::nullopt;
            QJsonValue current = root;
            for (const auto &component : reference.sliced(2).split(u'/')) {
                QString token;
                if (!decodePointerToken(component, token) || !current.isObject())
                    return std::nullopt;
                const auto object = current.toObject();
                if (!object.contains(token))
                    return std::nullopt;
                current = object.value(token);
            }
            return current;
        }

        bool hasValidationSiblings(const QJsonObject &schema, const QString &except) {
            static const QSet<QString> annotations{
                QStringLiteral("$schema"),      QStringLiteral("$id"),
                QStringLiteral("$anchor"),      QStringLiteral("$defs"),
                QStringLiteral("$comment"),     QStringLiteral("title"),
                QStringLiteral("description"),  QStringLiteral("default"),
                QStringLiteral("examples"),     QStringLiteral("deprecated"),
                QStringLiteral("readOnly"),     QStringLiteral("writeOnly"),
                QStringLiteral("x-mcp-header"),
            };
            for (auto it = schema.constBegin(); it != schema.constEnd(); ++it) {
                if (it.key() != except && !annotations.contains(it.key()))
                    return true;
            }
            return false;
        }

        QSet<QString> allTypes() {
            return {
                QStringLiteral("null"),   QStringLiteral("boolean"), QStringLiteral("object"),
                QStringLiteral("array"),  QStringLiteral("number"),  QStringLiteral("integer"),
                QStringLiteral("string"),
            };
        }

        QSet<QString> schemaTypes(const QJsonObject &schema) {
            if (!schema.contains(QStringLiteral("type")))
                return allTypes();
            const auto type = schema.value(QStringLiteral("type"));
            if (type.isString())
                return {type.toString()};
            QSet<QString> result;
            for (const auto &entry : type.toArray())
                result.insert(entry.toString());
            return result;
        }

        bool targetAcceptsType(const QSet<QString> &target, const QString &sourceType) {
            if (target.contains(sourceType))
                return true;
            return sourceType == QStringLiteral("integer") &&
                   target.contains(QStringLiteral("number"));
        }

        bool typesAreSubset(const QSet<QString> &source, const QSet<QString> &target) {
            return std::all_of(source.constBegin(), source.constEnd(), [&](const QString &type) {
                return targetAcceptsType(target, type);
            });
        }

        Bound lowerBound(const QJsonObject &schema) {
            Bound result;
            const auto merge = [&](const QString &keyword, const bool inclusive) {
                if (!schema.contains(keyword))
                    return;
                const Bound candidate{schema.value(keyword).toDouble(), inclusive, true};
                if (!result.present || candidate.value > result.value ||
                    (candidate.value == result.value && !candidate.inclusive && result.inclusive)) {
                    result = candidate;
                }
            };
            merge(QStringLiteral("minimum"), true);
            merge(QStringLiteral("exclusiveMinimum"), false);
            return result;
        }

        Bound upperBound(const QJsonObject &schema) {
            Bound result;
            const auto merge = [&](const QString &keyword, const bool inclusive) {
                if (!schema.contains(keyword))
                    return;
                const Bound candidate{schema.value(keyword).toDouble(), inclusive, true};
                if (!result.present || candidate.value < result.value ||
                    (candidate.value == result.value && !candidate.inclusive && result.inclusive)) {
                    result = candidate;
                }
            };
            merge(QStringLiteral("maximum"), true);
            merge(QStringLiteral("exclusiveMaximum"), false);
            return result;
        }

        bool lowerIsAtLeast(const Bound &source, const Bound &target) {
            if (!target.present)
                return true;
            if (!source.present)
                return false;
            if (source.value > target.value)
                return true;
            if (source.value < target.value)
                return false;
            return target.inclusive || !source.inclusive;
        }

        bool upperIsAtMost(const Bound &source, const Bound &target) {
            if (!target.present)
                return true;
            if (!source.present)
                return false;
            if (source.value < target.value)
                return true;
            if (source.value > target.value)
                return false;
            return target.inclusive || !source.inclusive;
        }

        qint64 integerKeyword(const QJsonObject &schema, const QString &keyword,
                              const qint64 fallback) {
            return schema.contains(keyword) ? schema.value(keyword).toInteger() : fallback;
        }

        QSet<QString> requiredProperties(const QJsonObject &schema) {
            QSet<QString> result;
            for (const auto &entry : schema.value(QStringLiteral("required")).toArray())
                result.insert(entry.toString());
            return result;
        }

        QJsonValue additionalSchema(const QJsonObject &schema) {
            return schema.contains(QStringLiteral("additionalProperties"))
                       ? schema.value(QStringLiteral("additionalProperties"))
                       : QJsonValue(true);
        }

        SchemaCompatibilityResult proveNode(const QJsonValue &sourceSchema,
                                            const QJsonValue &targetSchema,
                                            const QJsonValue &sourceRoot,
                                            const QJsonValue &targetRoot, int depth);

        SchemaCompatibilityResult proveObjectSubset(const QJsonObject &source,
                                                    const QJsonObject &target,
                                                    const QJsonValue &sourceRoot,
                                                    const QJsonValue &targetRoot, const int depth) {
            const auto sourceRequired = requiredProperties(source);
            const auto targetRequired = requiredProperties(target);
            if (!std::all_of(targetRequired.constBegin(), targetRequired.constEnd(),
                             [&](const QString &name) { return sourceRequired.contains(name); })) {
                return incompatible(
                    QStringLiteral("Source does not require every target-required property"));
            }

            const auto sourceProperties = source.value(QStringLiteral("properties")).toObject();
            const auto targetProperties = target.value(QStringLiteral("properties")).toObject();
            const auto sourceAdditional = additionalSchema(source);
            const auto targetAdditional = additionalSchema(target);

            for (auto it = sourceProperties.constBegin(); it != sourceProperties.constEnd(); ++it) {
                QJsonValue targetProperty;
                if (targetProperties.contains(it.key()))
                    targetProperty = targetProperties.value(it.key());
                else if (targetAdditional.isBool() && targetAdditional.toBool())
                    continue;
                else if (targetAdditional.isBool())
                    return incompatible(
                        QStringLiteral("Source may emit property rejected by target: %1")
                            .arg(it.key()));
                else
                    targetProperty = targetAdditional;

                const auto propertyResult =
                    proveNode(it.value(), targetProperty, sourceRoot, targetRoot, depth + 1);
                if (!propertyResult.compatible())
                    return propertyResult;
            }

            for (auto it = targetProperties.constBegin(); it != targetProperties.constEnd(); ++it) {
                if (sourceProperties.contains(it.key()) ||
                    (sourceAdditional.isBool() && !sourceAdditional.toBool())) {
                    continue;
                }
                if (sourceAdditional.isBool() && sourceAdditional.toBool()) {
                    return unproven(
                        QStringLiteral("Unconstrained source additional property may violate %1")
                            .arg(it.key()));
                }
                const auto propertyResult =
                    proveNode(sourceAdditional, it.value(), sourceRoot, targetRoot, depth + 1);
                if (!propertyResult.compatible())
                    return propertyResult;
            }

            if (!(sourceAdditional.isBool() && !sourceAdditional.toBool())) {
                if (targetAdditional.isBool() && !targetAdditional.toBool()) {
                    return incompatible(
                        QStringLiteral("Source permits additional properties rejected by target"));
                }
                if (!targetAdditional.isBool()) {
                    if (sourceAdditional.isBool() && sourceAdditional.toBool()) {
                        return unproven(QStringLiteral("Unconstrained source additional properties "
                                                       "cannot satisfy target schema"));
                    }
                    const auto additionalResult = proveNode(sourceAdditional, targetAdditional,
                                                            sourceRoot, targetRoot, depth + 1);
                    if (!additionalResult.compatible())
                        return additionalResult;
                }
            }

            const auto sourceMinimum = std::max<qint64>(
                integerKeyword(source, QStringLiteral("minProperties"), 0), sourceRequired.size());
            const auto targetMinimum = integerKeyword(target, QStringLiteral("minProperties"), 0);
            if (sourceMinimum < targetMinimum)
                return incompatible(QStringLiteral("Source minProperties is weaker than target"));

            auto sourceMaximum = integerKeyword(source, QStringLiteral("maxProperties"),
                                                std::numeric_limits<qint64>::max());
            if (sourceAdditional.isBool() && !sourceAdditional.toBool())
                sourceMaximum = std::min<qint64>(sourceMaximum, sourceProperties.size());
            const auto targetMaximum = integerKeyword(target, QStringLiteral("maxProperties"),
                                                      std::numeric_limits<qint64>::max());
            if (sourceMaximum > targetMaximum)
                return incompatible(QStringLiteral("Source maxProperties is weaker than target"));
            return compatible();
        }

        SchemaCompatibilityResult proveNode(const QJsonValue &sourceSchema,
                                            const QJsonValue &targetSchema,
                                            const QJsonValue &sourceRoot,
                                            const QJsonValue &targetRoot, const int depth) {
            if (depth > MaxProofDepth)
                return unproven(QStringLiteral("Schema proof exceeds the supported depth"));
            if (canonicalJsonEqual(sourceSchema, targetSchema))
                return compatible();
            if (sourceSchema.isBool())
                return !sourceSchema.toBool() || (targetSchema.isBool() && targetSchema.toBool())
                           ? compatible()
                           : incompatible(
                                 QStringLiteral("The target rejects values allowed by source"));
            if (targetSchema.isBool())
                return targetSchema.toBool()
                           ? compatible()
                           : incompatible(
                                 QStringLiteral("The false target schema rejects source values"));

            auto source = sourceSchema.toObject();
            auto target = targetSchema.toObject();
            if (source.contains(QStringLiteral("$ref"))) {
                if (hasValidationSiblings(source, QStringLiteral("$ref"))) {
                    return unproven(
                        QStringLiteral("A source $ref with validation siblings is not provable"));
                }
                const auto resolved =
                    resolveReference(sourceRoot, source.value(QStringLiteral("$ref")).toString());
                if (!resolved)
                    return invalidSchema(QStringLiteral("Source $ref cannot be resolved"));
                return proveNode(*resolved, targetSchema, sourceRoot, targetRoot, depth + 1);
            }
            if (target.contains(QStringLiteral("$ref"))) {
                if (hasValidationSiblings(target, QStringLiteral("$ref"))) {
                    return unproven(
                        QStringLiteral("A target $ref with validation siblings is not provable"));
                }
                const auto resolved =
                    resolveReference(targetRoot, target.value(QStringLiteral("$ref")).toString());
                if (!resolved)
                    return invalidSchema(QStringLiteral("Target $ref cannot be resolved"));
                return proveNode(sourceSchema, *resolved, sourceRoot, targetRoot, depth + 1);
            }

            if (source.contains(QStringLiteral("const"))) {
                return validateJsonValue(source.value(QStringLiteral("const")), targetSchema)
                               .valid()
                           ? compatible()
                           : incompatible(QStringLiteral("Source const is rejected by target"));
            }
            if (source.contains(QStringLiteral("enum"))) {
                for (const auto &value : source.value(QStringLiteral("enum")).toArray()) {
                    if (!validateJsonValue(value, targetSchema).valid()) {
                        return incompatible(
                            QStringLiteral("At least one source enum value is rejected by target"));
                    }
                }
                return compatible();
            }
            if (target.contains(QStringLiteral("const")) ||
                target.contains(QStringLiteral("enum"))) {
                return unproven(
                    QStringLiteral("An unconstrained source cannot prove a target const or enum"));
            }

            if (source.contains(QStringLiteral("oneOf"))) {
                if (hasValidationSiblings(source, QStringLiteral("oneOf"))) {
                    return unproven(
                        QStringLiteral("A source oneOf with validation siblings is not provable"));
                }
                for (const auto &branch : source.value(QStringLiteral("oneOf")).toArray()) {
                    const auto result =
                        proveNode(branch, targetSchema, sourceRoot, targetRoot, depth + 1);
                    if (!result.compatible())
                        return result;
                }
                return compatible();
            }
            if (target.contains(QStringLiteral("oneOf"))) {
                return unproven(QStringLiteral(
                    "Target oneOf exclusivity cannot be proven without finite values"));
            }

            const auto sourceTypes = schemaTypes(source);
            const auto targetTypes = schemaTypes(target);
            if (!typesAreSubset(sourceTypes, targetTypes))
                return incompatible(
                    QStringLiteral("Source type set is not a subset of target types"));

            if (sourceTypes.contains(QStringLiteral("number")) ||
                sourceTypes.contains(QStringLiteral("integer"))) {
                if (!lowerIsAtLeast(lowerBound(source), lowerBound(target)) ||
                    !upperIsAtMost(upperBound(source), upperBound(target))) {
                    return incompatible(
                        QStringLiteral("Source numeric range is wider than target"));
                }
                if (target.contains(QStringLiteral("multipleOf"))) {
                    if (!source.contains(QStringLiteral("multipleOf"))) {
                        return unproven(
                            QStringLiteral("Source does not constrain target multipleOf"));
                    }
                    const auto quotient = source.value(QStringLiteral("multipleOf")).toDouble() /
                                          target.value(QStringLiteral("multipleOf")).toDouble();
                    if (std::abs(quotient - std::round(quotient)) > 1e-9) {
                        return incompatible(
                            QStringLiteral("Source multipleOf is not a multiple of target"));
                    }
                }
            }

            if (sourceTypes.contains(QStringLiteral("string"))) {
                if (integerKeyword(source, QStringLiteral("minLength"), 0) <
                    integerKeyword(target, QStringLiteral("minLength"), 0)) {
                    return incompatible(QStringLiteral("Source minLength is weaker than target"));
                }
                if (integerKeyword(source, QStringLiteral("maxLength"),
                                   std::numeric_limits<qint64>::max()) >
                    integerKeyword(target, QStringLiteral("maxLength"),
                                   std::numeric_limits<qint64>::max())) {
                    return incompatible(QStringLiteral("Source maxLength is weaker than target"));
                }
                for (const auto &keyword : {QStringLiteral("pattern"), QStringLiteral("format")}) {
                    if (target.contains(keyword) &&
                        source.value(keyword) != target.value(keyword)) {
                        return unproven(QStringLiteral("Different %1 constraints are not provable")
                                            .arg(keyword));
                    }
                }
            }

            if (sourceTypes.contains(QStringLiteral("array"))) {
                if (integerKeyword(source, QStringLiteral("minItems"), 0) <
                    integerKeyword(target, QStringLiteral("minItems"), 0)) {
                    return incompatible(QStringLiteral("Source minItems is weaker than target"));
                }
                if (integerKeyword(source, QStringLiteral("maxItems"),
                                   std::numeric_limits<qint64>::max()) >
                    integerKeyword(target, QStringLiteral("maxItems"),
                                   std::numeric_limits<qint64>::max())) {
                    return incompatible(QStringLiteral("Source maxItems is weaker than target"));
                }
                if (target.value(QStringLiteral("uniqueItems")).toBool() &&
                    !source.value(QStringLiteral("uniqueItems")).toBool()) {
                    return unproven(QStringLiteral("Source does not require unique array items"));
                }
                if (target.contains(QStringLiteral("items"))) {
                    if (!source.contains(QStringLiteral("items"))) {
                        return unproven(QStringLiteral("Source array items are unconstrained"));
                    }
                    const auto itemResult = proveNode(source.value(QStringLiteral("items")),
                                                      target.value(QStringLiteral("items")),
                                                      sourceRoot, targetRoot, depth + 1);
                    if (!itemResult.compatible())
                        return itemResult;
                }
            }

            if (sourceTypes.contains(QStringLiteral("object"))) {
                const auto objectResult =
                    proveObjectSubset(source, target, sourceRoot, targetRoot, depth + 1);
                if (!objectResult.compatible())
                    return objectResult;
            }
            return compatible();
        }
    }

    SchemaCompatibilityResult proveSchemaSubset(const QJsonValue &sourceSchema,
                                                const QJsonValue &targetSchema) {
        const auto sourceCheck = checkJsonSchema(sourceSchema);
        if (!sourceCheck.valid())
            return invalidSchema(QStringLiteral("Source schema is invalid or unsupported"));
        const auto targetCheck = checkJsonSchema(targetSchema);
        if (!targetCheck.valid())
            return invalidSchema(QStringLiteral("Target schema is invalid or unsupported"));
        return proveNode(sourceSchema, targetSchema, sourceSchema, targetSchema, 0);
    }

    ToolSchemaCompatibility checkToolSchemaCompatibility(const QJsonValue &connectorInput,
                                                         const QJsonValue &editorInput,
                                                         const QJsonValue &editorOutput,
                                                         const QJsonValue &connectorOutput) {
        return {
            .input = proveSchemaSubset(connectorInput, editorInput),
            .output = proveSchemaSubset(editorOutput, connectorOutput),
        };
    }

}
