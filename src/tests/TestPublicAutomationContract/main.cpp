#include <lite/AutomationWire/ExposurePolicy.h>
#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/PublicConstants.h>
#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/AutomationWire/PublicValueDomains.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <limits>
#include <string>

namespace {
    int failures = 0;

    void expect(const bool condition, const QString &message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    QJsonObject referencedSchema(const QString &reference, const QJsonObject &root) {
        constexpr auto prefix = "#/$defs/";
        if (!reference.startsWith(QString::fromLatin1(prefix)))
            return {};
        return root.value(QStringLiteral("$defs"))
            .toObject()
            .value(reference.sliced(static_cast<qsizetype>(std::char_traits<char>::length(prefix))))
            .toObject();
    }

    QJsonValue sampleValue(QJsonObject schema, const QJsonObject &root, const QString &field = {}) {
        if (schema.contains(QStringLiteral("$ref")))
            schema = referencedSchema(schema.value(QStringLiteral("$ref")).toString(), root);
        if (schema.contains(QStringLiteral("const")))
            return schema.value(QStringLiteral("const"));
        const auto values = schema.value(QStringLiteral("enum")).toArray();
        if (!values.isEmpty())
            return values.first();
        const auto branches = schema.value(QStringLiteral("oneOf")).toArray();
        if (!branches.isEmpty())
            return sampleValue(branches.first().toObject(), root, field);

        const auto type = schema.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("object")) {
            QJsonObject result;
            const auto properties = schema.value(QStringLiteral("properties")).toObject();
            for (const auto &required : schema.value(QStringLiteral("required")).toArray()) {
                const auto name = required.toString();
                result.insert(name,
                              sampleValue(properties.value(name).toObject(), root, name));
            }
            const auto minimum = schema.value(QStringLiteral("minProperties")).toInteger();
            for (auto it = properties.constBegin(); result.size() < minimum &&
                                                   it != properties.constEnd(); ++it) {
                if (!result.contains(it.key()))
                    result.insert(it.key(), sampleValue(it.value().toObject(), root, it.key()));
            }
            return result;
        }
        if (type == QStringLiteral("array")) {
            QJsonArray result;
            const auto count = schema.value(QStringLiteral("minItems")).toInteger();
            for (qint64 index = 0; index < count; ++index) {
                result.append(sampleValue(schema.value(QStringLiteral("items")).toObject(), root,
                                          field));
            }
            return result;
        }
        if (type == QStringLiteral("boolean"))
            return false;
        if (type == QStringLiteral("integer") || type == QStringLiteral("number"))
            return schema.contains(QStringLiteral("minimum"))
                       ? schema.value(QStringLiteral("minimum"))
                       : QJsonValue(0);
        if (type == QStringLiteral("null"))
            return QJsonValue(QJsonValue::Null);
        if (schema.value(QStringLiteral("format")).toString() == QStringLiteral("uuid"))
            return QStringLiteral("00000000-0000-4000-8000-000000000001");
        const auto pattern = schema.value(QStringLiteral("pattern")).toString();
        if (pattern == QStringLiteral("^sha256:[0-9a-f]{64}$"))
            return QStringLiteral("sha256:") + QString(64, u'0');
        const auto minimumLength = std::max<qint64>(1, schema.value(QStringLiteral("minLength")).toInteger());
        return QString(minimumLength, u'x');
    }

    QJsonObject selectedRootBranch(const QJsonObject &schema) {
        const auto branches = schema.value(QStringLiteral("oneOf")).toArray();
        return branches.isEmpty() ? schema : branches.first().toObject();
    }

    void verifyRequiredDeletion(const AutomationWire::ToolContract &contract,
                                const QJsonObject &schema, const QString &label) {
        auto sample = sampleValue(schema, schema).toObject();
        expect(AutomationWire::validateJsonValue(sample, schema).valid(),
               contract.operationId + u' ' + label + QStringLiteral(" fixture must validate"));
        const auto selected = selectedRootBranch(schema);
        for (const auto &required : selected.value(QStringLiteral("required")).toArray()) {
            auto missing = sample;
            missing.remove(required.toString());
            expect(!AutomationWire::validateJsonValue(missing, schema).valid(),
                   contract.operationId + u' ' + label + QStringLiteral(" must require /") +
                       required.toString());
        }
    }

