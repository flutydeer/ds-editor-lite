#include "tst_automation_protocol.h"
#include <lite/AutomationWire/ControlLevel.h>
#include <lite/AutomationWire/CanonicalJson.h>
#include <lite/AutomationWire/ExposurePolicy.h>
#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/McpProtocol.h>
#include <lite/AutomationWire/PublicConstants.h>
#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QCoreApplication>
#include <QtTest>
#include "../TestSupport/TestAssertions.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>


using namespace AutomationWire;

using TestSupport::expect;

void AutomationProtocolTests::cursorRoundTrip_data() {
    QTest::addColumn<qint64>("offset");
    QTest::newRow("first-page") << qint64(0);
    QTest::newRow("following-page") << qint64(37);
    QTest::newRow("maximum-safe-integer") << qint64(MaximumSafeJsonInteger);
}

void AutomationProtocolTests::cursorRoundTrip() {
    QFETCH(qint64, offset);
    OpaqueCursorCodec codec;
    const auto token = codec.issue("manifest:l2", "snapshot-a", offset);
    QVERIFY(!token.isEmpty());
    QVERIFY(!token.contains(u'='));
    QVERIFY(!token.contains(u'+'));
    QVERIFY(!token.contains(u'/'));
    const auto parsed = codec.parse(token, "manifest:l2", "snapshot-a");
    QVERIFY(parsed.valid());
    QCOMPARE(*parsed.offset, offset);
}

void AutomationProtocolTests::invalidCursorOffset_data() {
    QTest::addColumn<qint64>("offset");
    QTest::newRow("negative") << qint64(-1);
    QTest::newRow("unsafe-integer") << qint64(MaximumSafeJsonInteger + 1);
}

void AutomationProtocolTests::invalidCursorOffset() {
    QFETCH(qint64, offset);
    OpaqueCursorCodec codec;
    QVERIFY(codec.issue("manifest:l2", "snapshot-a", offset).isEmpty());
}

void AutomationProtocolTests::changedScopeInvalidatesCursor_data() {
    QTest::addColumn<QString>("context");
    QTest::addColumn<QString>("snapshot");
    QTest::addColumn<int>("error");
    QTest::newRow("other-collection") << QString("tasks:list") << QString("snapshot-a")
                                      << int(OpaqueCursorError::ContextMismatch);
    QTest::newRow("changed-collection") << QString("manifest:l2") << QString("snapshot-b")
                                        << int(OpaqueCursorError::SnapshotMismatch);
}

void AutomationProtocolTests::changedScopeInvalidatesCursor() {
    QFETCH(QString, context);
    QFETCH(QString, snapshot);
    QFETCH(int, error);
    OpaqueCursorCodec codec;
    const auto token = codec.issue("manifest:l2", "snapshot-a", 37);
    QCOMPARE(int(codec.parse(token, context, snapshot).error), error);
}

void AutomationProtocolTests::malformedCursor_data() {
    QTest::addColumn<QString>("token");
    QTest::newRow("empty") << QString();
    QTest::newRow("truncated") << QString("abc");
    QTest::newRow("foreign-format") << QString("a.b.c");
    QTest::newRow("invalid-base64") << QString("*");
}

void AutomationProtocolTests::malformedCursor() {
    QFETCH(QString, token);
    OpaqueCursorCodec codec;
    QVERIFY(!codec.parse(token, "manifest:l2", "snapshot-a").valid());
}

void AutomationProtocolTests::canonicalJsonEncoding() {
    const QJsonObject first{
        {QStringLiteral("z"), QJsonArray{3, 2, 1}                                                },
        {QStringLiteral("a"),
         QJsonObject{{QStringLiteral("b"), true}, {QStringLiteral("a"), QStringLiteral("value")}}},
    };
    const QJsonObject second{
        {QStringLiteral("a"),
         QJsonObject{{QStringLiteral("a"), QStringLiteral("value")}, {QStringLiteral("b"), true}}},
        {QStringLiteral("z"), QJsonArray{3, 2, 1}                                                },
    };
    expect(canonicalJson(first) == canonicalJson(second),
           QStringLiteral("canonical JSON must ignore object insertion order"));
    expect(canonicalJson(first) ==
               QByteArrayLiteral("{\"a\":{\"a\":\"value\",\"b\":true},\"z\":[3,2,1]}"),
           QStringLiteral("canonical JSON must use sorted compact object keys"));
    const auto digest = sha256Digest(first);
    expect(digest.startsWith(QStringLiteral("sha256:")) && digest.size() == 71,
           QStringLiteral("canonical digest must be a tagged SHA-256 value"));
}

