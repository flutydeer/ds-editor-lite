#include "../PublicAutomationToolsetExpectations.h"

#include <lite/AutomationWire/AutomationProfile.h>
#include <lite/AutomationWire/CanonicalJson.h>
#include <lite/AutomationWire/ExposurePolicy.h>
#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/McpProtocol.h>
#include <lite/AutomationWire/PublicConstants.h>
#include <lite/AutomationWire/PublicEnums.h>
#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/AutomationWire/SchemaCompatibility.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>

namespace {
    using namespace AutomationWire;

    bool expect(const bool condition, const QString &message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    void collectOpenSchemaPaths(const QJsonValue &schema, const QString &path, QStringList &paths) {
        if (schema.isBool()) {
            if (schema.toBool())
                paths.append(path);
            return;
        }
        if (!schema.isObject())
            return;

        const auto object = schema.toObject();
        for (const auto &mapKeyword : {QStringLiteral("$defs"), QStringLiteral("properties")}) {
            const auto children = object.value(mapKeyword).toObject();
            for (auto it = children.constBegin(); it != children.constEnd(); ++it) {
                collectOpenSchemaPaths(
                    it.value(), QStringLiteral("%1/%2/%3").arg(path, mapKeyword, it.key()), paths);
            }
        }
        for (const auto &schemaKeyword :
             {QStringLiteral("additionalProperties"), QStringLiteral("items")}) {
            if (object.contains(schemaKeyword)) {
                collectOpenSchemaPaths(object.value(schemaKeyword),
                                       QStringLiteral("%1/%2").arg(path, schemaKeyword), paths);
            }
        }
        const auto branches = object.value(QStringLiteral("oneOf")).toArray();
        for (qsizetype index = 0; index < branches.size(); ++index) {
            collectOpenSchemaPaths(branches.at(index),
                                   QStringLiteral("%1/oneOf/%2").arg(path).arg(index), paths);
        }
    }

    bool hasStrictObjectRoot(const QJsonObject &schema) {
        if (schema.value(QStringLiteral("additionalProperties")) == false)
            return true;
        const auto branches = schema.value(QStringLiteral("oneOf")).toArray();
        if (branches.isEmpty())
            return false;
        return std::all_of(branches.begin(), branches.end(), [](const QJsonValue &branch) {
            return branch.toObject().value(QStringLiteral("additionalProperties")) == false;
        });
    }

    bool testCanonicalJson() {
        bool ok = true;
        const QJsonObject first{
            {QStringLiteral("z"), QJsonArray{3, 2, 1}                                                                         },
            {QStringLiteral("a"), QJsonObject{{QStringLiteral("b"), true},
                                              {QStringLiteral("a"), QStringLiteral("value")}}},
        };
        const QJsonObject second{
            {QStringLiteral("a"), QJsonObject{{QStringLiteral("a"), QStringLiteral("value")},
                                              {QStringLiteral("b"), true}}},
            {QStringLiteral("z"), QJsonArray{3, 2, 1}                                                      },
        };
        ok &= expect(canonicalJson(first) == canonicalJson(second),
                     QStringLiteral("canonical JSON must ignore object insertion order"));
        ok &= expect(canonicalJson(first) ==
                         QByteArrayLiteral("{\"a\":{\"a\":\"value\",\"b\":true},\"z\":[3,2,1]}"),
                     QStringLiteral("canonical JSON must use sorted compact object keys"));
        const auto digest = sha256Digest(first);
        ok &= expect(digest.startsWith(QStringLiteral("sha256:")) && digest.size() == 71,
                     QStringLiteral("canonical digest must be a tagged SHA-256 value"));
        return ok;
    }

    bool testJsonSchema() {
        bool ok = true;
        const auto item = JsonSchema::object(
            {
                {QStringLiteral("code"),
                 JsonSchema::string({QStringLiteral("a"), QStringLiteral("b")})},
                {QStringLiteral("count"), JsonSchema::integer(1.0, 3.0)},
        },
            {QStringLiteral("code"), QStringLiteral("count")});
        const QJsonObject definitions{
            {QStringLiteral("Item"), item}
        };
        const auto schema = JsonSchema::document(
            JsonSchema::object(
                {
                    {QStringLiteral("item"), JsonSchema::reference(QStringLiteral("#/$defs/Item"))},
                    {QStringLiteral("items"),
                     JsonSchema::array(JsonSchema::reference(QStringLiteral("#/$defs/Item")), 1,
                     2)},
                    {QStringLiteral("mode"), JsonSchema::oneOf(QJsonArray{
                                                 JsonSchema::constant(QStringLiteral("x")),
                                                 JsonSchema::constant(QStringLiteral("y")),
                                             })},
        },
                {QStringLiteral("item"), QStringLiteral("items"), QStringLiteral("mode")}),
            definitions);
        const QJsonObject validItem{
            {QStringLiteral("code"),  QStringLiteral("a")},
            {QStringLiteral("count"), 2                  },
        };
        const QJsonObject valid{
            {QStringLiteral("item"),  validItem            },
            {QStringLiteral("items"), QJsonArray{validItem}},
            {QStringLiteral("mode"),  QStringLiteral("x")  },
        };
        ok &= expect(checkJsonSchema(schema).valid(),
                     QStringLiteral("supported JSON Schema must pass schema checking"));
        ok &=
            expect(validateJsonValue(valid, schema).valid(),
                   QStringLiteral("valid JSON must satisfy refs, arrays, ranges, enum and oneOf"));

        auto invalid = valid;
        invalid.insert(QStringLiteral("unexpected"), true);
        ok &= expect(!validateJsonValue(invalid, schema).valid(),
                     QStringLiteral("additionalProperties false must reject unknown fields"));
        invalid = valid;
        invalid.insert(QStringLiteral("item"), QJsonObject{
                                                   {QStringLiteral("code"),  QStringLiteral("c")},
                                                   {QStringLiteral("count"), 4                  }
        });
        ok &= expect(!validateJsonValue(invalid, schema).valid(),
                     QStringLiteral("enum and numeric ranges must be enforced"));

        auto unknownKeyword = JsonSchema::object();
        unknownKeyword.insert(QStringLiteral("contains"), QJsonObject{});
        ok &= expect(!checkJsonSchema(unknownKeyword).valid(),
                     QStringLiteral("unknown validation keywords must fail closed"));
        const auto referencedUnknownKeyword =
            JsonSchema::document(JsonSchema::reference(QStringLiteral("#/$defs/Bad")),
                                 QJsonObject{
                                     {QStringLiteral("Bad"), unknownKeyword}
        });
        ok &= expect(!checkJsonSchema(referencedUnknownKeyword).valid(),
                     QStringLiteral("unknown keywords in referenced schemas must fail closed"));
        ok &= expect(
            !checkJsonSchema(JsonSchema::reference(QStringLiteral("#/$defs/Missing"))).valid(),
            QStringLiteral("unresolved refs must fail closed"));
        const auto nonCanonicalArrayReference = JsonSchema::document(JsonSchema::oneOf(
            QJsonArray{JsonSchema::reference(QStringLiteral("#/oneOf/00")), false}));
        ok &=
            expect(!checkJsonSchema(nonCanonicalArrayReference).valid(),
                   QStringLiteral("JSON Pointer array indices must use canonical decimal syntax"));
        auto annotationReference =
            JsonSchema::document(JsonSchema::reference(QStringLiteral("#/default")));
        annotationReference.insert(QStringLiteral("default"),
                                   QJsonObject{
                                       {QStringLiteral("contains"), QJsonObject{}}
        });
        ok &= expect(!checkJsonSchema(annotationReference).valid(),
                     QStringLiteral("refs must not reinterpret annotation values as schemas"));

        auto containerReference = JsonSchema::document(JsonSchema::object({
            {QStringLiteral("name"), JsonSchema::string()}
        }));
        containerReference.insert(QStringLiteral("$ref"), QStringLiteral("#/properties"));
        ok &= expect(!checkJsonSchema(containerReference).valid(),
                     QStringLiteral("refs must point to schema nodes, not schema containers"));

        const auto cyclicReference = JsonSchema::document(
            JsonSchema::reference(QStringLiteral("#/$defs/Loop")),
            QJsonObject{
                {QStringLiteral("Loop"), JsonSchema::reference(QStringLiteral("#/$defs/Loop"))}
        });
        const auto cyclicResult = validateJsonValue(QJsonObject{}, cyclicReference);
        ok &= expect(checkJsonSchema(cyclicReference).valid() && !cyclicResult.valid() &&
                         cyclicResult.issues.first().code == SchemaIssueCode::LimitExceeded,
                     QStringLiteral(
                         "non-consuming cyclic refs must fail closed without unbounded recursion"));

        const auto unconstrainedInteger = JsonSchema::document(JsonSchema::integer());
        ok &= expect(
            validateJsonValue(QJsonValue(MaximumSafeJsonInteger), unconstrainedInteger).valid() &&
                validateJsonValue(QJsonValue(-MaximumSafeJsonInteger), unconstrainedInteger)
                    .valid() &&
                !validateJsonValue(QJsonValue(MaximumSafeJsonInteger + 1), unconstrainedInteger)
                     .valid() &&
                !validateJsonValue(QJsonValue(-MaximumSafeJsonInteger - 1), unconstrainedInteger)
                     .valid(),
            QStringLiteral("the generic integer validator must enforce the JSON safe range"));

        auto safeCardinality = JsonSchema::array(true);
        safeCardinality.insert(QStringLiteral("maxItems"), MaximumSafeJsonInteger);
        auto unsafeCardinality = safeCardinality;
        unsafeCardinality.insert(QStringLiteral("maxItems"), MaximumSafeJsonInteger + 1);
        ok &= expect(
            checkJsonSchema(safeCardinality).valid() && !checkJsonSchema(unsafeCardinality).valid(),
            QStringLiteral("Schema cardinalities must be safely representable qint64 values"));

        JsonResourceLimits limits;
        ok &= expect(!checkJsonResourceLimits(QJsonValue(QJsonValue::Undefined), limits).valid(),
                     QStringLiteral("undefined values must not pass as JSON"));
        limits.maximumStringCodeUnits = 3;
        ok &= expect(
            checkJsonResourceLimits(QStringLiteral("abc"), limits).valid() &&
                !checkJsonResourceLimits(QStringLiteral("abcd"), limits).valid() &&
                !checkJsonResourceLimits(
                     QJsonObject{
                         {QStringLiteral("abcd"), true}
        },
                     limits)
                     .valid(),
            QStringLiteral("central JSON resource limits must bound strings and object keys"));
        limits = {};
        limits.maximumArrayItems = 2;
        ok &= expect(checkJsonResourceLimits(QJsonArray{1, 2}, limits).valid() &&
                         !checkJsonResourceLimits(QJsonArray{1, 2, 3}, limits).valid(),
                     QStringLiteral("central JSON resource limits must bound arrays"));
        limits = {};
        limits.maximumObjectProperties = 2;
        ok &= expect(checkJsonResourceLimits(
                         QJsonObject{
                             {QStringLiteral("a"), 1},
                             {QStringLiteral("b"), 2}
        },
                         limits)
                             .valid() &&
                         !checkJsonResourceLimits(QJsonObject{{QStringLiteral("a"), 1},
                                                              {QStringLiteral("b"), 2},
                                                              {QStringLiteral("c"), 3}},
                                                  limits)
                              .valid(),
                     QStringLiteral("central JSON resource limits must bound objects"));
        limits = {};
        limits.maximumNodes = 3;
        ok &= expect(checkJsonResourceLimits(QJsonArray{1, 2}, limits).valid() &&
                         !checkJsonResourceLimits(QJsonArray{1, 2, 3}, limits).valid(),
                     QStringLiteral("central JSON resource limits must bound aggregate nodes"));
        limits = {};
        limits.maximumDepth = 2;
        ok &= expect(checkJsonResourceLimits(
                         QJsonObject{
                             {QStringLiteral("a"), QJsonObject{{QStringLiteral("b"), 1}}}
        },
                         limits)
                             .valid() &&
                         !checkJsonResourceLimits(
                              QJsonObject{{QStringLiteral("a"),
                                           QJsonObject{{QStringLiteral("b"),
                                                        QJsonObject{{QStringLiteral("c"), 1}}}}}},
                              limits)
                              .valid(),
                     QStringLiteral("central JSON resource limits must bound nesting depth"));
        return ok;
    }

    bool testSchemaCompatibility() {
        bool ok = true;
        const auto narrow = JsonSchema::document(JsonSchema::object(
            {
                {QStringLiteral("name"),
                 JsonSchema::string({QStringLiteral("alpha"), QStringLiteral("beta")})}
        },
            {QStringLiteral("name")}));
        const auto broad = JsonSchema::document(JsonSchema::object(
            {
                {QStringLiteral("name"), JsonSchema::string()}
        },
            {QStringLiteral("name")}));
        ok &= expect(proveSchemaSubset(narrow, broad).compatible(),
                     QStringLiteral("a narrower schema must prove as a subset"));
        ok &= expect(!proveSchemaSubset(broad, narrow).compatible(),
                     QStringLiteral("an unconstrained value must not satisfy a target enum"));

        const auto extraProperty = JsonSchema::document(JsonSchema::object(
            {
                {QStringLiteral("name"),  JsonSchema::string() },
                {QStringLiteral("extra"), JsonSchema::boolean()},
        },
            {QStringLiteral("name")}));
        ok &= expect(!proveSchemaSubset(extraProperty, broad).compatible(),
                     QStringLiteral("a source property rejected by target must be incompatible"));

        const auto ambiguousOneOf = JsonSchema::document(JsonSchema::oneOf(QJsonArray{
            JsonSchema::string({}, 1),
            JsonSchema::string({}, {}, 10),
        }));
        ok &= expect(!proveSchemaSubset(broad, ambiguousOneOf).compatible(),
                     QStringLiteral("unprovable oneOf coverage must fail closed"));

        const auto toolCompatibility = checkToolSchemaCompatibility(narrow, broad, narrow, broad);
        ok &= expect(toolCompatibility.compatible(),
                     QStringLiteral("tool compatibility must apply input and output directions"));

        QJsonObject sourceLower{
            {QStringLiteral("type"),    QStringLiteral("number")},
            {QStringLiteral("minimum"), 5                       }
        };
        QJsonObject targetLower{
            {QStringLiteral("type"),             QStringLiteral("number")},
            {QStringLiteral("minimum"),          10                      },
            {QStringLiteral("exclusiveMinimum"), 0                       }
        };
        ok &= expect(!proveSchemaSubset(sourceLower, targetLower).compatible(),
                     QStringLiteral("minimum must dominate a weaker exclusiveMinimum"));
        sourceLower.insert(QStringLiteral("minimum"), 10);
        sourceLower.insert(QStringLiteral("exclusiveMinimum"), 0);
        targetLower = {
            {QStringLiteral("type"),    QStringLiteral("number")},
            {QStringLiteral("minimum"), 5                       }
        };
        ok &= expect(
            proveSchemaSubset(sourceLower, targetLower).compatible(),
            QStringLiteral("source lower-bound normalization must retain its strictest bound"));

        QJsonObject sourceUpper{
            {QStringLiteral("type"),    QStringLiteral("number")},
            {QStringLiteral("maximum"), 15                      }
        };
        QJsonObject targetUpper{
            {QStringLiteral("type"),             QStringLiteral("number")},
            {QStringLiteral("maximum"),          10                      },
            {QStringLiteral("exclusiveMaximum"), 20                      }
        };
        ok &= expect(!proveSchemaSubset(sourceUpper, targetUpper).compatible(),
                     QStringLiteral("maximum must dominate a weaker exclusiveMaximum"));
        sourceUpper.insert(QStringLiteral("maximum"), 10);
        sourceUpper.insert(QStringLiteral("exclusiveMaximum"), 20);
        targetUpper = {
            {QStringLiteral("type"),    QStringLiteral("number")},
            {QStringLiteral("maximum"), 15                      }
        };
        ok &= expect(
            proveSchemaSubset(sourceUpper, targetUpper).compatible(),
            QStringLiteral("source upper-bound normalization must retain its strictest bound"));
        return ok;
    }

    bool testPublicContract() {
        bool ok = true;
        const auto &tools = publicToolContracts();
        const auto expectedIds = PublicAutomationToolsetExpectations::editorToolIds();
        ok &= expect(tools.size() == 179,
                     QStringLiteral("public declaration must contain 179 editor tools"));
        ok &=
            expect(publicToolIds() == expectedIds,
                   QStringLiteral("public declaration must exactly match the frozen tool matrix"));

        QSet<QString> ids;
        qsizetype dynamicSourceCount = 0;
        qsizetype openSchemaCount = 0;
        const QStringList descriptorFields{
            QStringLiteral("operation_id"),
            QStringLiteral("version"),
            QStringLiteral("minimum_compatible_version"),
            QStringLiteral("category"),
            QStringLiteral("kind"),
            QStringLiteral("input_schema"),
            QStringLiteral("output_schema"),
            QStringLiteral("value_sources"),
            QStringLiteral("minimum_profile"),
            QStringLiteral("sync_mode"),
            QStringLiteral("document_policy"),
            QStringLiteral("revision_policy"),
            QStringLiteral("history_policy"),
            QStringLiteral("file_access"),
            QStringLiteral("host_availability"),
            QStringLiteral("concurrency_scope"),
            QStringLiteral("conflict_policy"),
            QStringLiteral("safety_metadata"),
            QStringLiteral("introduced_version"),
            QStringLiteral("schema_digest"),
        };
        const QStringList legacyDescriptorFields{
            QStringLiteral("operationId"),      QStringLiteral("minimumCompatibleVersion"),
            QStringLiteral("inputSchema"),      QStringLiteral("outputSchema"),
            QStringLiteral("valueSources"),     QStringLiteral("minimumProfile"),
            QStringLiteral("syncMode"),         QStringLiteral("documentPolicy"),
            QStringLiteral("revisionPolicy"),   QStringLiteral("historyPolicy"),
            QStringLiteral("fileAccess"),       QStringLiteral("hostAvailability"),
            QStringLiteral("concurrencyScope"), QStringLiteral("conflictPolicy"),
            QStringLiteral("safetyMetadata"),   QStringLiteral("introducedVersion"),
            QStringLiteral("schemaDigest"),
        };
        for (const auto &tool : tools) {
            ids.insert(tool.operationId);
            ok &= expect(tool.minimumProfile != AutomationProfile::Custom,
                         QStringLiteral("public tools must use a preset minimum profile"));
            const auto inputCheck = checkJsonSchema(tool.inputSchema);
            const auto outputCheck = checkJsonSchema(tool.outputSchema);
            const auto firstIssue = !inputCheck.valid()    ? inputCheck.issues.first()
                                    : !outputCheck.valid() ? outputCheck.issues.first()
                                                           : SchemaIssue{};
            ok &= expect(inputCheck.valid() && outputCheck.valid(),
                         QStringLiteral("every public tool schema must be supported: %1 (%2: %3)")
                             .arg(tool.operationId, firstIssue.schemaPath, firstIssue.message));
            ok &= expect(hasStrictObjectRoot(tool.inputSchema) &&
                             hasStrictObjectRoot(tool.outputSchema),
                         QStringLiteral("public root object schemas must reject unknown fields: %1")
                             .arg(tool.operationId));

            QStringList inputOpenSchemas;
            QStringList outputOpenSchemas;
            collectOpenSchemaPaths(tool.inputSchema, {}, inputOpenSchemas);
            collectOpenSchemaPaths(tool.outputSchema, {}, outputOpenSchemas);
            for (const auto &path : inputOpenSchemas) {
                ++openSchemaCount;
                ok &= expect(tool.operationId == QStringLiteral("automation.get_options") &&
                                 path == QStringLiteral(
                                             "/properties/partial_arguments/additionalProperties"),
                             QStringLiteral("unexpected open input schema: %1%2")
                                 .arg(tool.operationId, path));
            }
            for (const auto &path : outputOpenSchemas) {
                ++openSchemaCount;
                ok &= expect(tool.operationId == QStringLiteral("automation.get_manifest") &&
                                 path ==
                                     QStringLiteral("/properties/extensions/additionalProperties"),
                             QStringLiteral("unexpected open output schema: %1%2")
                                 .arg(tool.operationId, path));
            }

            const auto descriptor = tool.toManifestJson();
            for (const auto &field : descriptorFields) {
                ok &= expect(descriptor.contains(field),
                             QStringLiteral("public descriptor field is required: %1.%2")
                                 .arg(tool.operationId, field));
            }
            for (const auto &field : legacyDescriptorFields) {
                ok &= expect(!descriptor.contains(field),
                             QStringLiteral("Manifest descriptor must not expose camelCase: %1.%2")
                                 .arg(tool.operationId, field));
            }
            ok &= expect(
                tool.version == 1 && tool.introducedVersion == 1 &&
                    tool.minimumCompatibleVersion == 1 &&
                    descriptor.value(QStringLiteral("version")).toInteger() == 1 &&
                    descriptor.value(QStringLiteral("introduced_version")).toInteger() == 1 &&
                    descriptor.value(QStringLiteral("minimum_compatible_version")).toInteger() == 1,
                QStringLiteral("all three per-tool version fields must start at one"));
            const auto mcpMetadata = tool.toMcpToolJson()
                                         .value(QStringLiteral("_meta"))
                                         .toObject()
                                         .value(QStringLiteral("io.openvpi.ds-editor-lite/tool"))
                                         .toObject();
            ok &= expect(
                mcpMetadata.value(QStringLiteral("kind")) ==
                        descriptor.value(QStringLiteral("kind")) &&
                    mcpMetadata.value(QStringLiteral("host_availability")) ==
                        descriptor.value(QStringLiteral("host_availability")),
                QStringLiteral("MCP tool metadata must preserve Manifest execution semantics: %1")
                    .arg(tool.operationId));
            for (const auto &entry : tool.valueSources) {
                ++dynamicSourceCount;
                const auto sourceDescriptor = entry.toObject();
                const auto sourceId =
                    sourceDescriptor.value(QStringLiteral("operation_id")).toString();
                const auto *source = findPublicTool(sourceId);
                const auto declaredProfile = automationProfileFromName(
                    sourceDescriptor.value(QStringLiteral("minimum_profile")).toString());
                ok &= expect(
                    source && source->kind == OperationKind::Query && declaredProfile &&
                        *declaredProfile == source->minimumProfile &&
                        presetIncludes(tool.minimumProfile, source->minimumProfile) &&
                        !sourceDescriptor.value(QStringLiteral("field_path")).toString().isEmpty(),
                    QStringLiteral("valueSource must reference an accessible query: %1 -> %2")
                        .arg(tool.operationId, sourceId));
            }
        }
        ok &= expect(ids.size() == tools.size(),
                     QStringLiteral("public operation IDs must be unique"));
        ok &= expect(openSchemaCount == 2,
                     QStringLiteral("only controlled partial arguments and Manifest extensions may "
                                    "use open nested schemas"));
        ok &= expect(documentLifecycleValues() == QStringList{QStringLiteral("active"),
                                                              QStringLiteral("replacing"),
                                                              QStringLiteral("closing")} &&
                         documentLifecycleFromName(QStringLiteral("replacing")) ==
                             DocumentLifecycle::Replacing &&
                         publicObjectKindValues().size() == 8 &&
                         publicObjectKindName(PublicObjectKind::Anchor) == QStringLiteral("anchor"),
                     QStringLiteral("public lifecycle and object-kind codecs must use generated "
                                    "stable strings"));
        ok &= expect(
            dynamicSourceCount > 0 &&
                !findPublicTool(QStringLiteral("tracks.set_default_language"))
                     ->valueSources.isEmpty() &&
                !findPublicTool(QStringLiteral("exports.audio.start"))->valueSources.isEmpty() &&
                !findPublicTool(QStringLiteral("inference.start"))->valueSources.isEmpty(),
            QStringLiteral("controlled dynamic fields must publish discoverable value sources"));
        ok &= expect(toolsForProfile(AutomationProfile::Meta).size() == 4 &&
                         toolsForProfile(AutomationProfile::L1).size() == 91 &&
                         toolsForProfile(AutomationProfile::L2).size() == 134 &&
                         toolsForProfile(AutomationProfile::L3).size() == 179,
                     QStringLiteral("public preset counts must be 4/91/134/179"));
        ok &= expect(
            toolsForProfile(AutomationProfile::Custom, {QStringLiteral("notes.insert")}).size() ==
                5,
            QStringLiteral("Custom must retain Meta and explicit enabled tools"));

        const auto manifest = buildPublicManifest(AutomationProfile::L1);
        const auto page =
            buildPublicManifest(AutomationProfile::L1, {}, QStringLiteral("gui"), 0, 7);
        ok &= expect(manifest.toolsetVersion == 1 && manifest.operations.size() == 91 &&
                         !manifest.digest.isEmpty() && page.operations.size() == 7 &&
                         page.digest == manifest.digest && !page.nextCursor.isEmpty(),
                     QStringLiteral("public Manifest must be versioned, digested and pageable"));
        const auto manifestJson = manifest.toJson();
        ok &= expect(
            manifestJson.contains(QStringLiteral("toolset_version")) &&
                manifestJson.contains(QStringLiteral("profile")) &&
                manifestJson.contains(QStringLiteral("host_mode")) &&
                !manifestJson.contains(QStringLiteral("toolsetVersion")) &&
                validateJsonValue(
                    manifestJson,
                    findPublicTool(QStringLiteral("automation.get_manifest"))->outputSchema)
                    .valid(),
            QStringLiteral("PublicManifest wire codec must use the strict snake_case envelope"));

        const auto revisionProperty = findPublicTool(QStringLiteral("history.undo"))
                                          ->inputSchema.value(QStringLiteral("properties"))
                                          .toObject()
                                          .value(QStringLiteral("expected_revision"));
        ok &= expect(
            validateJsonValue(QJsonValue(MaximumSafeJsonInteger), revisionProperty).valid() &&
                !validateJsonValue(QJsonValue(MaximumSafeJsonInteger + 1), revisionProperty)
                     .valid(),
            QStringLiteral("revision JSON numbers must stay inside the exact integer range"));

        const auto colorProperty = findPublicTool(QStringLiteral("tracks.set_color"))
                                       ->inputSchema.value(QStringLiteral("properties"))
                                       .toObject()
                                       .value(QStringLiteral("color_index"));
        ok &=
            expect(validateJsonValue(TrackPaletteColorCount - 1, colorProperty).valid() &&
                       !validateJsonValue(TrackPaletteColorCount, colorProperty).valid(),
                   QStringLiteral("track color Schema must derive its upper bound from the shared "
                                  "palette constant"));

        const auto propertiesFor = [](const QString &id) {
            return findPublicTool(id)->inputSchema.value(QStringLiteral("properties")).toObject();
        };
        const auto *setLoop = findPublicTool(QStringLiteral("playback.set_loop"));
        const auto *seek = findPublicTool(QStringLiteral("playback.seek"));
        QSet<QString> setLoopRequired;
        for (const auto &field : setLoop->inputSchema.value(QStringLiteral("required")).toArray())
            setLoopRequired.insert(field.toString());
        const auto setLoopProperties = propertiesFor(QStringLiteral("playback.set_loop"));
        const auto setLoopDescriptor = setLoop->toManifestJson();
        QJsonObject integerLoop{
            {QStringLiteral("document_id"),       QStringLiteral("00000000-0000-4000-8000-000000000001")},
            {QStringLiteral("expected_revision"), 0                                                     },
            {QStringLiteral("start"),             120                                                   },
            {QStringLiteral("end"),               960                                                   },
        };
        auto fractionalLoop = integerLoop;
        fractionalLoop.insert(QStringLiteral("end"), 960.5);
        ok &=
            expect(setLoop && seek && setLoopRequired.contains(QStringLiteral("document_id")) &&
                       setLoopRequired.contains(QStringLiteral("expected_revision")) &&
                       !setLoopRequired.contains(QStringLiteral("expected_state_version")) &&
                       setLoopProperties.contains(QStringLiteral("expected_state_version")) &&
                       setLoopProperties.contains(QStringLiteral("idempotency_key")) &&
                       validateJsonValue(integerLoop, setLoop->inputSchema).valid() &&
                       !validateJsonValue(fractionalLoop, setLoop->inputSchema).valid() &&
                       setLoop->outputSchema.value(QStringLiteral("properties"))
                           .toObject()
                           .contains(QStringLiteral("previous")) &&
                       setLoop->outputSchema.value(QStringLiteral("properties"))
                           .toObject()
                           .contains(QStringLiteral("playback")) &&
                       setLoopDescriptor.value(QStringLiteral("document_policy")) ==
                           QStringLiteral("write") &&
                       setLoopDescriptor.value(QStringLiteral("history_policy")) ==
                           QStringLiteral("single_entry") &&
                       !propertiesFor(QStringLiteral("playback.seek"))
                            .contains(QStringLiteral("expected_revision")),
                   QStringLiteral(
                       "persistent loop edits and transient seek must publish distinct contracts"));
        const auto replaceProperties = propertiesFor(QStringLiteral("parameters.replace"));
        const auto drawProperties = propertiesFor(QStringLiteral("parameters.draw"));
        const auto insertAnchorProperties =
            propertiesFor(QStringLiteral("parameters.insert_anchors"));
        const auto bakeProperties = propertiesFor(QStringLiteral("parameters.bake"));
        ok &= expect(!replaceProperties.contains(QStringLiteral("curve_id")) &&
                         !replaceProperties.contains(QStringLiteral("local_start")) &&
                         !replaceProperties.contains(QStringLiteral("local_end")) &&
                         !replaceProperties.contains(QStringLiteral("merge_mode")) &&
                         drawProperties.contains(QStringLiteral("merge_mode")) &&
                         !drawProperties.contains(QStringLiteral("curve_id")) &&
                         !drawProperties.contains(QStringLiteral("local_end")) &&
                         insertAnchorProperties.contains(QStringLiteral("curve_id")) &&
                         !insertAnchorProperties.contains(QStringLiteral("merge_mode")) &&
                         bakeProperties.contains(QStringLiteral("local_start")) &&
                         bakeProperties.contains(QStringLiteral("local_end")) &&
                         !bakeProperties.contains(QStringLiteral("curve_id")) &&
                         !bakeProperties.contains(QStringLiteral("merge_mode")),
                     QStringLiteral("parameter tools must expose only their operation-specific "
                                    "optional fields"));

        const auto audioImportProperties = propertiesFor(QStringLiteral("audio_clips.import"));
        ok &= expect(audioImportProperties.contains(QStringLiteral("track_id")) &&
                         audioImportProperties.contains(QStringLiteral("start")) &&
                         audioImportProperties.contains(QStringLiteral("path")) &&
                         audioImportProperties.contains(QStringLiteral("name")) &&
                         audioImportProperties.contains(QStringLiteral("gain")) &&
                         audioImportProperties.contains(QStringLiteral("mute")) &&
                         !audioImportProperties.contains(QStringLiteral("properties")) &&
                         !audioImportProperties.contains(QStringLiteral("client_ref")),
                     QStringLiteral("single audio import must expose a flat, typed clip draft"));

        const auto inferenceScope =
            propertiesFor(QStringLiteral("inference.start")).value(QStringLiteral("scope"));
        ok &= expect(
            validateJsonValue(
                QJsonObject{
                    {QStringLiteral("kind"), QStringLiteral("document")}
        },
                inferenceScope)
                    .valid() &&
                !validateJsonValue(QJsonObject{{QStringLiteral("kind"), QStringLiteral("document")},
                                               {QStringLiteral("track_ids"), QJsonArray{1}}},
                                   inferenceScope)
                     .valid() &&
                validateJsonValue(QJsonObject{{QStringLiteral("kind"), QStringLiteral("track")},
                                              {QStringLiteral("track_ids"), QJsonArray{1}}},
                                  inferenceScope)
                    .valid() &&
                !validateJsonValue(QJsonObject{{QStringLiteral("kind"), QStringLiteral("track")}},
                                   inferenceScope)
                     .valid(),
            QStringLiteral("InferenceScope kind must determine the required exclusive ID "
                           "collection"));

        const auto *audioPreview = findPublicTool(QStringLiteral("exports.audio.preview"));
        const QJsonObject commonAudioOptions{
            {QStringLiteral("format"),       QStringLiteral("wav")   },
            {QStringLiteral("sample_rate"),  44100                   },
            {QStringLiteral("channel_mode"), QStringLiteral("stereo")},
            {QStringLiteral("mixing_mode"),  QStringLiteral("mixed") },
        };
        const auto audioArguments = [&](QJsonObject options) {
            return QJsonObject{
                {QStringLiteral("document_id"),
                 QStringLiteral("00000000-0000-4000-8000-000000000001")     },
                {QStringLiteral("path"),        QStringLiteral("export.wav")},
                {QStringLiteral("options"),     options                     },
            };
        };
        auto allAudioOptions = commonAudioOptions;
        allAudioOptions.insert(QStringLiteral("source"), QStringLiteral("all"));
        auto allWithIds = allAudioOptions;
        allWithIds.insert(QStringLiteral("source_ids"), QJsonArray{1});
        auto customAudioOptions = commonAudioOptions;
        customAudioOptions.insert(QStringLiteral("source"), QStringLiteral("custom"));
        auto customWithIds = customAudioOptions;
        customWithIds.insert(QStringLiteral("source_ids"), QJsonArray{1});
        auto audioWithRange = allAudioOptions;
        audioWithRange.insert(QStringLiteral("range"),
                              QJsonObject{
                                  {QStringLiteral("start"), 0  },
                                  {QStringLiteral("end"),   480}
        });
        ok &= expect(
            audioPreview &&
                validateJsonValue(audioArguments(allAudioOptions), audioPreview->inputSchema)
                    .valid() &&
                !validateJsonValue(audioArguments(allWithIds), audioPreview->inputSchema).valid() &&
                !validateJsonValue(audioArguments(customAudioOptions), audioPreview->inputSchema)
                     .valid() &&
                validateJsonValue(audioArguments(customWithIds), audioPreview->inputSchema)
                    .valid() &&
                !validateJsonValue(audioArguments(audioWithRange), audioPreview->inputSchema)
                     .valid(),
            QStringLiteral("audio export source mode must conditionally require source_ids and "
                           "reject unsupported ranges"));

        const auto *midiExtraction = findPublicTool(QStringLiteral("extract.midi.start"));
        const auto midiArguments = [](const QJsonObject &destination) {
            return QJsonObject{
                {QStringLiteral("document_id"),
                 QStringLiteral("00000000-0000-4000-8000-000000000001")},
                {QStringLiteral("expected_revision"),    0             },
                {QStringLiteral("source_audio_clip_id"), 1             },
                {QStringLiteral("destination"),          destination   },
                {QStringLiteral("options"),              QJsonObject{} },
            };
        };
        const QJsonObject createDestination{
            {QStringLiteral("target_track_id"), 1                            },
            {QStringLiteral("start"),           0                            },
            {QStringLiteral("mode"),            QStringLiteral("create_clip")},
        };
        auto createWithTarget = createDestination;
        createWithTarget.insert(QStringLiteral("target_clip_id"), 2);
        QJsonObject mergeDestination{
            {QStringLiteral("target_track_id"), 1                                },
            {QStringLiteral("start"),           0                                },
            {QStringLiteral("mode"),            QStringLiteral("merge_into_clip")},
            {QStringLiteral("target_clip_id"),  2                                },
        };
        auto mergeWithoutTarget = mergeDestination;
        mergeWithoutTarget.remove(QStringLiteral("target_clip_id"));
        ok &= expect(
            midiExtraction &&
                validateJsonValue(midiArguments(createDestination), midiExtraction->inputSchema)
                    .valid() &&
                !validateJsonValue(midiArguments(createWithTarget), midiExtraction->inputSchema)
                     .valid() &&
                validateJsonValue(midiArguments(mergeDestination), midiExtraction->inputSchema)
                    .valid() &&
                !validateJsonValue(midiArguments(mergeWithoutTarget), midiExtraction->inputSchema)
                     .valid(),
            QStringLiteral("MIDI extraction destination discriminator must forbid extra and "
                           "missing target_clip_id values"));

        const auto midiOverwrite = propertiesFor(QStringLiteral("exports.midi.start"))
                                       .value(QStringLiteral("overwrite_policy"))
                                       .toObject();
        const auto audioOverwrite = propertiesFor(QStringLiteral("exports.audio.start"))
                                        .value(QStringLiteral("overwrite_policy"))
                                        .toObject();
        const auto requiredFor = [](const QString &id) {
            QSet<QString> required;
            for (const auto &field :
                 findPublicTool(id)->inputSchema.value(QStringLiteral("required")).toArray()) {
                required.insert(field.toString());
            }
            return required;
        };
        ok &= expect(midiOverwrite.value(QStringLiteral("type")) == QStringLiteral("string") &&
                         audioOverwrite.value(QStringLiteral("type")) == QStringLiteral("string") &&
                         requiredFor(QStringLiteral("exports.midi.start"))
                             .contains(QStringLiteral("overwrite_policy")) &&
                         requiredFor(QStringLiteral("exports.audio.start"))
                             .contains(QStringLiteral("overwrite_policy")),
                     QStringLiteral("file exports must require one typed overwrite policy"));

        const auto exportDescriptor =
            findPublicTool(QStringLiteral("exports.audio.start"))->toManifestJson();
        const auto taskDescriptor =
            findPublicTool(QStringLiteral("tasks.cancel"))->toManifestJson();
        const auto metaDescriptor =
            findPublicTool(QStringLiteral("application.get_info"))->toManifestJson();
        ok &= expect(
            exportDescriptor.value(QStringLiteral("document_policy")) == QStringLiteral("read") &&
                exportDescriptor.value(QStringLiteral("revision_policy")) ==
                    QStringLiteral("check_and_revalidate") &&
                exportDescriptor.value(QStringLiteral("history_policy")) ==
                    QStringLiteral("none") &&
                exportDescriptor.value(QStringLiteral("file_access")) == QStringLiteral("write") &&
                taskDescriptor.value(QStringLiteral("document_policy")) == QStringLiteral("read") &&
                taskDescriptor.value(QStringLiteral("revision_policy")) == QStringLiteral("none") &&
                taskDescriptor.value(QStringLiteral("concurrency_scope")) ==
                    QStringLiteral("task") &&
                metaDescriptor.value(QStringLiteral("document_policy")) == QStringLiteral("none") &&
                metaDescriptor.value(QStringLiteral("concurrency_scope")) ==
                    QStringLiteral("application"),
            QStringLiteral("Meta, export, and task descriptor policy exceptions must be explicit"));

        const auto midiExportProperties = propertiesFor(QStringLiteral("exports.midi.start"));
        const auto audioExportProperties = propertiesFor(QStringLiteral("exports.audio.start"));
        for (const auto &properties : {midiExportProperties, audioExportProperties}) {
            ok &= expect(properties.contains(QStringLiteral("document_id")) &&
                             !properties.contains(QStringLiteral("expected_revision")) &&
                             !properties.contains(QStringLiteral("validate_only")) &&
                             !properties.contains(QStringLiteral("idempotency_key")),
                         QStringLiteral("exports must use document-query context without editing "
                                        "revision or History fields"));
        }

        int asynchronousCount = 0;
        const QJsonObject documentVersion{
            {QStringLiteral("document_id"), QStringLiteral("00000000-0000-4000-8000-000000000001")},
            {QStringLiteral("revision"),    0                                                     },
        };
        for (const auto &tool : tools) {
            if (tool.syncMode != SyncMode::Asynchronous)
                continue;
            ++asynchronousCount;
            const bool applicationScoped = tool.operationId == QStringLiteral("packages.refresh");
            const QJsonObject accepted{
                {QStringLiteral("task_id"),        QStringLiteral("00000000-0000-4000-8000-000000000002")},
                {QStringLiteral("scope"),
                 applicationScoped ? QStringLiteral("application") : QStringLiteral("document")          },
                {QStringLiteral("document"),
                 applicationScoped ? QJsonValue(QJsonValue::Null) : QJsonValue(documentVersion)          },
                {QStringLiteral("validated_only"), false                                                 },
            };
            const QJsonObject missingTask{
                {QStringLiteral("scope"),
                 applicationScoped ? QStringLiteral("application") : QStringLiteral("document")},
                {QStringLiteral("document"),
                 applicationScoped ? QJsonValue(QJsonValue::Null) : QJsonValue(documentVersion)},
                {QStringLiteral("validated_only"), false                                       },
            };
            const QJsonObject missingDocument{
                {QStringLiteral("task_id"),        QStringLiteral("00000000-0000-4000-8000-000000000002")},
                {QStringLiteral("scope"),
                 applicationScoped ? QStringLiteral("application") : QStringLiteral("document")          },
                {QStringLiteral("validated_only"), false                                                 },
            };
            ok &= expect(validateJsonValue(accepted, tool.outputSchema).valid() &&
                             !validateJsonValue(missingTask, tool.outputSchema).valid() &&
                             !validateJsonValue(missingDocument, tool.outputSchema).valid(),
                         QStringLiteral("TaskAccepted must require task_id and document for %1")
                             .arg(tool.operationId));
        }
        ok &= expect(
            asynchronousCount == 11,
            QStringLiteral("all eleven asynchronous public tools must share the discriminated "
                           "TaskAccepted schema"));

        const auto parameterSourcePaths = [](const QString &id) {
            QSet<QString> paths;
            for (const auto &source : findPublicTool(id)->valueSources)
                paths.insert(source.toObject().value(QStringLiteral("field_path")).toString());
            return paths;
        };
        ok &= expect(
            parameterSourcePaths(QStringLiteral("parameters.replace")) ==
                    QSet<QString>{QStringLiteral("/name"), QStringLiteral("/layer"),
                                  QStringLiteral("/curves/*/type"),
                                  QStringLiteral("/curves/*/nodes/*/interpolation"),
                                  QStringLiteral("/curves/*/values/*"),
                                  QStringLiteral("/curves/*/nodes/*/value")} &&
                parameterSourcePaths(QStringLiteral("parameters.draw")) ==
                    QSet<QString>{QStringLiteral("/name"), QStringLiteral("/layer"),
                                  QStringLiteral("/values/*")} &&
                parameterSourcePaths(QStringLiteral("parameters.set_anchor_interpolation")) ==
                    QSet<QString>{QStringLiteral("/name"), QStringLiteral("/layer"),
                                  QStringLiteral("/interpolation")} &&
                parameterSourcePaths(QStringLiteral("parameters.bake")) ==
                    QSet<QString>{QStringLiteral("/name")},
            QStringLiteral("parameter value sources must map only capability-backed enum "
                           "fields"));
        ok &= expect(
            parameterSourcePaths(QStringLiteral("notes.fill_lyrics"))
                .contains(QStringLiteral("/options/language/language_id")),
            QStringLiteral("fill-lyrics explicit language must publish a dynamic value source"));

        const auto mcpTool = tools.first().toMcpToolJson();
        ok &= expect(
            mcpTool.contains(QStringLiteral("inputSchema")) &&
                mcpTool.contains(QStringLiteral("outputSchema")) &&
                !mcpTool.contains(QStringLiteral("input_schema")) &&
                !mcpTool.contains(QStringLiteral("output_schema")),
            QStringLiteral("MCP tool descriptors must retain standard camelCase Schema keys"));
        return ok;
    }

    bool testExposurePolicy() {
        bool ok = true;
        ok &= expect(selectExposure({ExposureProfile::L0}).exposedIds.size() == 0 &&
                         selectExposure({ExposureProfile::L1}).exposedIds.size() == 91 &&
                         selectExposure({ExposureProfile::L2}).exposedIds.size() == 134 &&
                         selectExposure({ExposureProfile::L3}).exposedIds.size() == 179,
                     QStringLiteral("connector exposure preset counts must be 0/91/134/179"));

        const ExposureConfig filtered{
            .profile = ExposureProfile::L0,
            .includes = {QStringLiteral("category:notes"), QStringLiteral("missing.future")},
            .excludes = {QStringLiteral("prefix:notes.set_")},
        };
        const auto selection = selectExposure(filtered);
        ok &= expect(
            selection.valid() && selection.exposedIds.contains(QStringLiteral("notes.get")) &&
                !selection.exposedIds.contains(QStringLiteral("notes.set_lyric")) &&
                selection.pendingSelectors.contains(QStringLiteral("id:missing.future")),
            QStringLiteral("include/exclude/pending selectors must compose deterministically"));
        ok &= expect(
            !parseExposureSelector(QStringLiteral("prefix:notes.*")).valid() &&
                !parseExposureSelector(QStringLiteral("unknown:notes")).valid() &&
                parseExposureSelector(QStringLiteral("notes.insert")).selector->normalized() ==
                    QStringLiteral("id:notes.insert"),
            QStringLiteral(
                "selector grammar must reject glob/unknown prefixes and normalize bare IDs"));

        auto targets = publicExposureTargets();
        targets.append(
            {QStringLiteral("future.gui_tool"), QStringLiteral("future"), AutomationProfile::L3});
        ok &= expect(selectExposure({ExposureProfile::L2}, targets).exposedIds.size() == 134 &&
                         selectExposure({ExposureProfile::L3}, targets)
                             .exposedIds.contains(QStringLiteral("future.gui_tool")),
                     QStringLiteral("higher exposure presets must include higher-profile targets"));
        return ok;
    }

    bool testMcpProtocol() {
        using namespace AutomationWire::Mcp;
        bool ok = true;
        const RequestContext context{
            .clientCapabilities = {},
            .clientInfo = ImplementationInfo{QStringLiteral("test-client"), QStringLiteral("1")},
        };
        const auto request =
            makeRequest(QString::fromLatin1(ToolsCallMethod),
                        QJsonObject{
                            {QStringLiteral("name"),      QStringLiteral("notes.insert")},
                            {QStringLiteral("arguments"), QJsonObject{}                 }
        },
                        context, QStringLiteral("request-1"));
        const auto parsed = validateRequest(request);
        ok &= expect(parsed.valid() && parsed.request->name == QStringLiteral("notes.insert") &&
                         parsed.request->clientInfo.has_value(),
                     QStringLiteral("modern MCP request metadata must parse without initialize"));
        ok &= expect(isSupportedCoreMethod(QString::fromLatin1(InitializeMethod)) &&
                         isSupportedCoreMethod(QString::fromLatin1(DiscoverMethod)),
                     QStringLiteral("the shared core must expose both MCP lifecycle families"));

        const RequestContext legacyContext{
            .protocolVersion = QString::fromLatin1(LegacyProtocolVersion),
            .clientCapabilities = QJsonObject{},
            .clientInfo = ImplementationInfo{QStringLiteral("legacy-client"), QStringLiteral("1")},
        };
        const auto initialize = makeInitializeRequest(legacyContext, QStringLiteral("init-1"));
        const auto legacyInitialize = validateRequest(initialize);
        const auto legacyList = validateRequest(makeRequest(
            QString::fromLatin1(ToolsListMethod), {}, legacyContext, QStringLiteral("list-1")));
        ok &= expect(legacyInitialize.valid() && legacyList.valid() &&
                         legacyInitialize.request->protocolVersion ==
                             QString::fromLatin1(LegacyProtocolVersion) &&
                         legacyList.request->meta.isEmpty(),
                     QStringLiteral("MCP 2025-11-25 initialize and metadata-free requests must "
                                    "validate alongside 2026-07-28"));
        auto compatibilityContext = legacyContext;
        compatibilityContext.protocolVersion = QString::fromLatin1(CompatibilityProtocolVersion);
        const auto compatibilityInitialize = validateRequest(
            makeInitializeRequest(compatibilityContext, QStringLiteral("init-2025-06")));
        const auto compatibilityList =
            validateRequest(makeRequest(QString::fromLatin1(ToolsListMethod), {},
                                        compatibilityContext, QStringLiteral("list-2025-06")));
        ok &= expect(compatibilityInitialize.valid() && compatibilityList.valid() &&
                         compatibilityInitialize.request->protocolVersion ==
                             QString::fromLatin1(CompatibilityProtocolVersion) &&
                         isLegacyProtocolVersion(compatibilityList.request->protocolVersion),
                     QStringLiteral("MCP 2025-06-18 must share the legacy initialize lifecycle"));
        ok &= expect(
            !validateRequest(makeInitializeRequest(context, QStringLiteral("modern-init"))).valid(),
            QStringLiteral("MCP 2026-07-28 must reject the removed initialize handshake"));

        auto noClientInfoContext = context;
        noClientInfoContext.clientInfo.reset();
        ok &= expect(validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {},
                                                 noClientInfoContext, 1))
                         .valid(),
                     QStringLiteral("clientInfo must be optional"));
        const auto cancellation = validateRequest(QJsonObject{
            {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                       },
            {QStringLiteral("method"),  QStringLiteral("notifications/cancelled")   },
            {QStringLiteral("params"),
             QJsonObject{{QStringLiteral("requestId"), QStringLiteral("request-1")}}},
        });
        ok &= expect(cancellation.valid() && cancellation.request->notification &&
                         cancellation.request->meta.isEmpty() &&
                         cancellation.request->protocolVersion ==
                             QString::fromLatin1(LegacyProtocolVersion),
                     QStringLiteral("notification metadata must remain optional"));
        auto invalidRequest = request;
        auto invalidParams = invalidRequest.value(QStringLiteral("params")).toObject();
        auto invalidMeta = invalidParams.value(QStringLiteral("_meta")).toObject();
        invalidMeta.remove(QString::fromLatin1(ClientCapabilitiesMetaKey));
        invalidParams.insert(QStringLiteral("_meta"), invalidMeta);
        invalidRequest.insert(QStringLiteral("params"), invalidParams);
        ok &= expect(!validateRequest(invalidRequest).valid(),
                     QStringLiteral("clientCapabilities must be required per request"));

