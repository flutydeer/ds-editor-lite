#include "../PublicAutomationToolsetExpectations.h"

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/PublicToolContract.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QTextStream>

#include <algorithm>
#include <string>

namespace {
    using namespace AutomationWire;
    using namespace PublicAutomationToolsetExpectations;

    int failures = 0;

    void expect(const bool condition, const QString &message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    QJsonObject resolveSchema(QJsonObject schema, const QJsonObject &root) {
        QSet<QString> visited;
        while (schema.contains(QStringLiteral("$ref"))) {
            const auto reference = schema.value(QStringLiteral("$ref")).toString();
            constexpr auto prefix = "#/$defs/";
            if (!reference.startsWith(QString::fromLatin1(prefix)) || visited.contains(reference))
                return {};
            visited.insert(reference);
            schema = root.value(QStringLiteral("$defs"))
                         .toObject()
                         .value(reference.sliced(
                             static_cast<qsizetype>(std::char_traits<char>::length(prefix))))
                         .toObject();
        }
        return schema;
    }

    QJsonObject propertySchema(const QJsonObject &schema, const QString &name,
                               const QJsonObject &root = {}) {
        const auto &document = root.isEmpty() ? schema : root;
        return resolveSchema(schema, document)
            .value(QStringLiteral("properties"))
            .toObject()
            .value(name)
            .toObject();
    }

    QJsonObject arrayItemSchema(const QJsonObject &schema, const QString &name) {
        return resolveSchema(resolveSchema(propertySchema(schema, name), schema)
                                 .value(QStringLiteral("items"))
                                 .toObject(),
                             schema);
    }

    QJsonObject schemaAtPath(const QJsonObject &root, const QStringList &path) {
        auto current = root;
        for (const auto &segment : path) {
            current = resolveSchema(current, root);
            current = segment == QStringLiteral("*")
                          ? current.value(QStringLiteral("items")).toObject()
                          : current.value(QStringLiteral("properties"))
                                .toObject()
                                .value(segment)
                                .toObject();
            if (current.isEmpty())
                return {};
        }
        return resolveSchema(current, root);
    }

    QSet<QString> keys(const QJsonObject &object) {
        QSet<QString> result;
        for (auto it = object.constBegin(); it != object.constEnd(); ++it)
            result.insert(it.key());
        return result;
    }

    QSet<QString> requiredFields(const QJsonObject &schema, const QJsonObject &root = {}) {
        const auto &document = root.isEmpty() ? schema : root;
        QSet<QString> result;
        for (const auto &value :
             resolveSchema(schema, document).value(QStringLiteral("required")).toArray()) {
            result.insert(value.toString());
        }
        return result;
    }

    bool hasStrictObjectRoot(const QJsonObject &schema) {
        const auto branches = schema.value(QStringLiteral("oneOf")).toArray();
        if (!branches.isEmpty()) {
            return std::all_of(branches.cbegin(), branches.cend(), [](const auto &branch) {
                const auto object = branch.toObject();
                return object.value(QStringLiteral("type")) == QStringLiteral("object") &&
                       object.value(QStringLiteral("additionalProperties")) == false;
            });
        }
        return schema.value(QStringLiteral("additionalProperties")) == false;
    }

    QJsonObject commandContext() {
        return {
            {QStringLiteral("document_id"),       QStringLiteral("00000000-0000-4000-8000-000000000001")},
            {QStringLiteral("expected_revision"), 0                                                     },
        };
    }

    void verifyAuthoritativeToolset() {
        const auto &contracts = publicToolContracts();
        const auto expectedIds = editorToolIds();
        expect(editorTools().size() == 175 && editorToolIdSet().size() == 175,
               QStringLiteral("test fixture must contain exactly 175 unique editor tools"));
        expect(contracts.size() == 175,
               QStringLiteral("public contract surface must contain exactly 175 editor tools"));
        expect(publicToolIds() == expectedIds,
               QStringLiteral("public contract order and operation set must equal section 10.3"));

        QSet<QString> actualIds;
        const auto count = std::min(contracts.size(), editorTools().size());
        for (qsizetype index = 0; index < count; ++index) {
            const auto &contract = contracts.at(index);
            const auto &expected = editorTools().at(index);
            expect(!actualIds.contains(contract.operationId),
                   QStringLiteral("duplicate public operation: ") + contract.operationId);
            actualIds.insert(contract.operationId);
            expect(contract.operationId == expected.operationId,
                   QStringLiteral("operation order differs at index %1").arg(index));
            expect(contract.category == expected.category,
                   QStringLiteral("business domain differs for %1: expected %2, found %3")
                       .arg(contract.operationId, expected.category, contract.category));
            expect(contract.minimumProfile == expected.minimumProfile,
                   QStringLiteral("minimum profile differs for ") + contract.operationId);

            const auto descriptor = contract.toMcpToolJson();
            const auto metadata = descriptor.value(QStringLiteral("_meta"))
                                      .toObject()
                                      .value(QStringLiteral("io.openvpi.ds-editor-lite/tool"))
                                      .toObject();
            expect(contract.minimumToolsetVersion == 1 &&
                       metadata.value(QStringLiteral("minimum_toolset_version")).toInteger() == 1,
                   QStringLiteral("minimum toolset version must equal one for ") +
                       contract.operationId);
            expect(contract.inputSchema.value(QStringLiteral("type")) == QStringLiteral("object") &&
                       hasStrictObjectRoot(contract.inputSchema),
                   QStringLiteral("input root must be a closed object for ") +
                       contract.operationId);
            expect(checkJsonSchema(contract.inputSchema).valid(),
                   QStringLiteral("input schema must be supported for ") + contract.operationId);
            expect(checkJsonSchema(contract.outputSchema).valid(),
                   QStringLiteral("output schema must be supported for ") + contract.operationId);
        }
        for (const auto &contract : contracts)
            actualIds.insert(contract.operationId);
        expect(actualIds == editorToolIdSet(),
               QStringLiteral("public contract operation set must be exact"));
        expect(PublicToolsetVersion == 1,
               QStringLiteral("the first public toolset version must remain one"));
        expect(toolsForProfile(AutomationProfile::L0).size() == 2 &&
                   toolsForProfile(AutomationProfile::L1).size() == 87 &&
                   toolsForProfile(AutomationProfile::L2).size() == 130 &&
                   toolsForProfile(AutomationProfile::L3).size() == 175,
               QStringLiteral("editor profile counts must be 2/87/130/175"));
    }

    void verifyShallowCreationAndNoteDefaults() {
        const auto *tracks = findPublicTool(QStringLiteral("tracks.insert"));
        const auto *clips = findPublicTool(QStringLiteral("clips.insert"));
        const auto *notes = findPublicTool(QStringLiteral("notes.insert"));
        expect(tracks && clips && notes,
               QStringLiteral("track, clip, and note creation contracts must exist"));
        if (!tracks || !clips || !notes)
            return;

        const auto track = arrayItemSchema(tracks->inputSchema, QStringLiteral("tracks"));
        expect(keys(track.value(QStringLiteral("properties")).toObject()) ==
                       QSet<QString>{QStringLiteral("client_ref"), QStringLiteral("name"),
                                     QStringLiteral("color_index")} &&
                   requiredFields(track, tracks->inputSchema).isEmpty() &&
                   track.value(QStringLiteral("additionalProperties")) == false,
               QStringLiteral("TrackCreate must only expose optional client_ref/name/color_index"));
        auto minimalTrack = commandContext();
        minimalTrack.insert(QStringLiteral("index"), 0);
        minimalTrack.insert(QStringLiteral("tracks"), QJsonArray{QJsonObject{}});
        expect(validateJsonValue(minimalTrack, tracks->inputSchema).valid(),
               QStringLiteral("tracks.insert must accept a shallow default track"));
        auto nestedTrack = minimalTrack;
        nestedTrack.insert(QStringLiteral("tracks"),
                           QJsonArray{
                               QJsonObject{{QStringLiteral("clips"), QJsonArray{}},
                                           {QStringLiteral("voice"), QJsonObject{}}}
        });
        expect(!validateJsonValue(nestedTrack, tracks->inputSchema).valid(),
               QStringLiteral("tracks.insert must reject clips and voice object injection"));

        const auto clip = arrayItemSchema(clips->inputSchema, QStringLiteral("clips"));
        expect(keys(clip.value(QStringLiteral("properties")).toObject()) ==
                       QSet<QString>{QStringLiteral("client_ref"), QStringLiteral("track_id"),
                                     QStringLiteral("start"), QStringLiteral("length"),
                                     QStringLiteral("name")} &&
                   requiredFields(clip, clips->inputSchema) ==
                       QSet<QString>{QStringLiteral("track_id"), QStringLiteral("start")} &&
                   clip.value(QStringLiteral("additionalProperties")) == false,
               QStringLiteral("SingingClipCreate must remain a shallow empty-clip draft"));
        auto minimalClip = commandContext();
        minimalClip.insert(QStringLiteral("clips"), QJsonArray{
                                                        QJsonObject{{QStringLiteral("track_id"), 1},
                                                                    {QStringLiteral("start"), 0}}
        });
        expect(validateJsonValue(minimalClip, clips->inputSchema).valid(),
               QStringLiteral("clips.insert must accept GUI-equivalent default length and name"));
        auto nestedClip = minimalClip;
        nestedClip.insert(QStringLiteral("clips"),
                          QJsonArray{
                              QJsonObject{{QStringLiteral("track_id"), 1},
                                          {QStringLiteral("start"), 0},
                                          {QStringLiteral("notes"), QJsonArray{}},
                                          {QStringLiteral("parameters"), QJsonArray{}},
                                          {QStringLiteral("speaker_mix"), QJsonObject{}}}
        });
        expect(!validateJsonValue(nestedClip, clips->inputSchema).valid(),
               QStringLiteral("clips.insert must reject nested editing trees"));

        const auto note = arrayItemSchema(notes->inputSchema, QStringLiteral("notes"));
        const auto noteRequired = requiredFields(note, notes->inputSchema);
        expect(
            noteRequired == QSet<QString>{QStringLiteral("local_start"), QStringLiteral("length"),
                                          QStringLiteral("key_index")} &&
                !note.value(QStringLiteral("properties"))
                     .toObject()
                     .contains(QStringLiteral("voice_context")) &&
                note.value(QStringLiteral("additionalProperties")) == false,
            QStringLiteral("NoteDraft must require only geometry and pitch without voice_context"));
        auto minimalNote = commandContext();
        minimalNote.insert(QStringLiteral("clip_id"), 1);
        minimalNote.insert(QStringLiteral("notes"),
                           QJsonArray{
                               QJsonObject{{QStringLiteral("local_start"), 0},
                                           {QStringLiteral("length"), 480},
                                           {QStringLiteral("key_index"), 60}}
        });
        expect(validateJsonValue(minimalNote, notes->inputSchema).valid(),
               QStringLiteral("notes.insert must accept server-resolved lyric/language defaults"));
    }

    void verifyLifecycleIdempotencyContracts() {
        for (const auto &operationId :
             {QStringLiteral("documents.new"), QStringLiteral("documents.open")}) {
            const auto *contract = findPublicTool(operationId);
            expect(contract, operationId + QStringLiteral(" must exist"));
            if (!contract)
                continue;
            const auto properties =
                contract->inputSchema.value(QStringLiteral("properties")).toObject();
            expect(!properties.contains(QStringLiteral("idempotency_key")),
                   operationId +
                       QStringLiteral(" must not advertise unsupported replacement idempotency"));

            QJsonObject valid{
                {QStringLiteral("unsaved_policy"), QStringLiteral("discard")},
            };
            if (operationId == QStringLiteral("documents.open"))
                valid.insert(QStringLiteral("path"), QStringLiteral("C:/fixture.mid"));
            auto withIdempotencyKey = valid;
            withIdempotencyKey.insert(QStringLiteral("idempotency_key"),
                                      QStringLiteral("unsupported-lifecycle-key"));
            expect(validateJsonValue(valid, contract->inputSchema).valid() &&
                       !validateJsonValue(withIdempotencyKey, contract->inputSchema).valid(),
                   operationId +
                       QStringLiteral(" schema must reject unsupported idempotency_key input"));
        }
    }

    void verifyOmissionEquivalentOptionalInputs() {
        const QList<QPair<QString, QStringList>> emptyStringCases{
            {QStringLiteral("tracks.list"),              {QStringLiteral("cursor")}       },
            {QStringLiteral("clips.list"),               {QStringLiteral("type")}         },
            {QStringLiteral("clips.list"),               {QStringLiteral("cursor")}       },
            {QStringLiteral("voices.list"),              {QStringLiteral("query")}        },
            {QStringLiteral("voices.list"),              {QStringLiteral("package_id")}   },
            {QStringLiteral("voices.list"),              {QStringLiteral("cursor")}       },
            {QStringLiteral("notes.list"),               {QStringLiteral("cursor")}       },
            {QStringLiteral("tasks.list"),               {QStringLiteral("state")}        },
            {QStringLiteral("tasks.list"),               {QStringLiteral("kind")}         },
            {QStringLiteral("tasks.list"),               {QStringLiteral("cursor")}       },
            {QStringLiteral("packages.list"),            {QStringLiteral("cursor")}       },
            {QStringLiteral("formats.list"),             {QStringLiteral("purpose")}      },
            {QStringLiteral("lyric_rules.list"),         {QStringLiteral("kind")}         },
            {QStringLiteral("documents.open"),           {QStringLiteral("format_id")}    },
            {QStringLiteral("documents.open"),           {QStringLiteral("plan_digest")}  },
            {QStringLiteral("documents.open"),
             {QStringLiteral("options"), QStringLiteral("encoding")}                      },
            {QStringLiteral("documents.import"),         {QStringLiteral("format_id")}    },
            {QStringLiteral("documents.import"),         {QStringLiteral("plan_digest")}  },
            {QStringLiteral("documents.import"),
             {QStringLiteral("options"), QStringLiteral("encoding")}                      },
            {QStringLiteral("documents.import_batch"),
             {QStringLiteral("items"), QStringLiteral("*"), QStringLiteral("format_id")}  },
            {QStringLiteral("documents.import_batch"),
             {QStringLiteral("items"), QStringLiteral("*"), QStringLiteral("plan_digest")}},
            {QStringLiteral("documents.import_batch"),
             {QStringLiteral("items"), QStringLiteral("*"), QStringLiteral("options"),
              QStringLiteral("encoding")}                                                 },
            {QStringLiteral("audio_clips.confirm_path"), {QStringLiteral("path")}         },
            {QStringLiteral("packages.describe"),        {QStringLiteral("version")}      },
            {QStringLiteral("extract.pitch.start"),
             {QStringLiteral("options"), QStringLiteral("model_id")}                      },
            {QStringLiteral("extract.midi.start"),
             {QStringLiteral("options"), QStringLiteral("model_id")}                      },
            {QStringLiteral("extract.midi.start"),
             {QStringLiteral("options"), QStringLiteral("default_language")}              },
            {QStringLiteral("inference.start"),
             {QStringLiteral("options"), QStringLiteral("provider_id")}                   },
            {QStringLiteral("inference.start"),
             {QStringLiteral("options"), QStringLiteral("device_id")}                     },
            {QStringLiteral("inference.start"),
             {QStringLiteral("options"), QStringLiteral("model_id")}                      },
        };
        const QList<QPair<QString, QStringList>> emptyArrayCases{
            {QStringLiteral("settings.query"),       {QStringLiteral("domains")}},
            {QStringLiteral("inference.start"),      {QStringLiteral("stages")} },
            {QStringLiteral("exports.midi.preview"),
             {QStringLiteral("options"), QStringLiteral("track_ids")}           },
            {QStringLiteral("exports.midi.preview"),
             {QStringLiteral("options"), QStringLiteral("clip_ids")}            },
            {QStringLiteral("exports.midi.start"),
             {QStringLiteral("options"), QStringLiteral("track_ids")}           },
            {QStringLiteral("exports.midi.start"),
             {QStringLiteral("options"), QStringLiteral("clip_ids")}            },
        };

        const auto verifyCase = [](const QPair<QString, QStringList> &testCase,
                                   const QJsonValue &emptyValue) {
            const auto *contract = findPublicTool(testCase.first);
            expect(contract, testCase.first + QStringLiteral(" must exist"));
            if (!contract || testCase.second.isEmpty())
                return;
            auto parentPath = testCase.second;
            const auto field = parentPath.takeLast();
            const auto parentSchema = schemaAtPath(contract->inputSchema, parentPath);
            const auto fieldSchema = schemaAtPath(contract->inputSchema, testCase.second);
            expect(!parentSchema.isEmpty() && !fieldSchema.isEmpty() &&
                       !requiredFields(parentSchema, contract->inputSchema).contains(field) &&
                       validateJsonValue(emptyValue, fieldSchema).valid(),
                   testCase.first + u'.' + testCase.second.join(u'.') +
                       QStringLiteral(" must be optional and treat an empty value as omitted"));
        };
        for (const auto &testCase : emptyStringCases)
            verifyCase(testCase, QString());
        for (const auto &testCase : emptyArrayCases)
            verifyCase(testCase, QJsonArray{});

        for (const auto &testCase : QList<QPair<QString, QStringList>>{
                 {QStringLiteral("notes.search"),      {QStringLiteral("query")}     },
                 {QStringLiteral("documents.open"),    {QStringLiteral("path")}      },
                 {QStringLiteral("formats.inspect"),   {QStringLiteral("purpose")}   },
                 {QStringLiteral("packages.describe"), {QStringLiteral("package_id")}},
        }) {
            const auto *contract = findPublicTool(testCase.first);
            expect(contract && !validateJsonValue(
                                    QString(), schemaAtPath(contract->inputSchema, testCase.second))
                                    .valid(),
                   testCase.first + u'.' + testCase.second.join(u'.') +
                       QStringLiteral(" must remain non-empty"));
        }
        for (const auto &testCase : QList<QPair<QString, QStringList>>{
                 {QStringLiteral("clips.list"),       {QStringLiteral("type")}   },
                 {QStringLiteral("tasks.list"),       {QStringLiteral("state")}  },
                 {QStringLiteral("formats.list"),     {QStringLiteral("purpose")}},
                 {QStringLiteral("lyric_rules.list"), {QStringLiteral("kind")}   },
        }) {
            const auto *contract = findPublicTool(testCase.first);
            expect(contract &&
                       !validateJsonValue(QStringLiteral("not-a-valid-option"),
                                          schemaAtPath(contract->inputSchema, testCase.second))
                            .valid(),
                   testCase.first + u'.' + testCase.second.join(u'.') +
                       QStringLiteral(" must still reject unknown non-empty filters"));
        }
    }

    void verifyScalarMutation(const QString &operationId, const QString &targetField,
                              const QString &valueField) {
        const auto *contract = findPublicTool(operationId);
        expect(contract, operationId + QStringLiteral(" must exist"));
        if (!contract)
            return;
        const auto properties =
            contract->inputSchema.value(QStringLiteral("properties")).toObject();
        QSet<QString> expected{
            QStringLiteral("document_id"),
            QStringLiteral("expected_revision"),
            valueField,
        };
        if (!targetField.isEmpty())
            expected.insert(targetField);
        expect(keys(properties) == expected,
               operationId + QStringLiteral(" must expose one independent scalar edit"));
    }

    void verifyIndependentScalarOperations() {
        verifyScalarMutation(QStringLiteral("tracks.rename"), QStringLiteral("track_id"),
                             QStringLiteral("name"));
        verifyScalarMutation(QStringLiteral("tracks.set_color"), QStringLiteral("track_id"),
                             QStringLiteral("color_index"));
        verifyScalarMutation(QStringLiteral("tracks.set_gain"), QStringLiteral("track_id"),
                             QStringLiteral("gain"));
        verifyScalarMutation(QStringLiteral("tracks.set_pan"), QStringLiteral("track_id"),
                             QStringLiteral("pan"));
        verifyScalarMutation(QStringLiteral("tracks.set_mute"), QStringLiteral("track_id"),
                             QStringLiteral("mute"));
        verifyScalarMutation(QStringLiteral("tracks.set_solo"), QStringLiteral("track_id"),
                             QStringLiteral("solo"));
        verifyScalarMutation(QStringLiteral("tracks.set_default_language"),
                             QStringLiteral("track_id"), QStringLiteral("language_id"));
        verifyScalarMutation(QStringLiteral("clips.rename"), QStringLiteral("clip_id"),
                             QStringLiteral("name"));
        verifyScalarMutation(QStringLiteral("clips.set_gain"), QStringLiteral("clip_id"),
                             QStringLiteral("gain"));
        verifyScalarMutation(QStringLiteral("clips.set_mute"), QStringLiteral("clip_id"),
                             QStringLiteral("mute"));
        verifyScalarMutation(QStringLiteral("clips.set_default_language"),
                             QStringLiteral("clip_id"), QStringLiteral("language_id"));
        verifyScalarMutation(QStringLiteral("master.set_gain"), {}, QStringLiteral("gain"));
        verifyScalarMutation(QStringLiteral("master.set_pan"), {}, QStringLiteral("pan"));
        verifyScalarMutation(QStringLiteral("master.set_mute"), {}, QStringLiteral("mute"));
        verifyScalarMutation(QStringLiteral("master.set_solo"), {}, QStringLiteral("solo"));

        for (const auto &removed :
             {QStringLiteral("tracks.set_properties"), QStringLiteral("clips.set_properties"),
              QStringLiteral("master.set_control"), QStringLiteral("notes.set_word_properties")}) {
            expect(findPublicTool(removed) == nullptr,
                   removed + QStringLiteral(" must not collapse independent History edits"));
        }
    }

    void verifyVoiceContracts() {
        for (const auto &operation :
             {QStringLiteral("tracks.set_voice"), QStringLiteral("tracks.clear_voice")}) {
            const auto *contract = findPublicTool(operation);
            expect(contract && contract->category == QStringLiteral("tracks"),
                   operation + QStringLiteral(" must belong to the track domain"));
        }
        for (const auto &operation :
             {QStringLiteral("clips.use_track_voice"), QStringLiteral("clips.set_voice"),
              QStringLiteral("clips.clear_voice")}) {
            const auto *contract = findPublicTool(operation);
            expect(contract && contract->category == QStringLiteral("clips"),
                   operation + QStringLiteral(" must belong to the clip domain"));
        }
        expect(findPublicTool(QStringLiteral("tracks.get_voice_context")) == nullptr &&
                   findPublicTool(QStringLiteral("clips.get_voice_context")) == nullptr,
               QStringLiteral("voice context must be part of track and clip detail snapshots"));
        for (const auto &operation : {QStringLiteral("tracks.get"), QStringLiteral("clips.get")}) {
            const auto *contract = findPublicTool(operation);
            const auto snapshot = contract
                                      ? resolveSchema(propertySchema(contract->outputSchema,
                                                                     QStringLiteral("snapshot")),
                                                      contract->outputSchema)
                                      : QJsonObject{};
            const auto voiceContext =
                propertySchema(snapshot, QStringLiteral("voice_context"),
                               contract ? contract->outputSchema : QJsonObject{});
            expect(contract &&
                       requiredFields(snapshot, contract->outputSchema)
                           .contains(QStringLiteral("voice_context")) &&
                       !voiceContext.isEmpty(),
                   operation + QStringLiteral(" must return voice_context in its detail snapshot"));
        }

        const auto checkVoice = [](const QString &operation) {
            const auto *contract = findPublicTool(operation);
            if (!contract)
                return false;
            const auto voice =
                resolveSchema(propertySchema(contract->inputSchema, QStringLiteral("voice")),
                              contract->inputSchema);
            const auto singer = resolveSchema(
                propertySchema(voice, QStringLiteral("singer"), contract->inputSchema),
                contract->inputSchema);
            const auto speaker = resolveSchema(
                propertySchema(voice, QStringLiteral("speaker"), contract->inputSchema),
                contract->inputSchema);
            auto singerOnly = commandContext();
            singerOnly.insert(operation.startsWith(QStringLiteral("tracks."))
                                  ? QStringLiteral("track_id")
                                  : QStringLiteral("clip_id"),
                              1);
            singerOnly.insert(QStringLiteral("voice"),
                              QJsonObject{
                                  {QStringLiteral("singer"),
                                   QJsonObject{
                                       {QStringLiteral("package_id"), QStringLiteral("package")},
                                       {QStringLiteral("package_version"), QStringLiteral("1.0")},
                                       {QStringLiteral("singer_id"), QStringLiteral("singer")},
                                   }},
            });
            return requiredFields(voice, contract->inputSchema) ==
                       QSet<QString>{QStringLiteral("singer")} &&
                   requiredFields(singer, contract->inputSchema) ==
                       QSet<QString>{QStringLiteral("package_id"),
                                     QStringLiteral("package_version"),
                                     QStringLiteral("singer_id")} &&
                   speaker.value(QStringLiteral("oneOf")).toArray().size() == 2 &&
                   validateJsonValue(singerOnly, contract->inputSchema).valid();
        };
        expect(checkVoice(QStringLiteral("tracks.set_voice")) &&
                   checkVoice(QStringLiteral("clips.set_voice")),
               QStringLiteral(
                   "voice selection must accept a SingerRef when the voice has no speakers"));
    }

    void verifySpeakerMixContracts() {
        const QStringList operations{
            QStringLiteral("speaker_mix.get"),
            QStringLiteral("speaker_mix.set_fixed"),
            QStringLiteral("speaker_mix.enable_dynamic"),
            QStringLiteral("speaker_mix.disable_dynamic"),
            QStringLiteral("speaker_mix.set_dynamic_bypass"),
            QStringLiteral("speaker_mix.keyframes.insert"),
            QStringLiteral("speaker_mix.keyframes.move"),
            QStringLiteral("speaker_mix.keyframes.set_weights"),
            QStringLiteral("speaker_mix.keyframes.remove"),
            QStringLiteral("speaker_mix.presets.list"),
            QStringLiteral("speaker_mix.presets.save"),
            QStringLiteral("speaker_mix.presets.delete"),
            QStringLiteral("speaker_mix.presets.apply"),
        };
        for (const auto &operation : operations) {
            const auto *contract = findPublicTool(operation);
            expect(contract && contract->category == QStringLiteral("speaker_mix"),
                   operation + QStringLiteral(" must exist in the Speaker Mix domain"));
        }

        const auto *enable = findPublicTool(QStringLiteral("speaker_mix.enable_dynamic"));
        const auto *insert = findPublicTool(QStringLiteral("speaker_mix.keyframes.insert"));
        const auto *move = findPublicTool(QStringLiteral("speaker_mix.keyframes.move"));
        const auto *setWeights =
            findPublicTool(QStringLiteral("speaker_mix.keyframes.set_weights"));
        const auto *remove = findPublicTool(QStringLiteral("speaker_mix.keyframes.remove"));
        if (!enable || !insert || !move || !setWeights || !remove)
            return;
        expect(!enable->inputSchema.value(QStringLiteral("properties"))
                       .toObject()
                       .contains(QStringLiteral("mix")) &&
                   requiredFields(enable->inputSchema).contains(QStringLiteral("clip_id")),
               QStringLiteral("enable_dynamic must derive its initial mix from current state"));
        expect(
            !requiredFields(insert->inputSchema).contains(QStringLiteral("weights")),
            QStringLiteral("keyframe insertion must support GUI-equivalent interpolated weights"));
        expect(move->inputSchema.value(QStringLiteral("properties"))
                       .toObject()
                       .contains(QStringLiteral("moves")) &&
                   setWeights->inputSchema.value(QStringLiteral("properties"))
                       .toObject()
                       .contains(QStringLiteral("keyframe_id")) &&
                   remove->inputSchema.value(QStringLiteral("properties"))
                       .toObject()
                       .contains(QStringLiteral("keyframe_ids")),
               QStringLiteral("dynamic keyframe edits must use stable IDs and atomic batches"));

        const auto *get = findPublicTool(QStringLiteral("speaker_mix.get"));
        const auto *presetList = findPublicTool(QStringLiteral("speaker_mix.presets.list"));
        const auto *presetSave = findPublicTool(QStringLiteral("speaker_mix.presets.save"));
        const auto *presetDelete = findPublicTool(QStringLiteral("speaker_mix.presets.delete"));
        const auto *presetApply = findPublicTool(QStringLiteral("speaker_mix.presets.apply"));
        expect(get && presetList && presetSave && presetDelete && presetApply,
               QStringLiteral("Speaker Mix preset contracts must exist"));
        if (!get || !presetList || !presetSave || !presetDelete || !presetApply)
            return;
        const auto speakerMixSnapshot = resolveSchema(
            propertySchema(get->outputSchema, QStringLiteral("snapshot")), get->outputSchema);
        expect(speakerMixSnapshot.value(QStringLiteral("properties"))
                   .toObject()
                   .contains(QStringLiteral("source_preset")),
               QStringLiteral("speaker_mix.get must expose nullable preset-source metadata"));
        for (const auto *presetContract : {presetList, presetSave, presetDelete, presetApply}) {
            expect(presetContract->minimumProfile == AutomationProfile::L2,
                   presetContract->operationId + QStringLiteral(" must start at L2"));
        }
    }

    void verifyDocumentAndParameterContracts() {
        const auto *documentGet = findPublicTool(QStringLiteral("documents.get"));
        const auto *recent = findPublicTool(QStringLiteral("documents.list_recent"));
        expect(documentGet && recent && findPublicTool(QStringLiteral("project.get")) == nullptr,
               QStringLiteral("document queries must replace the redundant project snapshot"));
        if (documentGet && recent) {
            const auto snapshot =
                resolveSchema(propertySchema(documentGet->outputSchema, QStringLiteral("snapshot")),
                              documentGet->outputSchema);
            const auto statistics = resolveSchema(
                propertySchema(snapshot, QStringLiteral("statistics"), documentGet->outputSchema),
                documentGet->outputSchema);
            const QSet<QString> statisticFields{
                QStringLiteral("length_ticks"),
                QStringLiteral("track_count"),
                QStringLiteral("empty_track_count"),
                QStringLiteral("singing_only_track_count"),
                QStringLiteral("audio_only_track_count"),
                QStringLiteral("mixed_track_count"),
                QStringLiteral("clip_count"),
                QStringLiteral("singing_clip_count"),
                QStringLiteral("audio_clip_count"),
            };
            expect(requiredFields(statistics, documentGet->outputSchema) == statisticFields &&
                       recent->minimumProfile == AutomationProfile::L2 &&
                       !recent->inputSchema.value(QStringLiteral("properties"))
                            .toObject()
                            .contains(QStringLiteral("document_id")),
                   QStringLiteral("documents.get must own statistics and recent projects must be "
                                  "application-scoped"));
        }

        const auto *get = findPublicTool(QStringLiteral("parameters.get"));
        const auto *create = findPublicTool(QStringLiteral("parameters.create_anchor_curve"));
        const auto *insert = findPublicTool(QStringLiteral("parameters.insert_anchors"));
        const auto *merge = findPublicTool(QStringLiteral("parameters.merge_anchor_curves"));
        expect(get && create && insert && merge,
               QStringLiteral("bounded parameter and explicit anchor-curve contracts must exist"));
        if (!get || !create || !insert || !merge)
            return;
        const auto getProperties = get->inputSchema.value(QStringLiteral("properties")).toObject();
        const auto getSnapshot = resolveSchema(
            propertySchema(get->outputSchema, QStringLiteral("snapshot")), get->outputSchema);
        const auto getSnapshotProperties =
            getSnapshot.value(QStringLiteral("properties")).toObject();
        const auto createProperties =
            create->inputSchema.value(QStringLiteral("properties")).toObject();
        const auto createAnchors = createProperties.value(QStringLiteral("anchors")).toObject();
        expect(
            getProperties.contains(QStringLiteral("range")) &&
                getProperties.contains(QStringLiteral("max_points")) &&
                getSnapshotProperties.contains(QStringLiteral("source_point_count")) &&
                getSnapshotProperties.contains(QStringLiteral("returned_point_count")) &&
                getSnapshotProperties.contains(QStringLiteral("downsampled")) &&
                createAnchors.value(QStringLiteral("minItems")).toInteger() == 2 &&
                requiredFields(create->inputSchema).contains(QStringLiteral("client_ref")) &&
                requiredFields(insert->inputSchema).contains(QStringLiteral("curve_id")) &&
                requiredFields(merge->inputSchema).contains(QStringLiteral("target_curve_id")) &&
                requiredFields(merge->inputSchema).contains(QStringLiteral("source_curve_id")),
            QStringLiteral("parameter reads must be bounded and anchor topology must be explicit"));
    }

    void verifyConditionalCapabilityContracts() {
        const auto *audioPreview = findPublicTool(QStringLiteral("exports.audio.preview"));
        const auto *midiExtraction = findPublicTool(QStringLiteral("extract.midi.start"));
        const auto *fillLyrics = findPublicTool(QStringLiteral("notes.fill_lyrics"));
        const auto *audioGet = findPublicTool(QStringLiteral("audio_clips.get"));
        expect(audioPreview && midiExtraction && fillLyrics && audioGet,
               QStringLiteral("conditional capability contracts must exist"));
        if (!audioPreview || !midiExtraction || !fillLyrics || !audioGet)
            return;

        const auto audioOptions =
            propertySchema(audioPreview->inputSchema, QStringLiteral("options"));
        const auto audioBranches = audioOptions.value(QStringLiteral("oneOf")).toArray();
        const bool audioRangeAbsent =
            std::all_of(audioBranches.begin(), audioBranches.end(), [](const QJsonValue &branch) {
                return !branch.toObject()
                            .value(QStringLiteral("properties"))
                            .toObject()
                            .contains(QStringLiteral("range"));
            });
        expect(audioBranches.size() == 2 && audioRangeAbsent,
               QStringLiteral("audio export options must use two closed source-mode branches"));

        const auto destination =
            propertySchema(midiExtraction->inputSchema, QStringLiteral("destination"));
        expect(destination.value(QStringLiteral("oneOf")).toArray().size() == 2,
               QStringLiteral("MIDI extraction destination must be discriminated by mode"));

        bool hasFillLanguageSource = false;
        for (const auto &value : fillLyrics->valueSources) {
            const auto source = value.toObject();
            hasFillLanguageSource |= source.value(QStringLiteral("field_path")).toString() ==
                                         QStringLiteral("/options/language/language_id") &&
                                     source.value(QStringLiteral("operation_id")).toString() ==
                                         QStringLiteral("voices.describe");
        }
        expect(
            hasFillLanguageSource,
            QStringLiteral("explicit fill-lyrics language must use the voice capability source"));

        const QJsonObject unknownAudioSnapshot{
            {QStringLiteral("document"),
             QJsonObject{
                 {QStringLiteral("document_id"),
                  QStringLiteral("00000000-0000-4000-8000-000000000001")},
                 {QStringLiteral("revision"), 0},
             }},
            {QStringLiteral("snapshot"),
             QJsonObject{
                 {QStringLiteral("clip_id"), 1},
                 {QStringLiteral("path"), QString()},
                 {QStringLiteral("path_status"), QStringLiteral("missing")},
                 {QStringLiteral("candidate_paths"), QJsonArray{}},
                 {QStringLiteral("hash_exists"), false},
                 {QStringLiteral("duration_seconds"), QJsonValue(QJsonValue::Null)},
                 {QStringLiteral("sample_rate"), QJsonValue(QJsonValue::Null)},
                 {QStringLiteral("channels"), QJsonValue(QJsonValue::Null)},
             }},
        };
        expect(validateJsonValue(unknownAudioSnapshot, audioGet->outputSchema).valid(),
               QStringLiteral("undecoded audio metadata must be representable as null"));
    }

    void verifyPlaybackPersistenceContracts() {
        const QStringList persistentOperations{
            QStringLiteral("playback.set_loop"),
            QStringLiteral("playback.set_loop_enabled"),
            QStringLiteral("playback.clear_loop"),
        };
        const QSet<QString> mutationOutputFields{
            QStringLiteral("previous"),         QStringLiteral("current"),
            QStringLiteral("changed"),          QStringLiteral("validated_only"),
            QStringLiteral("affected_objects"), QStringLiteral("created_objects"),
            QStringLiteral("resolved_values"),  QStringLiteral("presentation_effects"),
            QStringLiteral("warnings"),         QStringLiteral("state_version"),
            QStringLiteral("playback"),
        };
        for (const auto &operation : persistentOperations) {
            const auto *contract = findPublicTool(operation);
            expect(contract, operation + QStringLiteral(" must exist"));
            if (!contract)
                continue;
            const auto inputProperties =
                contract->inputSchema.value(QStringLiteral("properties")).toObject();
            const auto inputRequired = requiredFields(contract->inputSchema);
            expect(inputProperties.contains(QStringLiteral("document_id")) &&
                       inputProperties.contains(QStringLiteral("expected_revision")) &&
                       inputProperties.contains(QStringLiteral("expected_state_version")) &&
                       !inputProperties.contains(QStringLiteral("validate_only")) &&
                       !inputProperties.contains(QStringLiteral("idempotency_key")) &&
                       inputRequired.contains(QStringLiteral("document_id")) &&
                       inputRequired.contains(QStringLiteral("expected_revision")) &&
                       !inputRequired.contains(QStringLiteral("expected_state_version")),
                   operation +
                       QStringLiteral(
                           " must require document revision and optionally check playback state"));
            expect(keys(contract->outputSchema.value(QStringLiteral("properties")).toObject()) ==
                           mutationOutputFields &&
                       requiredFields(contract->outputSchema) == mutationOutputFields,
                   operation + QStringLiteral(
                                   " must return document Mutation and playback snapshot fields"));
        }

        const auto *setLoop = findPublicTool(QStringLiteral("playback.set_loop"));
        if (setLoop) {
            const auto properties =
                setLoop->inputSchema.value(QStringLiteral("properties")).toObject();
            expect(properties.value(QStringLiteral("start"))
                               .toObject()
                               .value(QStringLiteral("type")) == QStringLiteral("integer") &&
                       properties.value(QStringLiteral("end"))
                               .toObject()
                               .value(QStringLiteral("type")) == QStringLiteral("integer"),
                   QStringLiteral("playback loop boundaries must be integer ticks"));
            auto valid = commandContext();
            valid.insert(QStringLiteral("start"), 120);
            valid.insert(QStringLiteral("end"), 960);
            auto fractional = valid;
            fractional.insert(QStringLiteral("start"), 120.5);
            expect(validateJsonValue(valid, setLoop->inputSchema).valid() &&
                       !validateJsonValue(fractional, setLoop->inputSchema).valid(),
                   QStringLiteral("playback.set_loop must reject fractional ticks"));
        }

        const auto *seek = findPublicTool(QStringLiteral("playback.seek"));
        if (seek) {
            const auto properties =
                seek->inputSchema.value(QStringLiteral("properties")).toObject();
            expect(!properties.contains(QStringLiteral("expected_revision")) &&
                       !properties.contains(QStringLiteral("idempotency_key")) &&
                       properties.contains(QStringLiteral("expected_state_version")) &&
                       !seek->outputSchema.value(QStringLiteral("properties"))
                            .toObject()
                            .contains(QStringLiteral("previous")),
                   QStringLiteral("seek must remain a non-persistent playback state operation"));
        }
    }

    void verifyAdvancedControlContracts() {
        const auto l3Tools = toolsForProfile(AutomationProfile::L3).mid(130);
        int queryCount = 0;
        int synchronousCommandCount = 0;
        int asynchronousCommandCount = 0;
        for (const auto &tool : l3Tools) {
            queryCount += tool.kind == OperationKind::Query;
            synchronousCommandCount +=
                tool.kind == OperationKind::Command && tool.syncMode == SyncMode::Synchronous;
            asynchronousCommandCount +=
                tool.kind == OperationKind::Command && tool.syncMode == SyncMode::Asynchronous;
            if (tool.operationId.startsWith(QStringLiteral("workspace.")) ||
                tool.operationId.startsWith(QStringLiteral("track_panel.")) ||
                tool.operationId.startsWith(QStringLiteral("clip_editor."))) {
                expect(tool.hostAvailability == QStringLiteral("gui") &&
                           (tool.inputSchema.value(QStringLiteral("properties"))
                                .toObject()
                                .contains(QStringLiteral("window_id")) ||
                            !tool.inputSchema.value(QStringLiteral("oneOf")).toArray().isEmpty()),
                       tool.operationId +
                           QStringLiteral(" must be GUI-only and identify an explicit window"));
            } else {
                expect(tool.hostAvailability == QStringLiteral("both"),
                       tool.operationId + QStringLiteral(" must be available in both hosts"));
            }
        }
        expect(l3Tools.size() == 45 && queryCount == 8 && synchronousCommandCount == 36 &&
                   asynchronousCommandCount == 1,
               QStringLiteral("L3 must contain exactly 8 Q/S, 36 C/S, and 1 C/A tools"));

        const auto *parameterTool =
            findPublicTool(QStringLiteral("clip_editor.parameters.set_foreground"));
        expect(parameterTool && requiredFields(parameterTool->inputSchema)
                                    .contains(QStringLiteral("expected_revision")),
               QStringLiteral("document-bound parameter GUI actions must check object freshness"));

        const auto *revealClips = findPublicTool(QStringLiteral("track_panel.reveal_clips"));
        bool hasTrackSource = false;
        bool hasClipSource = false;
        if (revealClips) {
            for (const auto &value : revealClips->valueSources) {
                const auto source = value.toObject();
                const auto fieldPath = source.value(QStringLiteral("field_path")).toString();
                const auto operationId = source.value(QStringLiteral("operation_id")).toString();
                hasTrackSource |= fieldPath == QStringLiteral("/track_id") &&
                                  operationId == QStringLiteral("tracks.list");
                hasClipSource |= fieldPath == QStringLiteral("/clip_ids/*") &&
                                 operationId == QStringLiteral("clips.list");
            }
        }
        expect(hasTrackSource && hasClipSource,
               QStringLiteral("track_panel.reveal_clips must expose both branch value sources"));

        const auto *refresh = findPublicTool(QStringLiteral("packages.refresh"));
        const QJsonObject accepted{
            {QStringLiteral("task_id"),        QStringLiteral("00000000-0000-4000-8000-000000000002")},
            {QStringLiteral("scope"),          QStringLiteral("application")                         },
            {QStringLiteral("document"),       QJsonValue::Null                                      },
            {QStringLiteral("validated_only"), false                                                 },
        };
        auto fakeDocument = accepted;
        fakeDocument.insert(QStringLiteral("document"), commandContext());
        expect(refresh && validateJsonValue(accepted, refresh->outputSchema).valid() &&
                   !validateJsonValue(fakeDocument, refresh->outputSchema).valid(),
               QStringLiteral(
                   "packages.refresh must accept an application task without fake document"));

        const auto *packages = findPublicTool(QStringLiteral("packages.list"));
        const QJsonObject redactedPackage{
            {QStringLiteral("package_id"),     QStringLiteral("fixture")},
            {QStringLiteral("name"),           QStringLiteral("Fixture")},
            {QStringLiteral("version"),        QStringLiteral("1")      },
            {QStringLiteral("vendor"),         QStringLiteral("Test")   },
            {QStringLiteral("canonical_path"), QJsonValue::Null         },
            {QStringLiteral("voices"),         QJsonArray{}             },
        };
        expect(packages && validateJsonValue(
                               QJsonObject{
                                   {QStringLiteral("packages"), QJsonArray{redactedPackage}}
        },
                               packages->outputSchema)
                               .valid(),
               QStringLiteral(
                   "package discovery must preserve entries while redacting guarded paths"));

        const auto *createRule = findPublicTool(QStringLiteral("lyric_rules.create"));
        const QJsonObject validatedRule{
            {QStringLiteral("kind"),          QStringLiteral("splitter")        },
            {QStringLiteral("name"),          QStringLiteral("fixture")         },
            {QStringLiteral("regexes"),       QJsonArray{QStringLiteral("\\s+")}},
            {QStringLiteral("validate_only"), true                              },
        };
        expect(createRule && validateJsonValue(validatedRule, createRule->inputSchema).valid(),
               QStringLiteral("lyric rule creation must support side-effect-free validation"));

        const auto *tasksGet = findPublicTool(QStringLiteral("tasks.get"));
        const QJsonObject applicationTask{
            {QStringLiteral("scope"),   QStringLiteral("application")                         },
            {QStringLiteral("task_id"), QStringLiteral("00000000-0000-4000-8000-000000000002")},
        };
        auto invalidApplicationTask = applicationTask;
        invalidApplicationTask.insert(QStringLiteral("document_id"),
                                      QStringLiteral("00000000-0000-4000-8000-000000000001"));
        expect(tasksGet && validateJsonValue(applicationTask, tasksGet->inputSchema).valid() &&
                   !validateJsonValue(invalidApplicationTask, tasksGet->inputSchema).valid(),
               QStringLiteral("task lookup scope must determine whether document_id is legal"));
    }
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    verifyAuthoritativeToolset();
    verifyShallowCreationAndNoteDefaults();
    verifyLifecycleIdempotencyContracts();
    verifyOmissionEquivalentOptionalInputs();
    verifyIndependentScalarOperations();
    verifyVoiceContracts();
    verifySpeakerMixContracts();
    verifyDocumentAndParameterContracts();
    verifyConditionalCapabilityContracts();
    verifyPlaybackPersistenceContracts();
    verifyAdvancedControlContracts();

    if (failures != 0)
        QTextStream(stderr) << failures << " public automation contract test(s) failed" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
