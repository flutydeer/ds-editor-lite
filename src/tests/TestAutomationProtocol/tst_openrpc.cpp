#include "tst_automation_protocol.h"

#include <lite/AutomationWire/OpenRpc.h>
#include <lite/AutomationWire/JsonSchema.h>

#include <QJsonDocument>
#include <QtTest>

using namespace AutomationWire;

namespace {
    QJsonObject method(const QJsonObject &document, const QString &name) {
        for (const auto &value : document.value("methods").toArray()) {
            if (value.toObject().value("name") == name)
                return value.toObject();
        }
        return {};
    }

    QJsonValue resolvePointer(const QJsonObject &document, const QString &reference) {
        QJsonValue value(document);
        for (const auto &token : reference.mid(2).split(u'/')) {
            const auto key = QString(token).replace("~1", "/").replace("~0", "~");
            value = value.isArray() ? value.toArray().at(key.toInt()) : value.toObject().value(key);
        }
        return value;
    }

    void verifySchemaReferences(const QJsonValue &value, const QJsonObject &document) {
        if (value.isArray()) {
            for (const auto &item : value.toArray())
                verifySchemaReferences(item, document);
        } else if (value.isObject()) {
            const auto object = value.toObject();
            if (object.contains("$ref")) {
                const auto reference = object.value("$ref").toString();
                QVERIFY2(reference.startsWith("#/components/schemas/"), qPrintable(reference));
                QVERIFY2(!resolvePointer(document, reference).isUndefined(), qPrintable(reference));
            }
            for (const auto &keyword : {"properties", "definitions"}) {
                const auto children = object.value(keyword).toObject();
                for (auto it = children.begin(); it != children.end(); ++it)
                    verifySchemaReferences(it.value(), document);
            }
            for (const auto &keyword : {"items", "additionalProperties", "allOf", "oneOf", "anyOf"})
                verifySchemaReferences(object.value(keyword), document);
        }
    }
}

void AutomationProtocolTests::openRpcContracts() {
    const auto &contracts = publicToolContracts();
    QString error;
    const auto document = makeOpenRpcDocument(
        contracts, {"Contract test", "1.0", "http://127.0.0.1:12345/automation/v1"}, &error);
    QVERIFY2(document, qPrintable(error));
    QCOMPARE(document->value("openrpc").toString(), QString::fromLatin1(OpenRpcVersion));
    QCOMPARE(document->value("methods").toArray().size(), contracts.size() + 1);
    QCOMPARE(document->value("servers").toArray().first().toObject().value("url").toString(),
             QStringLiteral("http://127.0.0.1:12345/automation/v1"));
    QVERIFY(QJsonDocument(*document).toJson(QJsonDocument::Compact).size() < 8 * 1024 * 1024);
    for (const auto &contract : contracts) {
        const auto description = method(*document, contract.operationId);
        QCOMPARE(description.value("x-ds-input-schema").toObject(), contract.inputSchema);
        QCOMPARE(description.value("paramStructure").toString(), QStringLiteral("by-name"));
        const auto output = description.value("result").toObject().value("schema").toObject();
        QVERIFY(!resolvePointer(*document, output.value("$ref").toString()).isUndefined());
        bool optionalSeen = false;
        QSet<QString> names;
        for (const auto &value : description.value("params").toArray()) {
            const auto parameter = value.toObject();
            QVERIFY(!names.contains(parameter.value("name").toString()));
            names.insert(parameter.value("name").toString());
            if (parameter.value("required").toBool())
                QVERIFY(!optionalSeen);
            else
                optionalSeen = true;
            verifySchemaReferences(parameter.value("schema"), *document);
        }
    }
    const auto schemas = document->value("components").toObject().value("schemas").toObject();
    for (auto it = schemas.begin(); it != schemas.end(); ++it)
        verifySchemaReferences(it.value(), *document);

    const auto reveal = method(*document, "track_panel.reveal_clips");
    QMap<QString, bool> parameters;
    for (const auto &value : reveal.value("params").toArray()) {
        const auto parameter = value.toObject();
        parameters.insert(parameter.value("name").toString(), parameter.value("required").toBool());
    }
    QCOMPARE(
        parameters,
        (QMap<QString, bool>{
            {"window_id",   true },
            {"document_id", true },
            {"track_id",    false},
            {"clip_ids",    false}
    }));
    const auto tasks = method(*document, "tasks.get");
    for (const auto &value : tasks.value("params").toArray()) {
        const auto parameter = value.toObject();
        QCOMPARE(parameter.value("required").toBool(), parameter.value("name") != "document_id");
    }
    QVERIFY(!method(*document, QString::fromLatin1(OpenRpcDiscoverMethod)).isEmpty());
}

void AutomationProtocolTests::openRpcSchemaConversion() {
    auto input = JsonSchema::document(
        JsonSchema::object({
            {"value", QJsonObject{{"$ref", "#/$defs/value"}, {"minLength", 2}}},
            {"$defs", JsonSchema::string()                                    },
    }),
        {{"value", JsonSchema::string()}});
    const QJsonObject literal{
        {"$defs", "payload"          },
        {"$ref",  "payload-reference"}
    };
    input.insert("default", literal);
    const ToolContract contract{.operationId = "test.schema",
                                .title = "Test schema",
                                .inputSchema = input,
                                .outputSchema = JsonSchema::document(JsonSchema::object())};
    QString error;
    const auto document = makeOpenRpcDocument({contract}, {"Test", "1", {}}, &error);
    QVERIFY2(document, qPrintable(error));
    const auto schema = document->value("components")
                            .toObject()
                            .value("schemas")
                            .toObject()
                            .value("test.schema.params")
                            .toObject();
    QCOMPARE(schema.value("$schema").toString(),
             QStringLiteral("http://json-schema.org/draft-07/schema#"));
    QVERIFY(schema.contains("definitions"));
    QVERIFY(!schema.contains("$defs"));
    QCOMPARE(schema.value("default").toObject(), literal);
    const auto properties = schema.value("properties").toObject();
    QVERIFY(properties.contains("$defs"));
    const auto value = properties.value("value").toObject();
    QCOMPARE(value.value("minLength").toInt(), 2);
    QCOMPARE(value.value("allOf").toArray().first().toObject().value("$ref").toString(),
             QStringLiteral("#/components/schemas/test.schema.params/definitions/value"));

    auto unsupported = contract;
    unsupported.inputSchema.insert("prefixItems", QJsonArray{});
    QVERIFY(!makeOpenRpcDocument({unsupported}, {"Test", "1", {}}, &error));
    QVERIFY(!error.isEmpty());
}