        const TransportMetadata metadata{
            .protocolVersion = QString::fromLatin1(ProtocolVersion),
            .method = QString::fromLatin1(ToolsCallMethod),
            .name = QStringLiteral("notes.insert"),
        };
        ok &= expect(validateTransportMetadata(metadata, *parsed.request).valid(),
                     QStringLiteral("matching MCP header metadata must validate"));
        auto mismatch = metadata;
        mismatch.name = QStringLiteral("notes.remove");
        const auto mismatchResult = validateTransportMetadata(mismatch, *parsed.request);
        ok &= expect(!mismatchResult.valid() &&
                         mismatchResult.error->code == ErrorCode::HeaderMismatch,
                     QStringLiteral("header/body mismatch must use HeaderMismatch"));

        auto unknownVersionContext = context;
        unknownVersionContext.protocolVersion = QStringLiteral("2099-01-01");
        const auto unknownVersionRequest =
            makeRequest(QString::fromLatin1(DiscoverMethod), {}, unknownVersionContext, 2);
        const auto unknownVersionNotification = validateRequest(
            makeRequest(QStringLiteral("notifications/cancelled"),
                        QJsonObject{
                            {QStringLiteral("requestId"), QStringLiteral("request-1")}
        },
                        unknownVersionContext));
        const auto structurallyParsed = parseRequest(unknownVersionRequest);
        const auto versionValidation = validateRequest(unknownVersionRequest);
        const auto mismatchedVersionMetadata = validateTransportMetadata(
            TransportMetadata{.protocolVersion = QString::fromLatin1(ProtocolVersion),
                              .method = QString::fromLatin1(DiscoverMethod)},
            *structurallyParsed.request);
        const auto matchingUnknownVersionMetadata = validateTransportMetadata(
            TransportMetadata{.protocolVersion = unknownVersionContext.protocolVersion,
                              .method = QString::fromLatin1(DiscoverMethod)},
            *structurallyParsed.request);
        const auto standaloneVersionData = versionValidation.error.data.toObject();
        const auto transportVersionData = matchingUnknownVersionMetadata.error->data.toObject();
        ok &= expect(structurallyParsed.valid() && !versionValidation.valid() &&
                         !unknownVersionNotification.valid() &&
                         unknownVersionNotification.error.code ==
                             ErrorCode::UnsupportedProtocolVersion &&
                         versionValidation.error.code == ErrorCode::UnsupportedProtocolVersion &&
                         standaloneVersionData.value(QStringLiteral("requested")) ==
                             unknownVersionContext.protocolVersion &&
                         standaloneVersionData.value(QStringLiteral("supported")).toArray() ==
                             QJsonArray{QString::fromLatin1(ProtocolVersion),
                                        QString::fromLatin1(LegacyProtocolVersion),
                                        QString::fromLatin1(CompatibilityProtocolVersion)} &&
                         !standaloneVersionData.contains(QStringLiteral("supportedVersions")) &&
                         !mismatchedVersionMetadata.valid() &&
                         mismatchedVersionMetadata.error->code == ErrorCode::HeaderMismatch &&
                         !matchingUnknownVersionMetadata.valid() &&
                         matchingUnknownVersionMetadata.error->code ==
                             ErrorCode::UnsupportedProtocolVersion &&
                         transportVersionData == standaloneVersionData,
                     QStringLiteral("unsupported protocol versions must be rejected for requests "
                                    "and notifications without weakening transport validation"));