    void countOpenObjects(const QJsonValue &value, int &count) {
        if (value.isArray()) {
            for (const auto &item : value.toArray())
                countOpenObjects(item, count);
            return;
        }
        if (!value.isObject())
            return;
        const auto object = value.toObject();
        if (object.value(QStringLiteral("additionalProperties")) == QJsonValue(true))
            ++count;
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            countOpenObjects(it.value(), count);
    }

    QJsonObject documentVersion() {
        return {
            {QStringLiteral("document_id"),
             QStringLiteral("00000000-0000-4000-8000-000000000001")},
            {QStringLiteral("revision"), 1},
        };
    }

    QJsonObject taskSnapshot(const QString &operationId) {
        return {
            {QStringLiteral("task_id"),
             QStringLiteral("00000000-0000-4000-8000-000000000002")},
            {QStringLiteral("operation_id"), operationId},
            {QStringLiteral("document"), documentVersion()},
            {QStringLiteral("state"), QStringLiteral("queued")},
            {QStringLiteral("progress"),
             QJsonObject{
                 {QStringLiteral("minimum"), 0},
                 {QStringLiteral("maximum"), 100},
                 {QStringLiteral("value"), 0},
                 {QStringLiteral("indeterminate"), true},
             }},
        };
    }

    void verifyValueDomains() {
        for (int raw = static_cast<int>(AutomationWire::PublicValueDomain::AutomationProfile);
             raw < static_cast<int>(AutomationWire::PublicValueDomain::Count);
             ++raw) {
            const auto domain = static_cast<AutomationWire::PublicValueDomain>(raw);
            const auto values = AutomationWire::publicValueDomainValues(domain);
            expect(!AutomationWire::publicValueDomainName(domain).isEmpty(),
                   QStringLiteral("public value domain name must be registered"));
            expect(!values.isEmpty(), QStringLiteral("public value domain must not be empty: ") +
                                          AutomationWire::publicValueDomainName(domain));
            QSet<QString> canonical;
            for (const auto &value : values) {
                const auto encoded = QString::fromUtf8(
                    QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact));
                expect(!canonical.contains(encoded),
                       QStringLiteral("public value domain contains a duplicate: ") +
                           AutomationWire::publicValueDomainName(domain));
                canonical.insert(encoded);
                expect(AutomationWire::publicValueDomainContains(domain, value),
                       QStringLiteral("public value domain membership must round-trip"));
            }
        }
        expect(AutomationWire::automationProfileNames() ==
                   AutomationWire::publicStringValueDomainValues(
                       AutomationWire::PublicValueDomain::AutomationProfile),
               QStringLiteral("automation profile codec must use the public value domain"));
        expect(AutomationWire::exposureProfileNames() ==
                   AutomationWire::publicStringValueDomainValues(
                       AutomationWire::PublicValueDomain::ExposureProfile),
               QStringLiteral("exposure profile codec must use the public value domain"));
    }

    void verifyNumericDomains() {
        const auto *trackInsert =
            AutomationWire::findPublicTool(QStringLiteral("tracks.insert"));
        const auto track = trackInsert->inputSchema.value(QStringLiteral("properties"))
                               .toObject()
                               .value(QStringLiteral("track"))
                               .toObject()
                               .value(QStringLiteral("properties"))
                               .toObject();
        const auto pan = track.value(QStringLiteral("pan")).toObject();
        expect(pan.value(QStringLiteral("minimum")).toDouble() == AutomationWire::MinimumPan &&
                   pan.value(QStringLiteral("maximum")).toDouble() == AutomationWire::MaximumPan,
               QStringLiteral("track pan schema must use the public numeric domain"));
        expect(track.value(QStringLiteral("color_index"))
                       .toObject()
                       .value(QStringLiteral("maximum"))
                       .toInt() == AutomationWire::TrackPaletteColorCount - 1,
               QStringLiteral("track color schema must use the public palette size"));

        const auto *noteInsert = AutomationWire::findPublicTool(QStringLiteral("notes.insert"));
        const auto note = noteInsert->inputSchema.value(QStringLiteral("properties"))
                              .toObject()
                              .value(QStringLiteral("notes"))
                              .toObject()
                              .value(QStringLiteral("items"))
                              .toObject()
                              .value(QStringLiteral("properties"))
                              .toObject();
        expect(note.value(QStringLiteral("key_index"))
                       .toObject()
                       .value(QStringLiteral("minimum"))
                       .toInt() == AutomationWire::MinimumMidiKeyIndex &&
                   note.value(QStringLiteral("key_index"))
                           .toObject()
                           .value(QStringLiteral("maximum"))
                           .toInt() == AutomationWire::MaximumMidiKeyIndex,
               QStringLiteral("note key schema must use the public MIDI key range"));
        expect(note.value(QStringLiteral("cent_shift"))
                       .toObject()
                       .value(QStringLiteral("minimum"))
                       .toInt() == AutomationWire::MinimumCentShift &&
                   note.value(QStringLiteral("cent_shift"))
                           .toObject()
                           .value(QStringLiteral("maximum"))
                           .toInt() == AutomationWire::MaximumCentShift,
               QStringLiteral("note cent schema must use the public cent range"));

        const auto *audio =
            AutomationWire::findPublicTool(QStringLiteral("exports.audio.preview"));
        const auto sampleRate = audio->inputSchema.value(QStringLiteral("properties"))
                                    .toObject()
                                    .value(QStringLiteral("options"))
                                    .toObject()
                                    .value(QStringLiteral("properties"))
                                    .toObject()
                                    .value(QStringLiteral("sample_rate"))
                                    .toObject();
        expect(sampleRate.value(QStringLiteral("minimum")).toInt() ==
                       AutomationWire::MinimumAudioSampleRate &&
                   sampleRate.value(QStringLiteral("maximum")).toInt() ==
                       AutomationWire::MaximumAudioSampleRate,
               QStringLiteral("audio sample-rate schema must use the public numeric domain"));

        const auto *manifest =
            AutomationWire::findPublicTool(QStringLiteral("automation.get_manifest"));
        const auto limit = manifest->inputSchema.value(QStringLiteral("properties"))
                               .toObject()
                               .value(QStringLiteral("limit"))
                               .toObject();
        expect(limit.value(QStringLiteral("minimum")).toInt() ==
                       AutomationWire::MinimumPageSize &&
                   limit.value(QStringLiteral("maximum")).toInt() ==
                       AutomationWire::MaximumPageSize,
               QStringLiteral("pagination schema must use the public numeric domain"));
    }

    void verifyGetOptions() {
        const auto *contract = AutomationWire::findPublicTool(QStringLiteral("automation.get_options"));
        expect(contract, QStringLiteral("automation.get_options contract must exist"));
        if (!contract)
            return;
        const QJsonObject valid{
            {QStringLiteral("operation_id"), QStringLiteral("voices.describe")},
            {QStringLiteral("field_path"), QStringLiteral("/singer")},
            {QStringLiteral("partial_arguments"),
             QJsonObject{{QStringLiteral("singer"),
                          QJsonObject{{QStringLiteral("singer_id"), QStringLiteral("voice")}}}}},
        };
        expect(AutomationWire::validateJsonValue(valid, contract->inputSchema).valid(),
               QStringLiteral("get_options strict target branch must accept a valid partial root"));
        auto numericOperation = valid;
        numericOperation.insert(QStringLiteral("operation_id"), 13);
        expect(!AutomationWire::validateJsonValue(numericOperation, contract->inputSchema).valid(),
               QStringLiteral("get_options operation_id must reject numbers"));
        auto unknownPartial = valid;
        unknownPartial.insert(QStringLiteral("partial_arguments"),
                              QJsonObject{{QStringLiteral("unknown"), true}});
        expect(!AutomationWire::validateJsonValue(unknownPartial, contract->inputSchema).valid(),
               QStringLiteral("get_options partial root must reject unknown fields"));

        const QJsonObject staticChoice{
            {QStringLiteral("operation_id"), QStringLiteral("documents.new")},
            {QStringLiteral("field_path"), QStringLiteral("/template")},
            {QStringLiteral("partial_arguments"), QJsonObject{}},
        };
        expect(!AutomationWire::validateJsonValue(staticChoice, contract->inputSchema).valid(),
               QStringLiteral("get_options must only publish dynamic value-source branches"));

        const auto *voices = AutomationWire::findPublicTool(QStringLiteral("voices.list"));
        expect(voices &&
                   !AutomationWire::validateJsonValue(
                        QJsonObject{{QStringLiteral("package_id"), 1}}, voices->inputSchema)
                        .valid(),
               QStringLiteral("public package_id must reject numbers"));
    }

    void verifyDocumentReplacementContracts() {
        for (const auto &operation : {QStringLiteral("documents.new"),
                                      QStringLiteral("documents.open")}) {
            const auto *contract = AutomationWire::findPublicTool(operation);
            expect(contract, operation + QStringLiteral(" contract must exist"));
            if (!contract)
                continue;
            QJsonObject input{{QStringLiteral("unsaved_policy"), QStringLiteral("reject")}};
            if (operation == QStringLiteral("documents.open"))
                input.insert(QStringLiteral("path"), QStringLiteral("D:/project.dspx"));
            expect(AutomationWire::validateJsonValue(input, contract->inputSchema).valid(),
                   operation + QStringLiteral(" must allow omitted current version"));
            auto idOnly = input;
            idOnly.insert(QStringLiteral("document_id"),
                          documentVersion().value(QStringLiteral("document_id")));
            expect(!AutomationWire::validateJsonValue(idOnly, contract->inputSchema).valid(),
                   operation + QStringLiteral(" must reject an unpaired document_id"));
            auto revisionOnly = input;
            revisionOnly.insert(QStringLiteral("expected_revision"), 1);
            expect(!AutomationWire::validateJsonValue(revisionOnly, contract->inputSchema).valid(),
                   operation + QStringLiteral(" must reject an unpaired expected_revision"));
            idOnly.insert(QStringLiteral("expected_revision"), 1);
            expect(AutomationWire::validateJsonValue(idOnly, contract->inputSchema).valid(),
                   operation + QStringLiteral(" must accept a paired current version"));
        }

        const auto *newDocument = AutomationWire::findPublicTool(QStringLiteral("documents.new"));
        auto newOutput = sampleValue(newDocument->outputSchema, newDocument->outputSchema).toObject();
        newOutput.insert(QStringLiteral("previous"), QJsonValue(QJsonValue::Null));
        newOutput.insert(QStringLiteral("current"), QJsonValue(QJsonValue::Null));
        expect(AutomationWire::validateJsonValue(newOutput, newDocument->outputSchema).valid(),
               QStringLiteral("documents.new may report null replacement versions"));

        const auto *open = AutomationWire::findPublicTool(QStringLiteral("documents.open"));
        auto openOutput = sampleValue(open->outputSchema, open->outputSchema).toObject();
        openOutput.insert(QStringLiteral("document"), QJsonValue(QJsonValue::Null));
        expect(AutomationWire::validateJsonValue(openOutput, open->outputSchema).valid(),
               QStringLiteral("documents.open accepted result may have no previous document"));

        const auto *save = AutomationWire::findPublicTool(QStringLiteral("documents.save"));
        const auto saveProperties =
            save->inputSchema.value(QStringLiteral("properties")).toObject();
        expect(!save->inputSchema.value(QStringLiteral("required"))
                    .toArray()
                    .contains(QStringLiteral("path")) &&
                   saveProperties.value(QStringLiteral("allow_overwrite"))
                           .toObject()
                           .value(QStringLiteral("default")) == QJsonValue(false),
               QStringLiteral("documents.save uses save-current with a safe overwrite default"));
    }

    void verifySnapshotsAndCapabilities() {
        const auto *status =
            AutomationWire::findPublicTool(QStringLiteral("automation.get_status"));
        const auto statusProperties =
            status->outputSchema.value(QStringLiteral("properties")).toObject();
        expect(statusProperties.value(QStringLiteral("documents"))
                       .toObject()
                       .value(QStringLiteral("maxItems"))
                       .toInt() == 1 &&
                   statusProperties.value(QStringLiteral("windows"))
                           .toObject()
                           .value(QStringLiteral("maxItems"))
                           .toInt() == 1,
               QStringLiteral("single-document status arrays must have maxItems one"));

        const auto *parameter = AutomationWire::findPublicTool(QStringLiteral("parameters.get"));
        auto parameterSchema = parameter->outputSchema.value(QStringLiteral("properties"))
                                   .toObject()
                                   .value(QStringLiteral("snapshot"))
                                   .toObject()
                                   .value(QStringLiteral("properties"))
                                   .toObject()
                                   .value(QStringLiteral("curves"))
                                   .toObject()
                                   .value(QStringLiteral("items"))
                                   .toObject();
        const auto curveBranches = parameterSchema.value(QStringLiteral("oneOf")).toArray();
        expect(curveBranches.size() == 2,
               QStringLiteral("parameter snapshot must distinguish draw and anchor curves"));
        if (curveBranches.size() == 2) {
            expect(curveBranches.at(0).toObject().value(QStringLiteral("required")).toArray().contains(
                       QStringLiteral("curve_id")),
                   QStringLiteral("draw snapshot must require stable curve_id"));
            const auto anchor = curveBranches.at(1).toObject();
            expect(anchor.value(QStringLiteral("required")).toArray().contains(
                       QStringLiteral("curve_id")),
                   QStringLiteral("anchor snapshot must require stable curve_id"));
            const auto node = anchor.value(QStringLiteral("properties"))
                                  .toObject()
                                  .value(QStringLiteral("nodes"))
                                  .toObject()
                                  .value(QStringLiteral("items"))
                                  .toObject();
            expect(node.value(QStringLiteral("required")).toArray().contains(
                       QStringLiteral("anchor_id")),
                   QStringLiteral("anchor node snapshot must require stable anchor_id"));
        }

        const auto requireCapabilityFields = [](const QString &operation,
                                                const QStringList &fields) {
            const auto *contract = AutomationWire::findPublicTool(operation);
            const auto capability = contract->outputSchema.value(QStringLiteral("properties"))
                                        .toObject()
                                        .value(QStringLiteral("capabilities"))
                                        .toObject();
            const auto required = capability.value(QStringLiteral("required")).toArray();
            for (const auto &field : fields) {
                expect(required.contains(field), operation + QStringLiteral(" must require /") + field);
            }
        };
        requireCapabilityFields(QStringLiteral("exports.audio.get_capabilities"),
                                {QStringLiteral("formats"), QStringLiteral("sample_rates"),
                                 QStringLiteral("channel_modes"), QStringLiteral("mixing_modes"),
                                 QStringLiteral("source_modes"), QStringLiteral("sources")});
        requireCapabilityFields(QStringLiteral("extract.get_capabilities"),
                                {QStringLiteral("pitch"), QStringLiteral("midi"),
                                 QStringLiteral("languages")});
        requireCapabilityFields(QStringLiteral("inference.get_capabilities"),
                                {QStringLiteral("providers"), QStringLiteral("devices"),
                                 QStringLiteral("models")});

        const auto *playback = AutomationWire::findPublicTool(QStringLiteral("playback.get"));
        const auto playbackSnapshot = playback->outputSchema.value(QStringLiteral("properties"))
                                          .toObject()
                                          .value(QStringLiteral("snapshot"))
                                          .toObject();
        expect(playbackSnapshot.value(QStringLiteral("required"))
                   .toArray()
                   .contains(QStringLiteral("playable")),
               QStringLiteral("playback.get must expose current playability"));
    }

    void verifyPhaseTwoCapabilityContracts() {
        const auto *pitch =
            AutomationWire::findPublicTool(QStringLiteral("extract.pitch.start"));
        const auto pitchOptions = pitch->inputSchema.value(QStringLiteral("properties"))
                                      .toObject()
                                      .value(QStringLiteral("options"))
                                      .toObject();
        const auto pitchOptionProperties =
            pitchOptions.value(QStringLiteral("properties")).toObject();
        expect(pitchOptions.value(QStringLiteral("required"))
                       .toArray()
                       .contains(QStringLiteral("target_clip_id")) &&
                   !pitchOptionProperties.contains(QStringLiteral("minimum_frequency")) &&
                   !pitchOptionProperties.contains(QStringLiteral("maximum_frequency")),
               QStringLiteral(
                   "pitch extraction must require its target and omit unsupported frequency ranges"));

        const auto *midi =
            AutomationWire::findPublicTool(QStringLiteral("extract.midi.start"));
        const auto midiOptions = midi->inputSchema.value(QStringLiteral("properties"))
                                     .toObject()
                                     .value(QStringLiteral("options"))
                                     .toObject();
        expect(midiOptions.value(QStringLiteral("properties"))
                   .toObject()
                   .contains(QStringLiteral("client_ref")) &&
                   !midiOptions.value(QStringLiteral("required"))
                        .toArray()
                        .contains(QStringLiteral("client_ref")),
               QStringLiteral("MIDI extraction client_ref must be strict and optional"));
        const auto languageSource = std::find_if(
            midi->valueSources.cbegin(), midi->valueSources.cend(), [](const QJsonValue &value) {
                const auto source = value.toObject();
                return source.value(QStringLiteral("field_path")) ==
                    QStringLiteral("/options/default_language");
            });
        expect(languageSource != midi->valueSources.cend() &&
                   languageSource->toObject().value(QStringLiteral("operation_id")) ==
                       QStringLiteral("extract.get_capabilities"),
               QStringLiteral(
                   "MIDI extraction languages must come from the extraction capability view"));

        const auto pitchOptionDocument = AutomationWire::JsonSchema::document(pitchOptions);
        const auto midiOptionDocument = AutomationWire::JsonSchema::document(midiOptions);
        expect(AutomationWire::checkJsonSchema(pitchOptionDocument).valid() &&
                   AutomationWire::checkJsonSchema(midiOptionDocument).valid(),
               QStringLiteral("embedded extraction option schemas must be self-contained"));

        const auto capability = QJsonObject{
            {QStringLiteral("clip_id"), 1},
            {QStringLiteral("pitch"),
             QJsonObject{
                 {QStringLiteral("supported"), true},
                 {QStringLiteral("source_supported"), true},
                 {QStringLiteral("available"), false},
                 {QStringLiteral("module_state"), QStringLiteral("unavailable")},
                 {QStringLiteral("unavailable_reason"), QStringLiteral("module unavailable")},
                 {QStringLiteral("models"), QJsonArray{}},
                 {QStringLiteral("option_schema"), pitchOptionDocument},
                 {QStringLiteral("range_support"),
                  QJsonObject{
                      {QStringLiteral("source_range"),
                       QStringLiteral("visible_audio_clip")},
                      {QStringLiteral("custom_frequency"), false},
                  }},
             }},
            {QStringLiteral("midi"),
             QJsonObject{
                 {QStringLiteral("supported"), true},
                 {QStringLiteral("source_supported"), true},
                 {QStringLiteral("available"), false},
                 {QStringLiteral("module_state"), QStringLiteral("unavailable")},
                 {QStringLiteral("unavailable_reason"), QStringLiteral("module unavailable")},
                 {QStringLiteral("models"), QJsonArray{}},
                 {QStringLiteral("option_schema"), midiOptionDocument},
                 {QStringLiteral("range_support"),
                  QJsonObject{
                      {QStringLiteral("source_range"),
                       QStringLiteral("visible_audio_clip")},
                      {QStringLiteral("minimum_note_length"),
                       QJsonObject{
                           {QStringLiteral("minimum"), 1},
                           {QStringLiteral("maximum"), std::numeric_limits<int>::max()},
                       }},
                  }},
             }},
            {QStringLiteral("languages"), QJsonArray{}},
        };
        const auto *capabilities =
            AutomationWire::findPublicTool(QStringLiteral("extract.get_capabilities"));
        const QJsonObject capabilityOutput{
            {QStringLiteral("document"), documentVersion()},
            {QStringLiteral("capabilities"), capability},
        };
        expect(AutomationWire::validateJsonValue(capabilityOutput, capabilities->outputSchema)
                   .valid(),
               QStringLiteral(
                   "extraction capabilities must accept honest module, model, range, and option state"));

        const auto *preview =
            AutomationWire::findPublicTool(QStringLiteral("exports.audio.preview"));
        const auto previewSnapshot = preview->outputSchema.value(QStringLiteral("properties"))
                                         .toObject()
                                         .value(QStringLiteral("snapshot"))
                                         .toObject();
        const auto previewProperties =
            previewSnapshot.value(QStringLiteral("properties")).toObject();
        expect(previewProperties.contains(QStringLiteral("plan")) &&
                   previewProperties.contains(QStringLiteral("diagnostics")) &&
                   !previewProperties.contains(QStringLiteral("warning_flags")),
               QStringLiteral(
                   "audio export preview must expose typed plan and diagnostics without raw flags"));
    }

    void verifyTaskSnapshotUnion() {
        const auto *contract = AutomationWire::findPublicTool(QStringLiteral("tasks.get"));
        const auto kinds = AutomationWire::publicStringValueDomainValues(
            AutomationWire::PublicValueDomain::TaskKind);
        expect(kinds.size() == 9, QStringLiteral("public task kind domain must contain 9 async tools"));
        for (const auto &kind : kinds) {
            expect(AutomationWire::validateJsonValue(taskSnapshot(kind), contract->outputSchema).valid(),
                   QStringLiteral("task snapshot branch must accept ") + kind);
        }
        auto openingWithoutDocument = taskSnapshot(QStringLiteral("documents.open"));
        openingWithoutDocument.insert(QStringLiteral("document"), QJsonValue(QJsonValue::Null));
        QJsonObject openedResult{
            {QStringLiteral("previous"), QJsonValue(QJsonValue::Null)},
            {QStringLiteral("current"), documentVersion()},
            {QStringLiteral("changed"), true},
            {QStringLiteral("validated_only"), false},
            {QStringLiteral("affected_objects"), QJsonArray{}},
            {QStringLiteral("created_objects"), QJsonArray{}},
            {QStringLiteral("warnings"), QJsonArray{}},
        };
        openingWithoutDocument.insert(QStringLiteral("result"), openedResult);
        expect(AutomationWire::validateJsonValue(openingWithoutDocument, contract->outputSchema).valid(),
               QStringLiteral("documents.open task may start without a current document"));
        auto importWithoutDocument = taskSnapshot(QStringLiteral("documents.import"));
        importWithoutDocument.insert(QStringLiteral("document"), QJsonValue(QJsonValue::Null));
        expect(!AutomationWire::validateJsonValue(importWithoutDocument, contract->outputSchema).valid(),
               QStringLiteral("non-replacement tasks must retain a document version"));
        expect(!AutomationWire::validateJsonValue(taskSnapshot(QStringLiteral("unknown.start")),
                                                  contract->outputSchema)
                    .valid(),
               QStringLiteral("task snapshot must reject an unknown async operation"));
    }
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    verifyValueDomains();

    const auto &contracts = AutomationWire::publicToolContracts();
    expect(contracts.size() == 87, QStringLiteral("public contract count must remain 87"));
    QSet<QString> ids;
    int openObjects = 0;
    for (const auto &contract : contracts) {
        expect(!ids.contains(contract.operationId),
               QStringLiteral("public operation id must be unique: ") + contract.operationId);
        ids.insert(contract.operationId);
        expect(contract.inputSchema.value(QStringLiteral("type")) == QStringLiteral("object"),
               contract.operationId +
                   QStringLiteral(" input schema root must declare type object"));
        expect(AutomationWire::checkJsonSchema(contract.inputSchema).valid(),
               contract.operationId + QStringLiteral(" input schema must be supported"));
        expect(AutomationWire::checkJsonSchema(contract.outputSchema).valid(),
               contract.operationId + QStringLiteral(" output schema must be supported"));
        verifyRequiredDeletion(contract, contract.inputSchema, QStringLiteral("input"));
        verifyRequiredDeletion(contract, contract.outputSchema, QStringLiteral("output"));
        countOpenObjects(contract.inputSchema, openObjects);
        countOpenObjects(contract.outputSchema, openObjects);
    }
    expect(openObjects == 1,
           QStringLiteral("only automation.get_manifest extensions may be recursively open"));

    verifyGetOptions();
    verifyNumericDomains();
    verifyDocumentReplacementContracts();
    verifySnapshotsAndCapabilities();
    verifyPhaseTwoCapabilityContracts();
    verifyTaskSnapshotUnion();

    if (failures != 0)
        QTextStream(stderr) << failures << " public automation contract test(s) failed" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