void AutomationProtocolTests::jsonSchema() {
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
                 JsonSchema::array(JsonSchema::reference(QStringLiteral("#/$defs/Item")), 1, 2)},
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
    expect(checkJsonSchema(schema).valid(),
           QStringLiteral("supported JSON Schema must pass schema checking"));
    expect(validateJsonValue(valid, schema).valid(),
           QStringLiteral("valid JSON must satisfy refs, arrays, ranges, enum and oneOf"));

    auto invalid = valid;
    invalid.insert(QStringLiteral("unexpected"), true);
    expect(!validateJsonValue(invalid, schema).valid(),
           QStringLiteral("additionalProperties false must reject unknown fields"));
    invalid = valid;
    invalid.insert(QStringLiteral("item"), QJsonObject{
                                               {QStringLiteral("code"),  QStringLiteral("c")},
                                               {QStringLiteral("count"), 4                  }
    });
    expect(!validateJsonValue(invalid, schema).valid(),
           QStringLiteral("enum and numeric ranges must be enforced"));

    auto unknownKeyword = JsonSchema::object();
    unknownKeyword.insert(QStringLiteral("contains"), QJsonObject{});
    expect(!checkJsonSchema(unknownKeyword).valid(),
           QStringLiteral("unknown validation keywords must fail closed"));
    const auto referencedUnknownKeyword =
        JsonSchema::document(JsonSchema::reference(QStringLiteral("#/$defs/Bad")),
                             QJsonObject{
                                 {QStringLiteral("Bad"), unknownKeyword}
    });
    expect(!checkJsonSchema(referencedUnknownKeyword).valid(),
           QStringLiteral("unknown keywords in referenced schemas must fail closed"));
    expect(!checkJsonSchema(JsonSchema::reference(QStringLiteral("#/$defs/Missing"))).valid(),
           QStringLiteral("unresolved refs must fail closed"));
    const auto nonCanonicalArrayReference = JsonSchema::document(
        JsonSchema::oneOf(QJsonArray{JsonSchema::reference(QStringLiteral("#/oneOf/00")), false}));
    expect(!checkJsonSchema(nonCanonicalArrayReference).valid(),
           QStringLiteral("JSON Pointer array indices must use canonical decimal syntax"));
    auto annotationReference =
        JsonSchema::document(JsonSchema::reference(QStringLiteral("#/default")));
    annotationReference.insert(QStringLiteral("default"),
                               QJsonObject{
                                   {QStringLiteral("contains"), QJsonObject{}}
    });
    expect(!checkJsonSchema(annotationReference).valid(),
           QStringLiteral("refs must not reinterpret annotation values as schemas"));

    auto containerReference = JsonSchema::document(JsonSchema::object({
        {QStringLiteral("name"), JsonSchema::string()}
    }));
    containerReference.insert(QStringLiteral("$ref"), QStringLiteral("#/properties"));
    expect(!checkJsonSchema(containerReference).valid(),
           QStringLiteral("refs must point to schema nodes, not schema containers"));

    const auto cyclicReference = JsonSchema::document(
        JsonSchema::reference(QStringLiteral("#/$defs/Loop")),
        QJsonObject{
            {QStringLiteral("Loop"), JsonSchema::reference(QStringLiteral("#/$defs/Loop"))}
    });
    const auto cyclicResult = validateJsonValue(QJsonObject{}, cyclicReference);
    expect(
        checkJsonSchema(cyclicReference).valid() && !cyclicResult.valid() &&
            cyclicResult.issues.first().code == SchemaIssueCode::LimitExceeded,
        QStringLiteral("non-consuming cyclic refs must fail closed without unbounded recursion"));

    const auto unconstrainedInteger = JsonSchema::document(JsonSchema::integer());
    expect(
        validateJsonValue(QJsonValue(MaximumSafeJsonInteger), unconstrainedInteger).valid() &&
            validateJsonValue(QJsonValue(-MaximumSafeJsonInteger), unconstrainedInteger).valid() &&
            !validateJsonValue(QJsonValue(MaximumSafeJsonInteger + 1), unconstrainedInteger)
                 .valid() &&
            !validateJsonValue(QJsonValue(-MaximumSafeJsonInteger - 1), unconstrainedInteger)
                 .valid(),
        QStringLiteral("the generic integer validator must enforce the JSON safe range"));

    auto safeCardinality = JsonSchema::array(true);
    safeCardinality.insert(QStringLiteral("maxItems"), MaximumSafeJsonInteger);
    auto unsafeCardinality = safeCardinality;
    unsafeCardinality.insert(QStringLiteral("maxItems"), MaximumSafeJsonInteger + 1);
    expect(checkJsonSchema(safeCardinality).valid() && !checkJsonSchema(unsafeCardinality).valid(),
           QStringLiteral("Schema cardinalities must be safely representable qint64 values"));

    JsonResourceLimits limits;
    expect(!checkJsonResourceLimits(QJsonValue(QJsonValue::Undefined), limits).valid(),
           QStringLiteral("undefined values must not pass as JSON"));
    limits.maximumStringCodeUnits = 3;
    expect(checkJsonResourceLimits(QStringLiteral("abc"), limits).valid() &&
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
    expect(checkJsonResourceLimits(QJsonArray{1, 2}, limits).valid() &&
               !checkJsonResourceLimits(QJsonArray{1, 2, 3}, limits).valid(),
           QStringLiteral("central JSON resource limits must bound arrays"));
    limits = {};
    limits.maximumObjectProperties = 2;
    expect(checkJsonResourceLimits(
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
    expect(checkJsonResourceLimits(QJsonArray{1, 2}, limits).valid() &&
               !checkJsonResourceLimits(QJsonArray{1, 2, 3}, limits).valid(),
           QStringLiteral("central JSON resource limits must bound aggregate nodes"));
    limits = {};
    limits.maximumDepth = 2;
    expect(checkJsonResourceLimits(
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
}

void AutomationProtocolTests::exposurePolicy() {
    const auto l0 = selectExposure({ExposureLevel::L0});
    const auto l1 = selectExposure({ExposureLevel::L1});
    const auto l2 = selectExposure({ExposureLevel::L2});
    const auto l3 = selectExposure({ExposureLevel::L3});
    QSet<QString> declaredIds;
    for (const auto &target : publicExposureTargets())
        declaredIds.insert(target.operationId);
    const auto includesAll = [](const QSet<QString> &superset, const QSet<QString> &subset) {
        for (const auto &id : subset) {
            if (!superset.contains(id))
                return false;
        }
        return true;
    };
    expect(l0.valid() && l1.valid() && l2.valid() && l3.valid() &&
               includesAll(l1.exposedIds, l0.exposedIds) &&
               includesAll(l2.exposedIds, l1.exposedIds) &&
               includesAll(l3.exposedIds, l2.exposedIds) && l3.exposedIds == declaredIds,
           QStringLiteral("connector exposure presets must be cumulative and complete"));

    const auto protectedL0 = selectExposure({
        .controlLevel = ExposureLevel::L0,
        .excludes = {QStringLiteral("category:application")},
    });
    expect(protectedL0.exposedIds == QSet<QString>{QStringLiteral("application.get_info"),
                                                   QStringLiteral("application.get_status"),
                                                   QStringLiteral("application.request_exit"),
                                                   QStringLiteral("application.request_restart")},
           QStringLiteral("connector excludes must not remove intrinsic L0 tools"));

    const ExposureConfig filtered{
        .controlLevel = ExposureLevel::L0,
        .includes = {QStringLiteral("category:notes"), QStringLiteral("missing.future")},
        .excludes = {QStringLiteral("prefix:notes.set_")},
    };
    const auto selection = selectExposure(filtered);
    expect(selection.valid() && selection.exposedIds.contains(QStringLiteral("notes.list")) &&
               !selection.exposedIds.contains(QStringLiteral("notes.set_lyric")) &&
               selection.pendingSelectors.contains(QStringLiteral("id:missing.future")),
           QStringLiteral("include/exclude/pending selectors must compose deterministically"));
    expect(!parseExposureSelector(QStringLiteral("prefix:notes.*")).valid() &&
               !parseExposureSelector(QStringLiteral("unknown:notes")).valid() &&
               parseExposureSelector(QStringLiteral("notes.insert")).selector->normalized() ==
                   QStringLiteral("id:notes.insert"),
           QStringLiteral(
               "selector grammar must reject glob/unknown prefixes and normalize bare IDs"));

    auto targets = publicExposureTargets();
    targets.append({QStringLiteral("future.gui_tool"), QStringLiteral("future"), ControlLevel::L3});
    expect(!selectExposure({ExposureLevel::L2}, targets)
                   .exposedIds.contains(QStringLiteral("future.gui_tool")) &&
               selectExposure({ExposureLevel::L3}, targets)
                   .exposedIds.contains(QStringLiteral("future.gui_tool")),
           QStringLiteral("higher exposure presets must include higher control-level targets"));
}

void AutomationProtocolTests::mcpProtocol() {
    using namespace AutomationWire::Mcp;
    const RequestContext context{
        .clientCapabilities = {},
        .clientInfo = ImplementationInfo{QStringLiteral("test-client"), QStringLiteral("1")},
    };
    const auto request = makeRequest(QString::fromLatin1(ToolsCallMethod),
                                     QJsonObject{
                                         {QStringLiteral("name"),      QStringLiteral("notes.insert")},
                                         {QStringLiteral("arguments"), QJsonObject{}                 }
    },
                                     context, QStringLiteral("request-1"));
    const auto parsed = validateRequest(request);
    expect(parsed.valid() && parsed.request->name == QStringLiteral("notes.insert") &&
               parsed.request->clientInfo.has_value(),
           QStringLiteral("modern MCP request metadata must parse without initialize"));
    expect(isSupportedCoreMethod(QString::fromLatin1(InitializeMethod)) &&
               isSupportedCoreMethod(QString::fromLatin1(DiscoverMethod)),
           QStringLiteral("the shared core must expose both MCP lifecycle families"));

    const RequestContext legacyContext{
        .protocolVersion = QString::fromLatin1(LegacyProtocolVersion),
        .clientCapabilities = QJsonObject{},
        .clientInfo = ImplementationInfo{QStringLiteral("legacy-client"), QStringLiteral("1")},
    };
    const auto initialize = makeInitializeRequest(legacyContext, QStringLiteral("init-1"));
    const auto legacyInitialize = validateRequest(initialize);
    const auto legacyList = validateRequest(makeRequest(QString::fromLatin1(ToolsListMethod), {},
                                                        legacyContext, QStringLiteral("list-1")));
    expect(legacyInitialize.valid() && legacyList.valid() &&
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
        validateRequest(makeRequest(QString::fromLatin1(ToolsListMethod), {}, compatibilityContext,
                                    QStringLiteral("list-2025-06")));
    expect(compatibilityInitialize.valid() && compatibilityList.valid() &&
               compatibilityInitialize.request->protocolVersion ==
                   QString::fromLatin1(CompatibilityProtocolVersion) &&
               isLegacyProtocolVersion(compatibilityList.request->protocolVersion),
           QStringLiteral("MCP 2025-06-18 must share the legacy initialize lifecycle"));
    expect(!validateRequest(makeInitializeRequest(context, QStringLiteral("modern-init"))).valid(),
           QStringLiteral("MCP 2026-07-28 must reject the removed initialize handshake"));

    auto noClientInfoContext = context;
    noClientInfoContext.clientInfo.reset();
    expect(validateRequest(
               makeRequest(QString::fromLatin1(DiscoverMethod), {}, noClientInfoContext, 1))
               .valid(),
           QStringLiteral("clientInfo must be optional"));
    const auto cancellation = validateRequest(QJsonObject{
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")                       },
        {QStringLiteral("method"),  QStringLiteral("notifications/cancelled")   },
        {QStringLiteral("params"),
         QJsonObject{{QStringLiteral("requestId"), QStringLiteral("request-1")}}},
    });
    expect(cancellation.valid() && cancellation.request->notification &&
               cancellation.request->meta.isEmpty() &&
               cancellation.request->protocolVersion == QString::fromLatin1(LegacyProtocolVersion),
           QStringLiteral("notification metadata must remain optional"));
    auto invalidRequest = request;
    auto invalidParams = invalidRequest.value(QStringLiteral("params")).toObject();
    auto invalidMeta = invalidParams.value(QStringLiteral("_meta")).toObject();
    invalidMeta.remove(QString::fromLatin1(ClientCapabilitiesMetaKey));
    invalidParams.insert(QStringLiteral("_meta"), invalidMeta);
    invalidRequest.insert(QStringLiteral("params"), invalidParams);
    expect(!validateRequest(invalidRequest).valid(),
           QStringLiteral("clientCapabilities must be required per request"));

    const TransportMetadata metadata{
        .protocolVersion = QString::fromLatin1(ProtocolVersion),
        .method = QString::fromLatin1(ToolsCallMethod),
        .name = QStringLiteral("notes.insert"),
    };
    expect(validateTransportMetadata(metadata, *parsed.request).valid(),
           QStringLiteral("matching MCP header metadata must validate"));
    auto mismatch = metadata;
    mismatch.name = QStringLiteral("notes.remove");
    const auto mismatchResult = validateTransportMetadata(mismatch, *parsed.request);
    expect(!mismatchResult.valid() && mismatchResult.error->code == ErrorCode::HeaderMismatch,
           QStringLiteral("header/body mismatch must use HeaderMismatch"));

    auto unknownVersionContext = context;
    unknownVersionContext.protocolVersion = QStringLiteral("2099-01-01");
    const auto unknownVersionRequest =
        makeRequest(QString::fromLatin1(DiscoverMethod), {}, unknownVersionContext, 2);
    const auto unknownVersionNotification =
        validateRequest(makeRequest(QStringLiteral("notifications/cancelled"),
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
    expect(structurallyParsed.valid() && !versionValidation.valid() &&
               !unknownVersionNotification.valid() &&
               unknownVersionNotification.error.code == ErrorCode::UnsupportedProtocolVersion &&
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
    expect(discover.value(QStringLiteral("resultType")) == QStringLiteral("complete") &&
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
    expect(validateResponse(response, QStringLiteral("request-1")).valid(),
           QStringLiteral("generated MCP result response must validate"));
    const auto legacyCall = makeToolCallResult(
        QJsonObject{
            {QStringLiteral("ok"), true}
    },
        false, {}, serverInfo, QString::fromLatin1(LegacyProtocolVersion));
    const auto legacyResponse =
        makeResultResponse(QStringLiteral("legacy-request"), legacyCall, serverInfo,
                           QString::fromLatin1(LegacyProtocolVersion));
    expect(!legacyResponse.value(QStringLiteral("result"))
                   .toObject()
                   .contains(QStringLiteral("resultType")) &&
               validateResponse(legacyResponse, QStringLiteral("legacy-request"),
                                QString::fromLatin1(LegacyProtocolVersion))
                   .valid(),
           QStringLiteral("MCP 2025-11-25 results must validate without resultType"));
    const auto adaptedLegacyScalar = makeResultResponse(
        QStringLiteral("legacy-scalar"), makeToolCallResult(QStringLiteral("scalar")), serverInfo,
        QString::fromLatin1(LegacyProtocolVersion));
    expect(!adaptedLegacyScalar.value(QStringLiteral("result"))
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
    expect(!validateResponse(missingResultId).valid() && !validateResponse(nullResultId).valid() &&
               !validateResponse(fractionalResultId).valid() &&
               !validateResponse(unsafeResultId).valid(),
           QStringLiteral("JSON-RPC results require a non-null string or safe-integer id"));
    const auto parseError =
        makeErrorResponse(QJsonValue(QJsonValue::Undefined),
                          ProtocolError{ParseError, QStringLiteral("Invalid JSON")});
    expect(parseError.value(QStringLiteral("id")).isNull() && validateResponse(parseError).valid(),
           QStringLiteral("JSON-RPC errors without a request id must use a null id"));
    auto missingErrorId = parseError;
    missingErrorId.remove(QStringLiteral("id"));
    expect(!validateResponse(missingErrorId).valid(),
           QStringLiteral("JSON-RPC errors must retain an explicit id, including null"));
    auto fractionalError = parseError;
    auto fractionalPayload = fractionalError.value(QStringLiteral("error")).toObject();
    fractionalPayload.insert(QStringLiteral("code"), -32020.5);
    fractionalError.insert(QStringLiteral("error"), fractionalPayload);
    expect(!validateResponse(fractionalError).valid(),
           QStringLiteral("JSON-RPC error codes must be integers"));

    expect(validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {}, context,
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
    expect(validateRequest(makeRequest(QString::fromLatin1(DiscoverMethod), {}, context,
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
    expect(!validateRequest(depthLimitedRequest).valid(),
           QStringLiteral("MCP parsing must apply central JSON resource limits"));
}