        const auto unicode = QStringLiteral("Hello, 世界");
        QString headerError;
        ok &=
            expect(decodeHeaderValue(encodeHeaderValue(unicode), &headerError) == unicode &&
                       decodeHeaderValue(encodeHeaderValue(QStringLiteral("=?base64?literal?="))) ==
                           QStringLiteral("=?base64?literal?="),
                   QStringLiteral("MCP header Base64 sentinel must round-trip safely"));

        const ImplementationInfo serverInfo{QStringLiteral("DS Editor Lite"), QStringLiteral("1")};
        const auto discover = makeDiscoverResult(serverInfo, 1000, QStringLiteral("private"));
        const auto list = makeToolsListResult(QJsonArray{}, {}, 1000);
        const auto call = makeToolCallResult(QJsonObject{
            {QStringLiteral("ok"), true}
        });
        ok &= expect(discover.value(QStringLiteral("resultType")) == QStringLiteral("complete") &&
                         discover.value(QStringLiteral("supportedVersions")).toArray() ==
                             QJsonArray{QString::fromLatin1(ProtocolVersion)} &&
                         discover.value(QStringLiteral("capabilities"))
                             .toObject()
                             .value(QStringLiteral("tools"))
                             .isObject() &&
                         list.value(QStringLiteral("resultType")) == QStringLiteral("complete") &&
                         call.value(QStringLiteral("resultType")) == QStringLiteral("complete") &&
                         call.value(QStringLiteral("content")).isArray() &&
                         call.contains(QStringLiteral("structuredContent")),
                     QStringLiteral("modern discovery must advertise only the stateless 2026 "
                                    "version and complete envelopes"));

