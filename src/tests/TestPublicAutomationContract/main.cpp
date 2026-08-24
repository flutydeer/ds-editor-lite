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
        expect(editorTools().size() == 127 && editorToolIdSet().size() == 127,
               QStringLiteral("test fixture must contain exactly 127 unique editor tools"));
        expect(contracts.size() == 127,
               QStringLiteral("public contract surface must contain exactly 127 editor tools"));
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
                   toolsForProfile(AutomationProfile::L1).size() == 89 &&
                   toolsForProfile(AutomationProfile::L2).size() == 127 &&
                   toolsForProfile(AutomationProfile::L3).size() == 127,
               QStringLiteral("editor profile counts must be 4/89/127/127"));
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
            return requiredFields(voice, contract->inputSchema) ==
                       QSet<QString>{QStringLiteral("singer"), QStringLiteral("speaker")} &&
                   requiredFields(singer, contract->inputSchema) ==
                       QSet<QString>{QStringLiteral("package_id"), QStringLiteral("singer_id")} &&
                   requiredFields(speaker, contract->inputSchema) ==
                       QSet<QString>{QStringLiteral("speaker_id")} &&
                   keys(speaker.value(QStringLiteral("properties")).toObject()) ==
                       QSet<QString>{QStringLiteral("speaker_id")};
        };
        expect(checkVoice(QStringLiteral("tracks.set_voice")) &&
                   checkVoice(QStringLiteral("clips.set_voice")),
               QStringLiteral("voice selection must separate SingerRef and SpeakerRef"));
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
}

int main(int argc, char **argv) {
    QCoreApplication application(argc, argv);
    verifyAuthoritativeToolset();
    verifyShallowCreationAndNoteDefaults();
    verifyIndependentScalarOperations();
    verifyVoiceContracts();
    verifySpeakerMixContracts();

    if (failures != 0)
        QTextStream(stderr) << failures << " public automation contract test(s) failed" << Qt::endl;
    return failures == 0 ? 0 : 1;
}
