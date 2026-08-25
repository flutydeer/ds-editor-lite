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
        return resolveSchema(
            resolveSchema(propertySchema(schema, name), schema)
                .value(QStringLiteral("items"))
                .toObject(),
            schema);
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

    QJsonObject commandContext() {
        return {
            {QStringLiteral("document_id"),
             QStringLiteral("00000000-0000-4000-8000-000000000001")},
            {QStringLiteral("expected_revision"), 0},
        };
    }

    void verifyAuthoritativeToolset() {
        const auto &contracts = publicToolContracts();
        const auto expectedIds = editorToolIds();
        expect(editorTools().size() == 128 && editorToolIdSet().size() == 128,
               QStringLiteral("test fixture must contain exactly 128 unique editor tools"));
        expect(contracts.size() == 128,
               QStringLiteral("public contract surface must contain exactly 128 editor tools"));
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

            const auto descriptor = contract.toManifestJson();
            expect(descriptor.value(QStringLiteral("version")).toInteger() == 1 &&
                       descriptor.value(QStringLiteral("introduced_version")).toInteger() == 1 &&
                       descriptor.value(QStringLiteral("minimum_compatible_version")).toInteger() ==
                           1,
                   QStringLiteral("all three per-tool version fields must equal one for ") +
                       contract.operationId);
            expect(contract.inputSchema.value(QStringLiteral("type")) == QStringLiteral("object") &&
                       contract.inputSchema.value(QStringLiteral("additionalProperties")) == false,
                   QStringLiteral("input root must be a closed object for ") + contract.operationId);
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
        expect(toolsForProfile(AutomationProfile::Meta).size() == 4 &&
                   toolsForProfile(AutomationProfile::L1).size() == 90 &&
                   toolsForProfile(AutomationProfile::L2).size() == 128 &&
                   toolsForProfile(AutomationProfile::L3).size() == 128,
               QStringLiteral("editor profile counts must be 4/90/128/128"));
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
        nestedTrack.insert(
            QStringLiteral("tracks"),
            QJsonArray{QJsonObject{{QStringLiteral("clips"), QJsonArray{}},
                                   {QStringLiteral("voice"), QJsonObject{}}}});
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
        minimalClip.insert(QStringLiteral("clips"),
                           QJsonArray{QJsonObject{{QStringLiteral("track_id"), 1},
                                                  {QStringLiteral("start"), 0}}});
        expect(validateJsonValue(minimalClip, clips->inputSchema).valid(),
               QStringLiteral("clips.insert must accept GUI-equivalent default length and name"));
        auto nestedClip = minimalClip;
        nestedClip.insert(
            QStringLiteral("clips"),
            QJsonArray{QJsonObject{{QStringLiteral("track_id"), 1},
                                   {QStringLiteral("start"), 0},
                                   {QStringLiteral("notes"), QJsonArray{}},
                                   {QStringLiteral("parameters"), QJsonArray{}},
                                   {QStringLiteral("speaker_mix"), QJsonObject{}}}});
        expect(!validateJsonValue(nestedClip, clips->inputSchema).valid(),
               QStringLiteral("clips.insert must reject nested editing trees"));

        const auto note = arrayItemSchema(notes->inputSchema, QStringLiteral("notes"));
        const auto noteRequired = requiredFields(note, notes->inputSchema);
        expect(noteRequired ==
                   QSet<QString>{QStringLiteral("local_start"), QStringLiteral("length"),
                                 QStringLiteral("key_index")} &&
                   !note.value(QStringLiteral("properties"))
                        .toObject()
                        .contains(QStringLiteral("voice_context")) &&
                   note.value(QStringLiteral("additionalProperties")) == false,
               QStringLiteral(
                   "NoteDraft must require only geometry and pitch without voice_context"));
        auto minimalNote = commandContext();
        minimalNote.insert(QStringLiteral("clip_id"), 1);
        minimalNote.insert(
            QStringLiteral("notes"),
            QJsonArray{QJsonObject{{QStringLiteral("local_start"), 0},
                                   {QStringLiteral("length"), 480},
                                   {QStringLiteral("key_index"), 60}}});
        expect(validateJsonValue(minimalNote, notes->inputSchema).valid(),
               QStringLiteral("notes.insert must accept server-resolved lyric/language defaults"));
    }

    void verifyLifecycleIdempotencyContracts() {
        for (const auto &operationId : {QStringLiteral("documents.new"),
                                        QStringLiteral("documents.open")}) {
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

    void verifyScalarMutation(const QString &operationId, const QString &targetField,
                              const QString &valueField) {
        const auto *contract = findPublicTool(operationId);
        expect(contract, operationId + QStringLiteral(" must exist"));
        if (!contract)
            return;
        const auto properties = contract->inputSchema.value(QStringLiteral("properties")).toObject();
        QSet<QString> expected{
            QStringLiteral("document_id"), QStringLiteral("expected_revision"),
            QStringLiteral("validate_only"), QStringLiteral("idempotency_key"), valueField,
        };
        if (!targetField.isEmpty())
            expected.insert(targetField);
        expect(keys(properties) == expected,
               operationId + QStringLiteral(" must expose one independent scalar edit"));
        const auto descriptor = contract->toManifestJson();
        expect(descriptor.value(QStringLiteral("history_policy")) ==
                       QStringLiteral("single_entry") &&
                   descriptor.value(QStringLiteral("revision_policy")) == QStringLiteral("check"),
               operationId + QStringLiteral(" must be exactly one History/revision edit"));
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

        for (const auto &removed : {QStringLiteral("tracks.set_properties"),
                                    QStringLiteral("clips.set_properties"),
                                    QStringLiteral("master.set_control"),
                                    QStringLiteral("notes.set_word_properties")}) {
            expect(findPublicTool(removed) == nullptr,
                   removed + QStringLiteral(" must not collapse independent History edits"));
        }
    }

    void verifyVoiceContracts() {
        for (const auto &operation : {QStringLiteral("tracks.get_voice_context"),
                                      QStringLiteral("tracks.set_voice"),
                                      QStringLiteral("tracks.clear_voice")}) {
            const auto *contract = findPublicTool(operation);
            expect(contract && contract->category == QStringLiteral("tracks"),
                   operation + QStringLiteral(" must belong to the track domain"));
        }
        for (const auto &operation : {QStringLiteral("clips.get_voice_context"),
                                      QStringLiteral("clips.use_track_voice"),
                                      QStringLiteral("clips.set_voice"),
                                      QStringLiteral("clips.clear_voice")}) {
            const auto *contract = findPublicTool(operation);
            expect(contract && contract->category == QStringLiteral("clips"),
                   operation + QStringLiteral(" must belong to the clip domain"));
        }

        const auto checkVoice = [](const QString &operation) {
            const auto *contract = findPublicTool(operation);
            if (!contract)
                return false;
            const auto voice = resolveSchema(
                propertySchema(contract->inputSchema, QStringLiteral("voice")),
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
            singerOnly.insert(
                QStringLiteral("voice"),
                QJsonObject{
                    {QStringLiteral("singer"),
                     QJsonObject{
                         {QStringLiteral("package_id"), QStringLiteral("package")},
                         {QStringLiteral("singer_id"), QStringLiteral("singer")},
                     }},
                });
            return requiredFields(voice, contract->inputSchema) ==
                       QSet<QString>{QStringLiteral("singer")} &&
                   requiredFields(singer, contract->inputSchema) ==
                       QSet<QString>{QStringLiteral("package_id"), QStringLiteral("singer_id")} &&
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
        expect(!requiredFields(insert->inputSchema).contains(QStringLiteral("weights")),
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

        const auto audioOptions = propertySchema(audioPreview->inputSchema, QStringLiteral("options"));
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
            hasFillLanguageSource |=
                source.value(QStringLiteral("field_path")).toString() ==
                    QStringLiteral("/options/language/language_id") &&
                source.value(QStringLiteral("operation_id")).toString() ==
                    QStringLiteral("voices.describe");
        }
        expect(hasFillLanguageSource,
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
                 {QStringLiteral("clip_id"),          1                            },
                 {QStringLiteral("path"),             QString()                    },
                 {QStringLiteral("path_status"),      QStringLiteral("missing")   },
                 {QStringLiteral("candidate_paths"),  QJsonArray{}                 },
                 {QStringLiteral("hash_exists"),      false                        },
                 {QStringLiteral("duration_seconds"), QJsonValue(QJsonValue::Null)},
                 {QStringLiteral("sample_rate"),      QJsonValue(QJsonValue::Null)},
                 {QStringLiteral("channels"),         QJsonValue(QJsonValue::Null)},
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
            QStringLiteral("previous"),
            QStringLiteral("current"),
            QStringLiteral("changed"),
            QStringLiteral("validated_only"),
            QStringLiteral("affected_objects"),
            QStringLiteral("created_objects"),
            QStringLiteral("resolved_values"),
            QStringLiteral("presentation_effects"),
            QStringLiteral("warnings"),
            QStringLiteral("state_version"),
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
            const auto descriptor = contract->toManifestJson();
            expect(inputProperties.contains(QStringLiteral("document_id")) &&
                       inputProperties.contains(QStringLiteral("expected_revision")) &&
                       inputProperties.contains(QStringLiteral("expected_state_version")) &&
                       inputProperties.contains(QStringLiteral("validate_only")) &&
                       inputProperties.contains(QStringLiteral("idempotency_key")) &&
                       inputRequired.contains(QStringLiteral("document_id")) &&
                       inputRequired.contains(QStringLiteral("expected_revision")) &&
                       !inputRequired.contains(QStringLiteral("expected_state_version")),
                   operation + QStringLiteral(
                                   " must require document revision and optionally check playback state"));
            expect(descriptor.value(QStringLiteral("document_policy")) ==
                           QStringLiteral("write") &&
                       descriptor.value(QStringLiteral("revision_policy")) ==
                           QStringLiteral("check") &&
                       descriptor.value(QStringLiteral("history_policy")) ==
                           QStringLiteral("single_entry") &&
                       descriptor.value(QStringLiteral("concurrency_scope")) ==
                           QStringLiteral("document") &&
                       descriptor.value(QStringLiteral("conflict_policy")) ==
                           QStringLiteral("revision_and_state_version"),
                   operation + QStringLiteral(" must publish its persistent loop-edit policies"));
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
            const auto properties = seek->inputSchema.value(QStringLiteral("properties")).toObject();
            const auto descriptor = seek->toManifestJson();
            expect(!properties.contains(QStringLiteral("expected_revision")) &&
                       !properties.contains(QStringLiteral("idempotency_key")) &&
                       properties.contains(QStringLiteral("expected_state_version")) &&
                       !seek->outputSchema.value(QStringLiteral("properties"))
                            .toObject()
                            .contains(QStringLiteral("previous")) &&
                       descriptor.value(QStringLiteral("document_policy")) ==
                           QStringLiteral("read") &&
                       descriptor.value(QStringLiteral("history_policy")) ==
                           QStringLiteral("none") &&
                       descriptor.value(QStringLiteral("concurrency_scope")) ==
                           QStringLiteral("playback"),
                   QStringLiteral("seek must remain a non-persistent playback state operation"));
        }
    }

    void verifyDocumentPolicies() {
        for (const auto &operation : {QStringLiteral("extract.pitch.start"),
                                      QStringLiteral("extract.midi.start"),
                                      QStringLiteral("inference.start")}) {
            const auto *contract = findPublicTool(operation);
            expect(contract &&
                       contract->toManifestJson().value(QStringLiteral("document_policy")) ==
                           QStringLiteral("write"),
                   operation + QStringLiteral(" must publish a write document policy"));
        }

        const auto *preview = findPublicTool(QStringLiteral("exports.midi.preview"));
        expect(preview &&
                   preview->toManifestJson().value(QStringLiteral("document_policy")) ==
                       QStringLiteral("read"),
               QStringLiteral("exports.midi.preview must publish a read document policy"));
    }
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    verifyAuthoritativeToolset();
    verifyShallowCreationAndNoteDefaults();
    verifyLifecycleIdempotencyContracts();
    verifyIndependentScalarOperations();
    verifyVoiceContracts();
    verifySpeakerMixContracts();
    verifyConditionalCapabilityContracts();
    verifyPlaybackPersistenceContracts();
    verifyDocumentPolicies();

    if (failures != 0)
        QTextStream(stderr) << failures << " public automation contract test(s) failed" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