        const auto response = makeResultResponse(QStringLiteral("request-1"), call, serverInfo);
        ok &= expect(validateResponse(response, QStringLiteral("request-1")).valid(),
                     QStringLiteral("generated MCP result response must validate"));
        const auto legacyCall = makeToolCallResult(
            QJsonObject{
                {QStringLiteral("ok"), true}
        },
            false, {}, serverInfo, QString::fromLatin1(LegacyProtocolVersion));
        const auto legacyResponse =
            makeResultResponse(QStringLiteral("legacy-request"), legacyCall, serverInfo,
                               QString::fromLatin1(LegacyProtocolVersion));
        ok &= expect(!legacyResponse.value(QStringLiteral("result"))
                             .toObject()
                             .contains(QStringLiteral("resultType")) &&
                         validateResponse(legacyResponse, QStringLiteral("legacy-request"),
                                          QString::fromLatin1(LegacyProtocolVersion))
                             .valid(),
                     QStringLiteral("MCP 2025-11-25 results must validate without resultType"));
        const auto adaptedLegacyScalar = makeResultResponse(
            QStringLiteral("legacy-scalar"), makeToolCallResult(QStringLiteral("scalar")),
            serverInfo, QString::fromLatin1(LegacyProtocolVersion));
        ok &= expect(!adaptedLegacyScalar.value(QStringLiteral("result"))
                             .toObject()
                             .contains(QStringLiteral("structuredContent")) &&
                         adaptedLegacyScalar.value(QStringLiteral("result"))
                             .toObject()
                             .value(QStringLiteral("content"))
                             .isArray(),
                     QStringLiteral("legacy adaptation must retain text while omitting non-object "
                                    "structured content"));
        auto missingResultId = response;
        missingResultId.remove(QStringLiteral("id"));
        auto nullResultId = response;
        nullResultId.insert(QStringLiteral("id"), QJsonValue(QJsonValue::Null));
        auto fractionalResultId = response;
        fractionalResultId.insert(QStringLiteral("id"), 1.5);
        auto unsafeResultId = response;
        unsafeResultId.insert(QStringLiteral("id"), MaximumSafeJsonInteger + 1);
        ok &= expect(
            !validateResponse(missingResultId).valid() && !validateResponse(nullResultId).valid() &&
                !validateResponse(fractionalResultId).valid() &&
                !validateResponse(unsafeResultId).valid(),
            QStringLiteral("JSON-RPC results require a non-null string or safe-integer id"));
        const auto parseError =
            makeErrorResponse(QJsonValue(QJsonValue::Undefined),
                              ProtocolError{ParseError, QStringLiteral("Invalid JSON")});
        ok &= expect(parseError.value(QStringLiteral("id")).isNull() &&
                         validateResponse(parseError).valid(),
                     QStringLiteral("JSON-RPC errors without a request id must use a null id"));
        auto missingErrorId = parseError;
        missingErrorId.remove(QStringLiteral("id"));
        ok &= expect(!validateResponse(missingErrorId).valid(),
                     QStringLiteral("JSON-RPC errors must retain an explicit id, including null"));
        auto fractionalError = parseError;
        auto fractionalPayload = fractionalError.value(QStringLiteral("error")).toObject();
        fractionalPayload.insert(QStringLiteral("code"), -32020.5);
        fractionalError.insert(QStringLiteral("error"), fractionalPayload);
        ok &= expect(!validateResponse(fractionalError).valid(),
                     QStringLiteral("JSON-RPC error codes must be integers"));

        ok &= expect(
            validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {}, context,
                                        MaximumSafeJsonInteger))
                    .valid() &&
                validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {}, context,
                                            -MaximumSafeJsonInteger))
                    .valid() &&
                !validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {}, context, 1.5))
                     .valid() &&
                !validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {}, context,
                                             MaximumSafeJsonInteger + 1))
                     .valid(),
            QStringLiteral("JSON-RPC request numeric ids must be safe integers"));
        ok &= expect(
            validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {}, context,
                                        QString(MaximumRequestIdCodeUnits, u'i')))
                    .valid() &&
                !validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {}, context,
                                             QString(MaximumRequestIdCodeUnits + 1, u'i')))
                     .valid(),
            QStringLiteral("JSON-RPC string ids must have a bounded representation"));

        QJsonObject nestedArguments;
        for (int depth = 0; depth < JsonResourceLimits{}.maximumDepth + 1; ++depth)
            nestedArguments = QJsonObject{
                {QStringLiteral("nested"), nestedArguments}
            };
        const auto depthLimitedRequest =
            makeRequest(QString::fromLatin1(ToolsCallMethod),
                        QJsonObject{
                            {QStringLiteral("name"),      QStringLiteral("notes.insert")},
                            {QStringLiteral("arguments"), nestedArguments               }
        },
                        context, QStringLiteral("deep"));
        ok &= expect(!validateRequest(depthLimitedRequest).valid(),
                     QStringLiteral("MCP parsing must apply central JSON resource limits"));
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = true;
    ok &= testCanonicalJson();
    ok &= testJsonSchema();
    ok &= testSchemaCompatibility();
    ok &= testPublicContract();
    ok &= testExposurePolicy();
    ok &= testMcpProtocol();
    if (ok)
        QTextStream(stdout) << "Validated AutomationWire protocol contracts" << Qt::endl;
    return ok ? 0 : 1;
}
