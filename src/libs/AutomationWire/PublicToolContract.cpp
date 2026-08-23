#include "PublicToolContract.h"

#include "CanonicalJson.h"
#include "JsonSchema.h"
#include "PublicConstants.h"
#include "PublicEnums.h"
#include "PublicValueDomains.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <limits>
#include <utility>

namespace AutomationWire {
    namespace {
        QJsonObject uuidSchema() {
            auto schema = JsonSchema::string();
            schema.insert(QStringLiteral("format"), QStringLiteral("uuid"));
            return schema;
        }

        QJsonObject identifierSchema() {
            return JsonSchema::integer(0.0, std::numeric_limits<int>::max());
        }

        QJsonObject revisionSchema() {
            return JsonSchema::integer(0.0, static_cast<double>(MaximumSafeJsonInteger));
        }

        QJsonObject nonEmptyStringSchema() {
            return JsonSchema::string({}, 1);
        }

        QJsonObject stringDomainSchema(const PublicValueDomain domain) {
            return JsonSchema::string(publicStringValueDomainValues(domain));
        }

        QJsonObject valueDomainSchema(const PublicValueDomain domain) {
            return JsonSchema::enumeration(publicValueDomainValues(domain));
        }

        QJsonObject domainConstant(const PublicValueDomain domain, const QString &value) {
            if (!publicValueDomainContains(domain, value)) {
                qFatal("Unregistered public value '%s' for domain '%s'", qPrintable(value),
                       qPrintable(publicValueDomainName(domain)));
            }
            return JsonSchema::constant(value);
        }

        QJsonObject voiceRefSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("package_id"), nonEmptyStringSchema()},
                    {QStringLiteral("singer_id"), nonEmptyStringSchema()},
                    {QStringLiteral("speaker_id"), nonEmptyStringSchema()},
                },
                {QStringLiteral("singer_id")});
        }

        QJsonObject documentVersionSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("document_id"), uuidSchema()},
                    {QStringLiteral("revision"), revisionSchema()},
                },
                {QStringLiteral("document_id"), QStringLiteral("revision")});
        }

        QJsonObject parameterNameSchema();
        QJsonObject parameterLayerSchema();
        QJsonObject interpolationSchema();
        QJsonArray valueSources(const QString &id);
        QJsonObject inputSchema(const QString &id, OperationKind kind);
        QJsonObject getOptionsInputSchema(const QList<ToolContract> &tools);

        QJsonObject trackControlSchema() {
            auto result = JsonSchema::object(
                {
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                    {QStringLiteral("solo"), JsonSchema::boolean()},
                });
            result.insert(QStringLiteral("minProperties"), 1);
            return result;
        }

        QJsonObject clipPropertiesSchema(const bool requireId) {
            auto result = JsonSchema::object(
                {
                    {QStringLiteral("clip_id"), identifierSchema()},
                    {QStringLiteral("name"), JsonSchema::string()},
                    {QStringLiteral("start"), JsonSchema::integer(0.0)},
                    {QStringLiteral("length"), JsonSchema::integer(1.0)},
                    {QStringLiteral("clip_start"), JsonSchema::integer(0.0)},
                    {QStringLiteral("clip_length"), JsonSchema::integer(1.0)},
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                },
                requireId ? QStringList{QStringLiteral("clip_id")} : QStringList{});
            result.insert(QStringLiteral("minProperties"), requireId ? 2 : 1);
            return result;
        }

        QJsonObject noteDraftSchema() {
            const auto pronunciation = JsonSchema::object(
                {
                    {QStringLiteral("value"), JsonSchema::string()},
                    {QStringLiteral("source"),
                     stringDomainSchema(PublicValueDomain::PronunciationSource)},
                },
                {QStringLiteral("value")});
            const auto phoneme = JsonSchema::object(
                {
                    {QStringLiteral("symbol"), nonEmptyStringSchema()},
                    {QStringLiteral("language"), JsonSchema::string()},
                    {QStringLiteral("offset"), JsonSchema::integer()},
                },
                {QStringLiteral("symbol")});
            return JsonSchema::object(
                {
                    {QStringLiteral("client_ref"), JsonSchema::string()},
                    {QStringLiteral("local_start"), JsonSchema::integer(0.0)},
                    {QStringLiteral("length"), JsonSchema::integer(1.0)},
                    {QStringLiteral("key_index"),
                     JsonSchema::integer(MinimumMidiKeyIndex, MaximumMidiKeyIndex)},
                    {QStringLiteral("cent_shift"),
                     JsonSchema::integer(MinimumCentShift, MaximumCentShift)},
                    {QStringLiteral("lyric"), JsonSchema::string()},
                    {QStringLiteral("language"), nonEmptyStringSchema()},
                    {QStringLiteral("pronunciation"), pronunciation},
                    {QStringLiteral("pronunciation_candidates"),
                     JsonSchema::array(JsonSchema::string())},
                    {QStringLiteral("phonemes"), JsonSchema::array(phoneme)},
                    {QStringLiteral("line_feed"), JsonSchema::boolean()},
                },
                {QStringLiteral("local_start"), QStringLiteral("length"),
                 QStringLiteral("key_index"), QStringLiteral("lyric"),
                 QStringLiteral("language")});
        }

        QJsonObject noteWordEditSchema() {
            const auto phoneme = JsonSchema::object(
                {
                    {QStringLiteral("symbol"), nonEmptyStringSchema()},
                    {QStringLiteral("language"), JsonSchema::string()},
                    {QStringLiteral("offset"), JsonSchema::integer()},
                },
                {QStringLiteral("symbol")});
            auto result = JsonSchema::object(
                {
                    {QStringLiteral("note_id"), identifierSchema()},
                    {QStringLiteral("lyric"), JsonSchema::string()},
                    {QStringLiteral("language"), nonEmptyStringSchema()},
                    {QStringLiteral("pronunciation"), JsonSchema::string()},
                    {QStringLiteral("pronunciation_candidates"),
                     JsonSchema::array(JsonSchema::string())},
                    {QStringLiteral("phonemes"), JsonSchema::array(phoneme)},
                },
                {QStringLiteral("note_id")});
            result.insert(QStringLiteral("minProperties"), 2);
            return result;
        }

        QJsonObject curveDraftSchema() {
            const auto draw = JsonSchema::object(
                {
                    {QStringLiteral("type"),
                     domainConstant(PublicValueDomain::CurveType, QStringLiteral("draw"))},
                    {QStringLiteral("local_start"), JsonSchema::integer()},
                    {QStringLiteral("step"), JsonSchema::integer(1.0)},
                    {QStringLiteral("values"), JsonSchema::array(JsonSchema::integer(), 1)},
                },
                {QStringLiteral("type"), QStringLiteral("local_start"), QStringLiteral("step"),
                 QStringLiteral("values")});
            const auto anchorNode = JsonSchema::object(
                {
                    {QStringLiteral("position"), JsonSchema::integer()},
                    {QStringLiteral("value"), JsonSchema::integer()},
                    {QStringLiteral("interpolation"), interpolationSchema()},
                },
                {QStringLiteral("position"), QStringLiteral("value"),
                 QStringLiteral("interpolation")});
            const auto anchor = JsonSchema::object(
                {
                    {QStringLiteral("type"),
                     domainConstant(PublicValueDomain::CurveType, QStringLiteral("anchor"))},
                    {QStringLiteral("nodes"), JsonSchema::array(anchorNode, 1)},
                },
                {QStringLiteral("type"), QStringLiteral("nodes")});
            return JsonSchema::oneOf(QJsonArray{draw, anchor});
        }

        QJsonObject speakerMixSchema() {
            const auto keyframe = JsonSchema::object(
                {
                    {QStringLiteral("position"), JsonSchema::integer()},
                    {QStringLiteral("weights"),
                     JsonSchema::array(
                         JsonSchema::number(MinimumMixWeight, MaximumMixWeight), 1)},
                },
                {QStringLiteral("position"), QStringLiteral("weights")});
            return JsonSchema::object(
                {
                    {QStringLiteral("mode"),
                     stringDomainSchema(PublicValueDomain::SpeakerMixMode)},
                    {QStringLiteral("singer"), voiceRefSchema()},
                    {QStringLiteral("speakers"), JsonSchema::array(voiceRefSchema(), 1)},
                    {QStringLiteral("fixed_weights"),
                     JsonSchema::array(
                         JsonSchema::number(MinimumMixWeight, MaximumMixWeight), 1)},
                    {QStringLiteral("dynamic_keyframes"), JsonSchema::array(keyframe, 1)},
                },
                {QStringLiteral("mode"), QStringLiteral("singer"),
                 QStringLiteral("speakers")});
        }

        QJsonObject clipDraftSchema() {
            const auto parameter = JsonSchema::object(
                {
                    {QStringLiteral("name"), parameterNameSchema()},
                    {QStringLiteral("type"), parameterLayerSchema()},
                    {QStringLiteral("curves"), JsonSchema::array(curveDraftSchema())},
                },
                {QStringLiteral("name"), QStringLiteral("type"), QStringLiteral("curves")});
            return JsonSchema::object(
                {
                    {QStringLiteral("client_ref"), JsonSchema::string()},
                    {QStringLiteral("type"),
                     domainConstant(PublicValueDomain::ClipType, QStringLiteral("singing"))},
                    {QStringLiteral("properties"), clipPropertiesSchema(false)},
                    {QStringLiteral("default_language"), nonEmptyStringSchema()},
                    {QStringLiteral("notes"), JsonSchema::array(noteDraftSchema())},
                    {QStringLiteral("parameters"), JsonSchema::array(parameter)},
                    {QStringLiteral("speaker_mix"), speakerMixSchema()},
                },
                {QStringLiteral("type"), QStringLiteral("properties"),
                 QStringLiteral("default_language")});
        }

        QJsonObject trackDraftSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("client_ref"), JsonSchema::string()},
                    {QStringLiteral("name"), JsonSchema::string()},
                    {QStringLiteral("color_index"),
                     JsonSchema::integer(0.0, TrackPaletteColorCount - 1)},
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                    {QStringLiteral("solo"), JsonSchema::boolean()},
                    {QStringLiteral("default_language"), nonEmptyStringSchema()},
                    {QStringLiteral("voice"), voiceRefSchema()},
                },
                {QStringLiteral("name"), QStringLiteral("color_index"),
                 QStringLiteral("gain"), QStringLiteral("pan"), QStringLiteral("mute"),
                 QStringLiteral("solo"), QStringLiteral("default_language")});
        }

        QJsonObject trackPropertiesSchema() {
            auto result = JsonSchema::object(
                {
                    {QStringLiteral("track_id"), identifierSchema()},
                    {QStringLiteral("name"), JsonSchema::string()},
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                    {QStringLiteral("solo"), JsonSchema::boolean()},
                },
                {QStringLiteral("track_id")});
            result.insert(QStringLiteral("minProperties"), 2);
            return result;
        }

        QJsonObject audioExportOptionsSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("format"), nonEmptyStringSchema()},
                    {QStringLiteral("sample_rate"),
                     JsonSchema::integer(MinimumAudioSampleRate, MaximumAudioSampleRate)},
                    {QStringLiteral("channel_mode"),
                     stringDomainSchema(PublicValueDomain::ChannelMode)},
                    {QStringLiteral("mixing_mode"),
                     stringDomainSchema(PublicValueDomain::AudioMixingMode)},
                    {QStringLiteral("source"),
                     stringDomainSchema(PublicValueDomain::AudioSourceMode)},
                    {QStringLiteral("source_ids"), JsonSchema::array(identifierSchema())},
                    {QStringLiteral("range"),
                     JsonSchema::object(
                         {
                             {QStringLiteral("start"), JsonSchema::number(0.0)},
                             {QStringLiteral("end"), JsonSchema::number(0.0)},
                         },
                         {QStringLiteral("start"), QStringLiteral("end")})},
                    {QStringLiteral("mute_solo_enabled"), JsonSchema::boolean()},
                },
                {QStringLiteral("format"), QStringLiteral("sample_rate"),
                 QStringLiteral("channel_mode"), QStringLiteral("mixing_mode"),
                 QStringLiteral("source")});
        }

        QJsonObject inferenceScopeSchema() {
            const auto document = JsonSchema::object(
                {{QStringLiteral("kind"),
                  domainConstant(PublicValueDomain::InferenceScopeKind,
                                 QStringLiteral("document"))}},
                {QStringLiteral("kind")});
            const auto track = JsonSchema::object(
                {
                    {QStringLiteral("kind"),
                     domainConstant(PublicValueDomain::InferenceScopeKind,
                                    QStringLiteral("track"))},
                    {QStringLiteral("track_ids"), JsonSchema::array(identifierSchema(), 1)},
                },
                {QStringLiteral("kind"), QStringLiteral("track_ids")});
            const auto clip = JsonSchema::object(
                {
                    {QStringLiteral("kind"),
                     domainConstant(PublicValueDomain::InferenceScopeKind,
                                    QStringLiteral("clip"))},
                    {QStringLiteral("clip_ids"), JsonSchema::array(identifierSchema(), 1)},
                },
                {QStringLiteral("kind"), QStringLiteral("clip_ids")});
            return JsonSchema::oneOf(QJsonArray{document, track, clip});
        }

        QJsonObject extractionOptionsSchema(const QString &id) {
            if (id == QStringLiteral("extract.pitch.start")) {
                return JsonSchema::object({
                    {QStringLiteral("target_clip_id"), identifierSchema()},
                    {QStringLiteral("model_id"), nonEmptyStringSchema()},
                    {QStringLiteral("minimum_frequency"), JsonSchema::number(1.0)},
                    {QStringLiteral("maximum_frequency"), JsonSchema::number(1.0)},
                }, {QStringLiteral("target_clip_id")});
            }
            return JsonSchema::object({
                {QStringLiteral("model_id"), nonEmptyStringSchema()},
                {QStringLiteral("default_language"), nonEmptyStringSchema()},
                {QStringLiteral("default_lyric"), JsonSchema::string()},
                {QStringLiteral("minimum_note_length"), JsonSchema::integer(1.0)},
            });
        }

        QJsonObject audioImportItemSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("track_id"), identifierSchema()},
                    {QStringLiteral("path"), nonEmptyStringSchema()},
                    {QStringLiteral("properties"), clipPropertiesSchema(false)},
                    {QStringLiteral("client_ref"), JsonSchema::string()},
                },
                {QStringLiteral("track_id"), QStringLiteral("path")});
        }

        QJsonObject fieldSchema(const QString &id, const QString &name) {
            if (name == QStringLiteral("document_id") || name == QStringLiteral("task_id"))
                return uuidSchema();
            if (name == QStringLiteral("track_ids") || name == QStringLiteral("clip_ids") ||
                name == QStringLiteral("note_ids")) {
                return JsonSchema::array(identifierSchema(), 1);
            }
            if (name == QStringLiteral("clip_id") || name == QStringLiteral("track_id") ||
                name == QStringLiteral("note_id") || name == QStringLiteral("anchor_id") ||
                name == QStringLiteral("curve_id") ||
                name == QStringLiteral("target_track_id")) {
                return identifierSchema();
            }
            if (name == QStringLiteral("index") || name == QStringLiteral("target_index") ||
                name == QStringLiteral("bar_index"))
                return JsonSchema::integer(0.0, std::numeric_limits<int>::max());
            if (name == QStringLiteral("minimum_length") || name == QStringLiteral("step") ||
                name == QStringLiteral("numerator") || name == QStringLiteral("denominator"))
                return JsonSchema::integer(1.0, std::numeric_limits<int>::max());
            if (name == QStringLiteral("limit"))
                return JsonSchema::integer(MinimumPageSize, MaximumPageSize);
            if (name == QStringLiteral("expected_revision"))
                return revisionSchema();
            if (name == QStringLiteral("position") || name == QStringLiteral("start") ||
                name == QStringLiteral("end")) {
                return id.startsWith(QStringLiteral("parameters."))
                           ? JsonSchema::integer(std::numeric_limits<int>::min(),
                                                 std::numeric_limits<int>::max())
                           : JsonSchema::integer(0.0, std::numeric_limits<int>::max());
            }
            if (name == QStringLiteral("tick") || name == QStringLiteral("delta_tick") ||
                name == QStringLiteral("delta_key") ||
                name == QStringLiteral("value") || name == QStringLiteral("local_start") ||
                name == QStringLiteral("local_end"))
                return JsonSchema::integer(std::numeric_limits<int>::min(),
                                           std::numeric_limits<int>::max());
            if (name == QStringLiteral("new_length"))
                return JsonSchema::integer(1.0, std::numeric_limits<int>::max());
            if (name == QStringLiteral("color_index"))
                return JsonSchema::integer(0.0, TrackPaletteColorCount - 1);
            if (name == QStringLiteral("tempo"))
                return JsonSchema::number(1.0);
            if (name == QStringLiteral("allow_overwrite")) {
                auto result = JsonSchema::boolean();
                result.insert(QStringLiteral("default"), false);
                return result;
            }
            if (name == QStringLiteral("validate_only") ||
                name == QStringLiteral("import_tempo") ||
                name == QStringLiteral("import_time_signature") ||
                name == QStringLiteral("quantize_start") ||
                name == QStringLiteral("quantize_length") || name == QStringLiteral("enabled")) {
                return JsonSchema::boolean();
            }
            if (name == QStringLiteral("singer") || name == QStringLiteral("speaker"))
                return voiceRefSchema();
            if (name == QStringLiteral("values") || name == QStringLiteral("offsets"))
                return JsonSchema::array(JsonSchema::integer());
            if (name == QStringLiteral("track"))
                return trackDraftSchema();
            if (name == QStringLiteral("new_note"))
                return noteDraftSchema();
            if (name == QStringLiteral("properties"))
                return id.startsWith(QStringLiteral("tracks."))
                           ? trackPropertiesSchema()
                       : id == QStringLiteral("audio_clips.import")
                           ? clipPropertiesSchema(false)
                           : clipPropertiesSchema(true);
            if (name == QStringLiteral("mix"))
                return speakerMixSchema();
            if (name == QStringLiteral("control"))
                return trackControlSchema();
            if (name == QStringLiteral("scope"))
                return inferenceScopeSchema();
            if (name == QStringLiteral("curves"))
                return JsonSchema::array(curveDraftSchema(), 1);
            if (name == QStringLiteral("clips")) {
                return JsonSchema::array(
                    JsonSchema::object(
                        {
                            {QStringLiteral("track_id"), identifierSchema()},
                            {QStringLiteral("clip"), clipDraftSchema()},
                        },
                        {QStringLiteral("track_id"), QStringLiteral("clip")}),
                    1);
            }
            if (name == QStringLiteral("notes"))
                return JsonSchema::array(noteDraftSchema(), 1);
            if (name == QStringLiteral("edits"))
                return JsonSchema::array(noteWordEditSchema(), 1);
            if (name == QStringLiteral("items"))
                return JsonSchema::array(audioImportItemSchema(), 1);
            if (name == QStringLiteral("stages"))
                return JsonSchema::array(
                    stringDomainSchema(PublicValueDomain::InferenceStage), 1);
            if (name == QStringLiteral("options")) {
                if (id.startsWith(QStringLiteral("exports.audio.")))
                    return audioExportOptionsSchema();
                if (id.startsWith(QStringLiteral("extract.")))
                    return extractionOptionsSchema(id);
                if (id == QStringLiteral("exports.midi.start")) {
                    return JsonSchema::object({
                        {QStringLiteral("include_tempo"), JsonSchema::boolean()},
                        {QStringLiteral("include_time_signatures"), JsonSchema::boolean()},
                    });
                }
                if (id == QStringLiteral("inference.start")) {
                    return JsonSchema::object({
                        {QStringLiteral("provider_id"), nonEmptyStringSchema()},
                        {QStringLiteral("device_id"), nonEmptyStringSchema()},
                        {QStringLiteral("model_id"), nonEmptyStringSchema()},
                    });
                }
                qFatal("No explicit public options schema for operation '%s'",
                       qPrintable(id));
                return {};
            }
            if (name == QStringLiteral("failure_policy"))
                return stringDomainSchema(PublicValueDomain::FailurePolicy);
            if (name == QStringLiteral("unsaved_policy"))
                return stringDomainSchema(PublicValueDomain::UnsavedPolicy);
            if (name == QStringLiteral("template"))
                return stringDomainSchema(PublicValueDomain::DocumentTemplate);
            if (name == QStringLiteral("quantize")) {
                return valueDomainSchema(PublicValueDomain::Quantize);
            }
            if (name == QStringLiteral("merge_mode")) {
                return id.startsWith(QStringLiteral("parameters."))
                           ? stringDomainSchema(PublicValueDomain::CurveMergeMode)
                           : stringDomainSchema(PublicValueDomain::DocumentImportMergeMode);
            }
            if (name == QStringLiteral("name") && id.startsWith(QStringLiteral("parameters."))) {
                return parameterNameSchema();
            }
            if (name == QStringLiteral("type") && id.startsWith(QStringLiteral("parameters."))) {
                return parameterLayerSchema();
            }
            if (name == QStringLiteral("interpolation"))
                return interpolationSchema();
            if (name == QStringLiteral("stage"))
                return stringDomainSchema(PublicValueDomain::InferenceStage);
            if (name == QStringLiteral("state") && id == QStringLiteral("tasks.list")) {
                return stringDomainSchema(PublicValueDomain::TaskState);
            }
            if (name == QStringLiteral("kind") && id == QStringLiteral("tasks.list")) {
                return stringDomainSchema(PublicValueDomain::TaskKind);
            }
            if (name == QStringLiteral("operation_id") ||
                name == QStringLiteral("field_path") ||
                name == QStringLiteral("idempotency_key") ||
                name == QStringLiteral("client_ref") ||
                name == QStringLiteral("cursor") || name == QStringLiteral("query") ||
                name == QStringLiteral("package_id") || name == QStringLiteral("language") ||
                name == QStringLiteral("path")) {
                return nonEmptyStringSchema();
            }
            qFatal("No explicit public field schema for operation '%s', field '%s'",
                   qPrintable(id), qPrintable(name));
            return {};
        }

        void addField(const QString &id, QJsonObject &properties, QStringList &required,
                      const QString &name,
                      const bool isRequired = true) {
            properties.insert(name, fieldSchema(id, name));
            if (isRequired && !required.contains(name))
                required.append(name);
        }

        bool isDocumentQuery(const QString &id) {
            static const QSet<QString> ids{
                QStringLiteral("documents.get"),
                QStringLiteral("project.get"),
                QStringLiteral("notes.get"),
                QStringLiteral("parameters.get"),
                QStringLiteral("parameters.get_capabilities"),
                QStringLiteral("timeline.get"),
                QStringLiteral("history.get_state"),
                QStringLiteral("exports.midi.start"),
                QStringLiteral("exports.audio.get_capabilities"),
                QStringLiteral("exports.audio.preview"),
                QStringLiteral("exports.audio.start"),
                QStringLiteral("extract.get_capabilities"),
                QStringLiteral("inference.get_capabilities"),
                QStringLiteral("tasks.list"),
                QStringLiteral("tasks.get"),
                QStringLiteral("tasks.cancel"),
                QStringLiteral("playback.get"),
            };
            return ids.contains(id);
        }

        bool isDocumentCommand(const QString &id, const OperationKind kind) {
            return kind == OperationKind::Command && id != QStringLiteral("tasks.cancel");
        }

        QStringList operationRequiredFields(const QString &id) {
            static const QHash<QString, QStringList> fields{
                {QStringLiteral("automation.get_options"),
                 {QStringLiteral("operation_id"), QStringLiteral("field_path")}},
                {QStringLiteral("notes.get"), {QStringLiteral("clip_id")}},
                {QStringLiteral("parameters.get"),
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("type")}},
                {QStringLiteral("parameters.get_capabilities"), {QStringLiteral("clip_id")}},
                {QStringLiteral("voices.describe"), {QStringLiteral("singer")}},
                {QStringLiteral("tracks.insert"),
                 {QStringLiteral("index"), QStringLiteral("track")}},
                {QStringLiteral("tracks.remove"), {QStringLiteral("track_ids")}},
                {QStringLiteral("tracks.move"),
                 {QStringLiteral("track_id"), QStringLiteral("target_index")}},
                {QStringLiteral("tracks.set_properties"), {QStringLiteral("properties")}},
                {QStringLiteral("tracks.set_color"),
                 {QStringLiteral("track_id"), QStringLiteral("color_index")}},
                {QStringLiteral("tracks.set_default_language"),
                 {QStringLiteral("track_id"), QStringLiteral("language")}},
                {QStringLiteral("clips.insert"), {QStringLiteral("clips")}},
                {QStringLiteral("clips.remove"), {QStringLiteral("clip_ids")}},
                {QStringLiteral("clips.set_properties"), {QStringLiteral("properties")}},
                {QStringLiteral("clips.set_default_language"),
                 {QStringLiteral("clip_id"), QStringLiteral("language")}},
                {QStringLiteral("notes.insert"),
                 {QStringLiteral("clip_id"), QStringLiteral("notes")}},
                {QStringLiteral("notes.remove"),
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids")}},
                {QStringLiteral("notes.move"),
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("delta_tick"), QStringLiteral("delta_key")}},
                {QStringLiteral("notes.resize_left"),
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("delta_tick"), QStringLiteral("minimum_length")}},
                {QStringLiteral("notes.resize_right"),
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("delta_tick"), QStringLiteral("minimum_length")}},
                {QStringLiteral("notes.split"),
                 {QStringLiteral("clip_id"), QStringLiteral("note_id"),
                  QStringLiteral("new_note"), QStringLiteral("new_length")}},
                {QStringLiteral("notes.quantize"),
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("quantize"), QStringLiteral("quantize_start"),
                  QStringLiteral("quantize_length")}},
                {QStringLiteral("notes.set_word_properties"),
                 {QStringLiteral("clip_id"), QStringLiteral("edits")}},
                {QStringLiteral("notes.set_phoneme_offsets"),
                 {QStringLiteral("clip_id"), QStringLiteral("note_id"),
                  QStringLiteral("offsets")}},
                {QStringLiteral("parameters.replace"),
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("type"),
                  QStringLiteral("curves")}},
                {QStringLiteral("parameters.draw"),
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("type"),
                  QStringLiteral("local_start"), QStringLiteral("step"),
                  QStringLiteral("values")}},
                {QStringLiteral("parameters.erase"),
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("type"),
                  QStringLiteral("local_start"), QStringLiteral("local_end")}},
                {QStringLiteral("parameters.insert_anchor"),
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("type"),
                  QStringLiteral("position"), QStringLiteral("value"),
                  QStringLiteral("interpolation")}},
                {QStringLiteral("parameters.move_anchor"),
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("type"),
                  QStringLiteral("anchor_id"), QStringLiteral("position"),
                  QStringLiteral("value")}},
                {QStringLiteral("parameters.remove_anchor"),
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("type"),
                  QStringLiteral("anchor_id")}},
                {QStringLiteral("parameters.set_anchor_interpolation"),
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("type"),
                  QStringLiteral("anchor_id"), QStringLiteral("interpolation")}},
                {QStringLiteral("parameters.bake"),
                 {QStringLiteral("clip_id"), QStringLiteral("name")}},
                {QStringLiteral("speaker_mix.track.select_single"),
                 {QStringLiteral("track_id"), QStringLiteral("singer"),
                  QStringLiteral("speaker")}},
                {QStringLiteral("speaker_mix.track.apply"),
                 {QStringLiteral("track_id"), QStringLiteral("singer"),
                  QStringLiteral("speaker"), QStringLiteral("mix")}},
                {QStringLiteral("speaker_mix.track.replace"),
                 {QStringLiteral("track_id"), QStringLiteral("mix")}},
                {QStringLiteral("speaker_mix.clip.use_track"), {QStringLiteral("clip_id")}},
                {QStringLiteral("speaker_mix.clip.select_single"),
                 {QStringLiteral("clip_id"), QStringLiteral("singer"),
                  QStringLiteral("speaker")}},
                {QStringLiteral("speaker_mix.clip.enable_dynamic"),
                 {QStringLiteral("clip_id"), QStringLiteral("singer"),
                  QStringLiteral("speaker"), QStringLiteral("mix")}},
                {QStringLiteral("speaker_mix.clip.apply"),
                 {QStringLiteral("clip_id"), QStringLiteral("singer"),
                  QStringLiteral("speaker"), QStringLiteral("mix")}},
                {QStringLiteral("speaker_mix.clip.replace"),
                 {QStringLiteral("clip_id"), QStringLiteral("mix")}},
                {QStringLiteral("tempos.set"),
                 {QStringLiteral("tick"), QStringLiteral("tempo")}},
                {QStringLiteral("tempos.delete"), {QStringLiteral("tick")}},
                {QStringLiteral("time_signatures.set"),
                 {QStringLiteral("bar_index"), QStringLiteral("numerator"),
                  QStringLiteral("denominator")}},
                {QStringLiteral("time_signatures.delete"), {QStringLiteral("bar_index")}},
                {QStringLiteral("master.set_control"), {QStringLiteral("control")}},
                {QStringLiteral("documents.new"), {QStringLiteral("unsaved_policy")}},
                {QStringLiteral("documents.open"),
                 {QStringLiteral("path"), QStringLiteral("unsaved_policy")}},
                {QStringLiteral("documents.import"), {QStringLiteral("path")}},
                {QStringLiteral("audio_clips.import"),
                 {QStringLiteral("track_id"), QStringLiteral("path")}},
                {QStringLiteral("audio_clips.import_batch"),
                 {QStringLiteral("items"), QStringLiteral("failure_policy")}},
                {QStringLiteral("audio_clips.relocate"),
                 {QStringLiteral("clip_id"), QStringLiteral("path")}},
                {QStringLiteral("audio_clips.confirm_path"),
                 {QStringLiteral("clip_id"), QStringLiteral("path")}},
                {QStringLiteral("exports.midi.start"), {QStringLiteral("path")}},
                {QStringLiteral("exports.audio.preview"), {QStringLiteral("options")}},
                {QStringLiteral("exports.audio.start"),
                 {QStringLiteral("path"), QStringLiteral("options")}},
                {QStringLiteral("extract.get_capabilities"), {QStringLiteral("clip_id")}},
                {QStringLiteral("extract.pitch.start"), {QStringLiteral("clip_id")}},
                {QStringLiteral("extract.midi.start"), {QStringLiteral("clip_id")}},
                {QStringLiteral("inference.start"), {QStringLiteral("scope")}},
                {QStringLiteral("inference.reset_stage"),
                 {QStringLiteral("scope"), QStringLiteral("stage")}},
                {QStringLiteral("tasks.get"),
                 {QStringLiteral("document_id"), QStringLiteral("task_id")}},
                {QStringLiteral("tasks.cancel"),
                 {QStringLiteral("document_id"), QStringLiteral("task_id")}},
                {QStringLiteral("playback.set_position"), {QStringLiteral("position")}},
                {QStringLiteral("playback.set_last_position"), {QStringLiteral("position")}},
                {QStringLiteral("playback.set_loop"),
                 {QStringLiteral("start"), QStringLiteral("end")}},
                {QStringLiteral("playback.set_loop_enabled"), {QStringLiteral("enabled")}},
            };
            return fields.value(id);
        }

        void addOptionalFields(const QString &id, QJsonObject &properties) {
            const auto add = [&](const QString &name) {
                properties.insert(name, fieldSchema(id, name));
            };
            if (id == QStringLiteral("automation.get_manifest") || id == QStringLiteral("tasks.list")) {
                add(QStringLiteral("cursor"));
                add(QStringLiteral("limit"));
            }
            if (id == QStringLiteral("tasks.list")) {
                add(QStringLiteral("state"));
                add(QStringLiteral("kind"));
            }
            if (id == QStringLiteral("voices.list")) {
                add(QStringLiteral("query"));
                add(QStringLiteral("package_id"));
            }
            if (id == QStringLiteral("documents.new"))
                add(QStringLiteral("template"));
            if (id == QStringLiteral("documents.import")) {
                add(QStringLiteral("import_tempo"));
                add(QStringLiteral("import_time_signature"));
                add(QStringLiteral("merge_mode"));
            }
            if (id == QStringLiteral("documents.save")) {
                add(QStringLiteral("path"));
                add(QStringLiteral("allow_overwrite"));
            }
            if (id == QStringLiteral("exports.midi.start") ||
                id == QStringLiteral("exports.audio.start")) {
                add(QStringLiteral("allow_overwrite"));
            }
            if (id == QStringLiteral("exports.midi.start") ||
                id.startsWith(QStringLiteral("extract."))) {
                add(QStringLiteral("options"));
            }
            if (id == QStringLiteral("inference.get_capabilities"))
                add(QStringLiteral("scope"));
            if (id == QStringLiteral("inference.start")) {
                add(QStringLiteral("stages"));
                add(QStringLiteral("options"));
            }
            if (id == QStringLiteral("parameters.draw"))
                add(QStringLiteral("merge_mode"));
            if (id == QStringLiteral("parameters.insert_anchor"))
                add(QStringLiteral("curve_id"));
            if (id == QStringLiteral("parameters.bake")) {
                add(QStringLiteral("local_start"));
                add(QStringLiteral("local_end"));
            }
            if (id == QStringLiteral("clips.set_properties"))
                add(QStringLiteral("target_track_id"));
            if (id == QStringLiteral("audio_clips.import")) {
                add(QStringLiteral("properties"));
                add(QStringLiteral("client_ref"));
            }
        }

        QJsonObject inputSchema(const QString &id, const OperationKind kind) {
            if (id == QStringLiteral("automation.get_options"))
                return JsonSchema::document(JsonSchema::object());
            QJsonObject properties;
            QStringList required;
            const bool replacement = id == QStringLiteral("documents.new") ||
                                     id == QStringLiteral("documents.open");
            if (isDocumentQuery(id))
                addField(id, properties, required, QStringLiteral("document_id"));
            if (isDocumentCommand(id, kind) && !replacement) {
                addField(id, properties, required, QStringLiteral("document_id"));
                addField(id, properties, required, QStringLiteral("expected_revision"));
                addField(id, properties, required, QStringLiteral("validate_only"), false);
                addField(id, properties, required, QStringLiteral("idempotency_key"), false);
            } else if (replacement) {
                addField(id, properties, required, QStringLiteral("validate_only"), false);
                addField(id, properties, required, QStringLiteral("idempotency_key"), false);
            }
            for (const auto &field : operationRequiredFields(id)) {
                if (!properties.contains(field))
                    addField(id, properties, required, field);
            }
            addOptionalFields(id, properties);
            if (replacement) {
                auto versionedProperties = properties;
                versionedProperties.insert(QStringLiteral("document_id"), uuidSchema());
                versionedProperties.insert(QStringLiteral("expected_revision"), revisionSchema());
                auto versionedRequired = required;
                versionedRequired.append(QStringLiteral("document_id"));
                versionedRequired.append(QStringLiteral("expected_revision"));
                return JsonSchema::document(JsonSchema::oneOf(QJsonArray{
                    JsonSchema::object(properties, required),
                    JsonSchema::object(versionedProperties, versionedRequired),
                }));
            }
            auto result = JsonSchema::document(JsonSchema::object(properties, required));
            return result;
        }

        QJsonObject partialRootSchema(QJsonObject schema) {
            schema.remove(QStringLiteral("$schema"));
            if (schema.contains(QStringLiteral("oneOf"))) {
                QJsonObject mergedProperties;
                for (const auto &value : schema.value(QStringLiteral("oneOf")).toArray()) {
                    const auto branchProperties =
                        value.toObject().value(QStringLiteral("properties")).toObject();
                    for (auto it = branchProperties.constBegin(); it != branchProperties.constEnd();
                         ++it) {
                        mergedProperties.insert(it.key(), it.value());
                    }
                }
                return JsonSchema::object(mergedProperties);
            }
            schema.remove(QStringLiteral("required"));
            return schema;
        }

        QJsonObject fieldPathSchema(const QString &path) {
            if (!path.contains(u'*'))
                return JsonSchema::constant(path);
            auto pattern = QRegularExpression::escape(path);
            pattern.replace(QStringLiteral("\\*"), QStringLiteral("(?:\\*|[0-9]+)"));
            auto schema = JsonSchema::string();
            schema.insert(QStringLiteral("pattern"), u'^' + pattern + u'$');
            return schema;
        }

        QJsonObject getOptionsInputSchema(const QList<ToolContract> &tools) {
            QJsonArray branches;
            QJsonObject definitions;
            for (const auto &target : tools) {
                if (target.operationId == QStringLiteral("automation.get_options")) {
                    continue;
                }
                QSet<QString> paths;
                for (const auto &sourceValue : target.valueSources) {
                    paths.insert(sourceValue.toObject()
                                     .value(QStringLiteral("field_path"))
                                     .toString());
                }
                if (paths.isEmpty())
                    continue;
                const auto definitionKey = target.trackingId;
                definitions.insert(definitionKey, partialRootSchema(target.inputSchema));
                auto orderedPaths = paths.values();
                std::sort(orderedPaths.begin(), orderedPaths.end());
                for (const auto &fieldPath : std::as_const(orderedPaths)) {
                    branches.append(JsonSchema::object(
                        {
                            {QStringLiteral("operation_id"),
                             JsonSchema::constant(target.operationId)},
                            {QStringLiteral("field_path"), fieldPathSchema(fieldPath)},
                            {QStringLiteral("partial_arguments"),
                             JsonSchema::reference(QStringLiteral("#/$defs/") + definitionKey)},
                        },
                        {QStringLiteral("operation_id"), QStringLiteral("field_path")}));
                }
            }
            return JsonSchema::document(JsonSchema::oneOf(branches), definitions);
        }

        QJsonObject objectRefSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("kind"), JsonSchema::string(publicObjectKindValues())},
                    {QStringLiteral("id"), identifierSchema()},
                },
                {QStringLiteral("kind"), QStringLiteral("id")});
        }

        QJsonObject mutationObjectSchema(const bool nullableVersions = false) {
            const auto createdObject = JsonSchema::object(
                {
                    {QStringLiteral("client_ref"), JsonSchema::string()},
                    {QStringLiteral("object"), objectRefSchema()},
                },
                {QStringLiteral("client_ref"), QStringLiteral("object")});
            const auto version = nullableVersions
                                     ? JsonSchema::oneOf(QJsonArray{
                                           documentVersionSchema(), JsonSchema::null()})
                                     : documentVersionSchema();
            return JsonSchema::object(
                {
                    {QStringLiteral("previous"), version},
                    {QStringLiteral("current"), version},
                    {QStringLiteral("changed"), JsonSchema::boolean()},
                    {QStringLiteral("validated_only"), JsonSchema::boolean()},
                    {QStringLiteral("affected_objects"), JsonSchema::array(objectRefSchema())},
                    {QStringLiteral("created_objects"), JsonSchema::array(createdObject)},
                    {QStringLiteral("warnings"), JsonSchema::array(JsonSchema::string())},
                },
                {QStringLiteral("previous"), QStringLiteral("current"), QStringLiteral("changed"),
                 QStringLiteral("validated_only"), QStringLiteral("affected_objects"),
                 QStringLiteral("created_objects"), QStringLiteral("warnings")});
        }

        QJsonObject mutationSchema(const bool nullableVersions = false) {
            return JsonSchema::document(mutationObjectSchema(nullableVersions));
        }

        QJsonObject taskAcceptedSchema(const bool nullableDocument = false) {
            const auto acceptedDocument = nullableDocument
                                              ? JsonSchema::oneOf(QJsonArray{
                                                    documentVersionSchema(), JsonSchema::null()})
                                              : documentVersionSchema();
            const auto accepted = JsonSchema::object(
                {
                    {QStringLiteral("task_id"), uuidSchema()},
                    {QStringLiteral("document"), acceptedDocument},
                    {QStringLiteral("validated_only"), JsonSchema::constant(false)},
                },
                {QStringLiteral("task_id"), QStringLiteral("document"),
                 QStringLiteral("validated_only")});
            const auto validated = JsonSchema::object(
                {
                    {QStringLiteral("document"),
                     JsonSchema::oneOf(
                         QJsonArray{documentVersionSchema(), JsonSchema::null()})},
                    {QStringLiteral("validated_only"), JsonSchema::constant(true)},
                },
                {QStringLiteral("document"), QStringLiteral("validated_only")});
            return JsonSchema::document(
                JsonSchema::oneOf(QJsonArray{accepted, validated}));
        }

        QJsonObject taskSnapshotObjectSchema() {
            const auto progress = JsonSchema::object(
                {
                    {QStringLiteral("minimum"), JsonSchema::integer()},
                    {QStringLiteral("maximum"), JsonSchema::integer()},
                    {QStringLiteral("value"), JsonSchema::integer()},
                    {QStringLiteral("indeterminate"), JsonSchema::boolean()},
                },
                {QStringLiteral("minimum"), QStringLiteral("maximum"), QStringLiteral("value"),
                 QStringLiteral("indeterminate")});
            const auto taskError = JsonSchema::object(
                {
                    {QStringLiteral("code"), nonEmptyStringSchema()},
                    {QStringLiteral("message"), JsonSchema::string()},
                    {QStringLiteral("field_path"), JsonSchema::string()},
                },
                {QStringLiteral("code"), QStringLiteral("message")});
            QJsonArray branches;
            for (const auto &operation : publicValueDomainValues(PublicValueDomain::TaskKind)) {
                const bool opensDocument = operation == QStringLiteral("documents.open");
                const auto document = opensDocument
                                          ? JsonSchema::oneOf(QJsonArray{
                                                documentVersionSchema(), JsonSchema::null()})
                                          : documentVersionSchema();
                branches.append(JsonSchema::object(
                    {
                        {QStringLiteral("task_id"), uuidSchema()},
                        {QStringLiteral("operation_id"), JsonSchema::constant(operation)},
                        {QStringLiteral("document"), document},
                        {QStringLiteral("state"),
                         stringDomainSchema(PublicValueDomain::TaskState)},
                        {QStringLiteral("progress"), progress},
                        {QStringLiteral("result"), mutationObjectSchema(opensDocument)},
                        {QStringLiteral("error"), taskError},
                        {QStringLiteral("created_by_client_id"), JsonSchema::string()},
                    },
                    {QStringLiteral("task_id"), QStringLiteral("operation_id"),
                     QStringLiteral("document"), QStringLiteral("state"),
                     QStringLiteral("progress")}));
            }
            return JsonSchema::oneOf(branches);
        }

        QJsonObject taskSnapshotSchema() {
            return JsonSchema::document(taskSnapshotObjectSchema());
        }

        QJsonObject digestSchema() {
            auto result = JsonSchema::string({}, 71, 71);
            result.insert(QStringLiteral("pattern"), QStringLiteral("^sha256:[0-9a-f]{64}$"));
            return result;
        }

        QJsonObject automationProfileSchema() {
            return stringDomainSchema(PublicValueDomain::AutomationProfile);
        }

        QJsonObject hostModeSchema() {
            return stringDomainSchema(PublicValueDomain::HostMode);
        }

        QJsonObject jsonValueMetaSchema() {
            const auto self = JsonSchema::reference(QStringLiteral("#/$defs/json_value"));
            return JsonSchema::oneOf(QJsonArray{
                JsonSchema::null(),
                JsonSchema::boolean(),
                JsonSchema::number(),
                JsonSchema::string(),
                JsonSchema::array(self),
                JsonSchema::objectWithAdditionalSchema({}, {}, self),
            });
        }

        QJsonObject jsonSchemaMetaSchema() {
            const auto schemaRef = JsonSchema::reference(QStringLiteral("#/$defs/schema"));
            const auto valueRef = JsonSchema::reference(QStringLiteral("#/$defs/json_value"));
            const auto typeName = JsonSchema::string({
                QStringLiteral("null"),    QStringLiteral("boolean"),
                QStringLiteral("object"),  QStringLiteral("array"),
                QStringLiteral("number"),  QStringLiteral("integer"),
                QStringLiteral("string"),
            });
            auto typeNames = JsonSchema::array(typeName, 1);
            typeNames.insert(QStringLiteral("uniqueItems"), true);
            const auto schemaObject = JsonSchema::object(
                {
                    {QStringLiteral("$schema"), nonEmptyStringSchema()},
                    {QStringLiteral("$id"), JsonSchema::string()},
                    {QStringLiteral("$anchor"), JsonSchema::string()},
                    {QStringLiteral("$defs"),
                     JsonSchema::objectWithAdditionalSchema({}, {}, schemaRef)},
                    {QStringLiteral("$ref"), nonEmptyStringSchema()},
                    {QStringLiteral("$comment"), JsonSchema::string()},
                    {QStringLiteral("title"), JsonSchema::string()},
                    {QStringLiteral("description"), JsonSchema::string()},
                    {QStringLiteral("default"), valueRef},
                    {QStringLiteral("examples"), JsonSchema::array(valueRef)},
                    {QStringLiteral("deprecated"), JsonSchema::boolean()},
                    {QStringLiteral("readOnly"), JsonSchema::boolean()},
                    {QStringLiteral("writeOnly"), JsonSchema::boolean()},
                    {QStringLiteral("type"),
                     JsonSchema::oneOf(QJsonArray{typeName, typeNames})},
                    {QStringLiteral("properties"),
                     JsonSchema::objectWithAdditionalSchema({}, {}, schemaRef)},
                    {QStringLiteral("required"), JsonSchema::array(JsonSchema::string())},
                    {QStringLiteral("additionalProperties"), schemaRef},
                    {QStringLiteral("items"), schemaRef},
                    {QStringLiteral("enum"), JsonSchema::array(valueRef, 1)},
                    {QStringLiteral("const"), valueRef},
                    {QStringLiteral("minimum"), JsonSchema::number()},
                    {QStringLiteral("maximum"), JsonSchema::number()},
                    {QStringLiteral("exclusiveMinimum"), JsonSchema::number()},
                    {QStringLiteral("exclusiveMaximum"), JsonSchema::number()},
                    {QStringLiteral("multipleOf"), JsonSchema::number()},
                    {QStringLiteral("minItems"), JsonSchema::integer(0.0)},
                    {QStringLiteral("maxItems"), JsonSchema::integer(0.0)},
                    {QStringLiteral("uniqueItems"), JsonSchema::boolean()},
                    {QStringLiteral("minLength"), JsonSchema::integer(0.0)},
                    {QStringLiteral("maxLength"), JsonSchema::integer(0.0)},
                    {QStringLiteral("pattern"), JsonSchema::string()},
                    {QStringLiteral("format"),
                     JsonSchema::string({QStringLiteral("uuid"), QStringLiteral("uri")})},
                    {QStringLiteral("minProperties"), JsonSchema::integer(0.0)},
                    {QStringLiteral("maxProperties"), JsonSchema::integer(0.0)},
                    {QStringLiteral("oneOf"), JsonSchema::array(schemaRef, 1)},
                    {QStringLiteral("x-mcp-header"), JsonSchema::string()},
                });
            return JsonSchema::oneOf(QJsonArray{JsonSchema::boolean(), schemaObject});
        }

        QJsonObject manifestOperationSchema() {
            const auto valueSource = JsonSchema::object(
                {
                    {QStringLiteral("field_path"), nonEmptyStringSchema()},
                    {QStringLiteral("operation_id"), nonEmptyStringSchema()},
                    {QStringLiteral("context_fields"),
                     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("minimum_profile"), automationProfileSchema()},
                    {QStringLiteral("host_availability"),
                     stringDomainSchema(PublicValueDomain::HostAvailability)},
                },
                {QStringLiteral("field_path"), QStringLiteral("operation_id"),
                 QStringLiteral("context_fields"), QStringLiteral("minimum_profile"),
                 QStringLiteral("host_availability")});
            const auto safety = JsonSchema::object(
                {
                    {QStringLiteral("read_only"), JsonSchema::boolean()},
                    {QStringLiteral("destructive"), JsonSchema::boolean()},
                    {QStringLiteral("idempotent"), JsonSchema::boolean()},
                    {QStringLiteral("open_world"), JsonSchema::boolean()},
                },
                {QStringLiteral("read_only"), QStringLiteral("destructive"),
                 QStringLiteral("idempotent"), QStringLiteral("open_world")});
            const auto schemaDigest = JsonSchema::object(
                {
                    {QStringLiteral("input"), digestSchema()},
                    {QStringLiteral("output"), digestSchema()},
                },
                {QStringLiteral("input"), QStringLiteral("output")});
            return JsonSchema::object(
                {
                    {QStringLiteral("operation_id"), nonEmptyStringSchema()},
                    {QStringLiteral("title"), nonEmptyStringSchema()},
                    {QStringLiteral("description"), nonEmptyStringSchema()},
                    {QStringLiteral("version"),
                     JsonSchema::integer(1.0, static_cast<double>(MaximumSafeJsonInteger))},
                    {QStringLiteral("minimum_compatible_version"),
                     JsonSchema::integer(1.0, static_cast<double>(MaximumSafeJsonInteger))},
                    {QStringLiteral("category"), nonEmptyStringSchema()},
                    {QStringLiteral("kind"),
                     stringDomainSchema(PublicValueDomain::OperationKind)},
                    {QStringLiteral("input_schema"),
                     JsonSchema::reference(QStringLiteral("#/$defs/schema"))},
                    {QStringLiteral("output_schema"),
                     JsonSchema::reference(QStringLiteral("#/$defs/schema"))},
                    {QStringLiteral("value_sources"), JsonSchema::array(valueSource)},
                    {QStringLiteral("minimum_profile"), automationProfileSchema()},
                    {QStringLiteral("sync_mode"),
                     stringDomainSchema(PublicValueDomain::SyncMode)},
                    {QStringLiteral("document_policy"),
                     stringDomainSchema(PublicValueDomain::DocumentPolicy)},
                    {QStringLiteral("revision_policy"),
                     stringDomainSchema(PublicValueDomain::RevisionPolicy)},
                    {QStringLiteral("history_policy"),
                     stringDomainSchema(PublicValueDomain::HistoryPolicy)},
                    {QStringLiteral("file_access"),
                     stringDomainSchema(PublicValueDomain::FileAccess)},
                    {QStringLiteral("host_availability"),
                     stringDomainSchema(PublicValueDomain::HostAvailability)},
                    {QStringLiteral("concurrency_scope"),
                     stringDomainSchema(PublicValueDomain::ConcurrencyScope)},
                    {QStringLiteral("conflict_policy"),
                     stringDomainSchema(PublicValueDomain::ConflictPolicy)},
                    {QStringLiteral("safety_metadata"), safety},
                    {QStringLiteral("introduced_version"),
                     JsonSchema::integer(1.0, static_cast<double>(MaximumSafeJsonInteger))},
                    {QStringLiteral("schema_digest"), schemaDigest},
                },
                {
                    QStringLiteral("operation_id"), QStringLiteral("title"),
                    QStringLiteral("description"), QStringLiteral("version"),
                    QStringLiteral("minimum_compatible_version"), QStringLiteral("category"),
                    QStringLiteral("kind"), QStringLiteral("input_schema"),
                    QStringLiteral("output_schema"), QStringLiteral("value_sources"),
                    QStringLiteral("minimum_profile"), QStringLiteral("sync_mode"),
                    QStringLiteral("document_policy"), QStringLiteral("revision_policy"),
                    QStringLiteral("history_policy"), QStringLiteral("file_access"),
                    QStringLiteral("host_availability"), QStringLiteral("concurrency_scope"),
                    QStringLiteral("conflict_policy"), QStringLiteral("safety_metadata"),
                    QStringLiteral("introduced_version"), QStringLiteral("schema_digest"),
                });
        }

        QJsonObject manifestOutputSchema() {
            const auto root = JsonSchema::object(
                {
                    {QStringLiteral("toolset_version"),
                     JsonSchema::integer(1.0, static_cast<double>(MaximumSafeJsonInteger))},
                    {QStringLiteral("digest"), digestSchema()},
                    {QStringLiteral("profile"), automationProfileSchema()},
                    {QStringLiteral("host_mode"), hostModeSchema()},
                    {QStringLiteral("operations"),
                     JsonSchema::array(
                         JsonSchema::reference(QStringLiteral("#/$defs/operation")))},
                    {QStringLiteral("extensions"),
                     JsonSchema::objectWithAdditionalSchema({}, {}, QJsonValue(true))},
                    {QStringLiteral("next_cursor"), JsonSchema::string()},
                },
                {QStringLiteral("toolset_version"), QStringLiteral("digest"),
                 QStringLiteral("profile"), QStringLiteral("host_mode"),
                 QStringLiteral("operations"), QStringLiteral("extensions")});
            return JsonSchema::document(
                root,
                {
                    {QStringLiteral("json_value"), jsonValueMetaSchema()},
                    {QStringLiteral("schema"), jsonSchemaMetaSchema()},
                    {QStringLiteral("operation"), manifestOperationSchema()},
                });
        }

        QJsonObject queryEnvelopeSchema(const QString &name, const QJsonValue &valueSchema,
                                        const bool nextCursor = false) {
            QJsonObject properties{
                {QStringLiteral("document"), documentVersionSchema()},
                {name, valueSchema},
            };
            if (nextCursor)
                properties.insert(QStringLiteral("next_cursor"), JsonSchema::string());
            return JsonSchema::document(JsonSchema::object(
                properties, {QStringLiteral("document"), name}));
        }

        QJsonObject statusOutputSchema() {
            const auto manifest = JsonSchema::object(
                {
                    {QStringLiteral("toolset_version"),
                     JsonSchema::integer(1.0, static_cast<double>(MaximumSafeJsonInteger))},
                    {QStringLiteral("digest"), digestSchema()},
                },
                {QStringLiteral("toolset_version"), QStringLiteral("digest")});
            const auto document = JsonSchema::object(
                {
                    {QStringLiteral("document_id"), uuidSchema()},
                    {QStringLiteral("revision"), revisionSchema()},
                    {QStringLiteral("active"), JsonSchema::boolean()},
                },
                {QStringLiteral("document_id"), QStringLiteral("revision")});
            const auto window = JsonSchema::object(
                {
                    {QStringLiteral("window_id"), nonEmptyStringSchema()},
                    {QStringLiteral("document_id"), uuidSchema()},
                    {QStringLiteral("active"), JsonSchema::boolean()},
                },
                {QStringLiteral("window_id"), QStringLiteral("document_id")});
            auto documents = JsonSchema::array(document);
            documents.insert(QStringLiteral("maxItems"), 1);
            auto windows = JsonSchema::array(window);
            windows.insert(QStringLiteral("maxItems"), 1);
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("editor_instance_id"), uuidSchema()},
                    {QStringLiteral("host_mode"), hostModeSchema()},
                    {QStringLiteral("profile"), automationProfileSchema()},
                    {QStringLiteral("manifest"), manifest},
                    {QStringLiteral("documents"), documents},
                    {QStringLiteral("windows"), windows},
                },
                {QStringLiteral("editor_instance_id"), QStringLiteral("host_mode"),
                 QStringLiteral("profile"), QStringLiteral("manifest"),
                 QStringLiteral("documents"), QStringLiteral("windows")}));
        }

        QJsonObject optionsOutputSchema() {
            const auto optionValue = JsonSchema::oneOf(QJsonArray{
                JsonSchema::null(),
                JsonSchema::boolean(),
                JsonSchema::number(),
                JsonSchema::string(),
                voiceRefSchema(),
            });
            const auto metadata = JsonSchema::object({
                {QStringLiteral("description"), JsonSchema::string()},
                {QStringLiteral("group"), JsonSchema::string()},
                {QStringLiteral("deprecated"), JsonSchema::boolean()},
                {QStringLiteral("minimum"), JsonSchema::integer()},
                {QStringLiteral("maximum"), JsonSchema::integer()},
                {QStringLiteral("step"), JsonSchema::integer(1.0)},
                {QStringLiteral("unit"), JsonSchema::string()},
            });
            const auto option = JsonSchema::object(
                {
                    {QStringLiteral("value"), optionValue},
                    {QStringLiteral("label"), JsonSchema::string()},
                    {QStringLiteral("available"), JsonSchema::boolean()},
                    {QStringLiteral("unavailable_reason"), JsonSchema::string()},
                    {QStringLiteral("metadata"), metadata},
                },
                {QStringLiteral("value"), QStringLiteral("label"),
                 QStringLiteral("available")});
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("operation_id"), nonEmptyStringSchema()},
                    {QStringLiteral("field_path"), nonEmptyStringSchema()},
                    {QStringLiteral("options"), JsonSchema::array(option)},
                    {QStringLiteral("dependencies"),
                     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("context_digest"), digestSchema()},
                },
                {QStringLiteral("operation_id"), QStringLiteral("field_path"),
                 QStringLiteral("options"), QStringLiteral("dependencies")}));
        }

        QJsonObject fileAccessOutputSchema() {
            const auto grant = JsonSchema::object(
                {
                    {QStringLiteral("path"), nonEmptyStringSchema()},
                    {QStringLiteral("access"),
                     stringDomainSchema(PublicValueDomain::GrantedFileAccess)},
                },
                {QStringLiteral("path"), QStringLiteral("access")});
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("read_roots"),
                     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("write_roots"),
                     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("session_grants"), JsonSchema::array(grant)},
                },
                {QStringLiteral("read_roots"), QStringLiteral("write_roots"),
                 QStringLiteral("session_grants")}));
        }

        QJsonObject documentSnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("path"), JsonSchema::string()},
                    {QStringLiteral("project_name"), JsonSchema::string()},
                    {QStringLiteral("busy"), JsonSchema::boolean()},
                    {QStringLiteral("saved"), JsonSchema::boolean()},
                    {QStringLiteral("dirty"), JsonSchema::boolean()},
                    {QStringLiteral("on_save_point"), JsonSchema::boolean()},
                    {QStringLiteral("lifecycle"),
                     JsonSchema::string(documentLifecycleValues())},
                },
                {QStringLiteral("path"), QStringLiteral("project_name"),
                 QStringLiteral("busy"), QStringLiteral("saved"),
                 QStringLiteral("dirty"), QStringLiteral("on_save_point"),
                 QStringLiteral("lifecycle")});
        }

        QJsonObject projectSnapshotSchema() {
            const auto trackProperties = JsonSchema::object(
                {
                    {QStringLiteral("name"), JsonSchema::string()},
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                    {QStringLiteral("solo"), JsonSchema::boolean()},
                },
                {QStringLiteral("name"), QStringLiteral("gain"), QStringLiteral("pan"),
                 QStringLiteral("mute"), QStringLiteral("solo")});
            const auto clipProperties = JsonSchema::object(
                {
                    {QStringLiteral("name"), JsonSchema::string()},
                    {QStringLiteral("start"), JsonSchema::integer(0.0)},
                    {QStringLiteral("length"), JsonSchema::integer(1.0)},
                    {QStringLiteral("clip_start"), JsonSchema::integer(0.0)},
                    {QStringLiteral("clip_length"), JsonSchema::integer(1.0)},
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                },
                {QStringLiteral("name"), QStringLiteral("start"), QStringLiteral("length"),
                 QStringLiteral("clip_start"), QStringLiteral("clip_length"),
                 QStringLiteral("gain"), QStringLiteral("mute")});
            const auto clip = JsonSchema::object(
                {
                    {QStringLiteral("clip_id"), identifierSchema()},
                    {QStringLiteral("track_id"), identifierSchema()},
                    {QStringLiteral("type"), stringDomainSchema(PublicValueDomain::ClipType)},
                    {QStringLiteral("properties"), clipProperties},
                    {QStringLiteral("default_language"), JsonSchema::string()},
                },
                {QStringLiteral("clip_id"), QStringLiteral("track_id"),
                 QStringLiteral("type"), QStringLiteral("properties"),
                 QStringLiteral("default_language")});
            const auto track = JsonSchema::object(
                {
                    {QStringLiteral("track_id"), identifierSchema()},
                    {QStringLiteral("properties"), trackProperties},
                    {QStringLiteral("color_index"),
                     JsonSchema::integer(0.0, TrackPaletteColorCount - 1)},
                    {QStringLiteral("default_language"), JsonSchema::string()},
                    {QStringLiteral("clips"), JsonSchema::array(clip)},
                },
                {QStringLiteral("track_id"), QStringLiteral("properties"),
                 QStringLiteral("color_index"), QStringLiteral("default_language"),
                 QStringLiteral("clips")});
            return JsonSchema::object(
                {{QStringLiteral("tracks"), JsonSchema::array(track)}},
                {QStringLiteral("tracks")});
        }

        QJsonObject noteSnapshotSchema() {
            const auto pronunciation = JsonSchema::object(
                {
                    {QStringLiteral("value"), JsonSchema::string()},
                    {QStringLiteral("source"),
                     stringDomainSchema(PublicValueDomain::PronunciationSource)},
                },
                {QStringLiteral("value")});
            const auto phoneme = JsonSchema::object(
                {
                    {QStringLiteral("symbol"), nonEmptyStringSchema()},
                    {QStringLiteral("language"), JsonSchema::string()},
                    {QStringLiteral("offset"), JsonSchema::integer()},
                },
                {QStringLiteral("symbol")});
            return JsonSchema::object(
                {
                    {QStringLiteral("note_id"), identifierSchema()},
                    {QStringLiteral("clip_id"), identifierSchema()},
                    {QStringLiteral("client_ref"), JsonSchema::string()},
                    {QStringLiteral("local_start"), JsonSchema::integer(0.0)},
                    {QStringLiteral("length"), JsonSchema::integer(1.0)},
                    {QStringLiteral("key_index"),
                     JsonSchema::integer(MinimumMidiKeyIndex, MaximumMidiKeyIndex)},
                    {QStringLiteral("cent_shift"),
                     JsonSchema::integer(MinimumCentShift, MaximumCentShift)},
                    {QStringLiteral("lyric"), JsonSchema::string()},
                    {QStringLiteral("language"), JsonSchema::string()},
                    {QStringLiteral("pronunciation"), pronunciation},
                    {QStringLiteral("pronunciation_candidates"),
                     JsonSchema::array(JsonSchema::string())},
                    {QStringLiteral("phonemes"), JsonSchema::array(phoneme)},
                    {QStringLiteral("line_feed"), JsonSchema::boolean()},
                },
                {QStringLiteral("note_id"), QStringLiteral("clip_id"),
                 QStringLiteral("client_ref"), QStringLiteral("local_start"),
                 QStringLiteral("length"), QStringLiteral("key_index"),
                 QStringLiteral("cent_shift"), QStringLiteral("lyric"),
                 QStringLiteral("language"), QStringLiteral("line_feed")});
        }

        QJsonObject parameterNameSchema() {
            return stringDomainSchema(PublicValueDomain::ParameterName);
        }

        QJsonObject parameterLayerSchema() {
            return stringDomainSchema(PublicValueDomain::ParameterLayer);
        }

        QJsonObject interpolationSchema() {
            return stringDomainSchema(PublicValueDomain::Interpolation);
        }

        QJsonObject curveSnapshotSchema() {
            const auto draw = JsonSchema::object(
                {
                    {QStringLiteral("type"),
                     domainConstant(PublicValueDomain::CurveType, QStringLiteral("draw"))},
                    {QStringLiteral("curve_id"), identifierSchema()},
                    {QStringLiteral("local_start"), JsonSchema::integer()},
                    {QStringLiteral("step"), JsonSchema::integer(1.0)},
                    {QStringLiteral("values"), JsonSchema::array(JsonSchema::integer())},
                },
                {QStringLiteral("type"), QStringLiteral("curve_id"),
                 QStringLiteral("local_start"),
                 QStringLiteral("step"), QStringLiteral("values")});
            const auto anchorNode = JsonSchema::object(
                {
                    {QStringLiteral("anchor_id"), identifierSchema()},
                    {QStringLiteral("position"), JsonSchema::integer()},
                    {QStringLiteral("value"), JsonSchema::integer()},
                    {QStringLiteral("interpolation"), interpolationSchema()},
                },
                {QStringLiteral("anchor_id"), QStringLiteral("position"), QStringLiteral("value"),
                 QStringLiteral("interpolation")});
            const auto anchor = JsonSchema::object(
                {
                    {QStringLiteral("type"),
                     domainConstant(PublicValueDomain::CurveType, QStringLiteral("anchor"))},
                    {QStringLiteral("curve_id"), identifierSchema()},
                    {QStringLiteral("nodes"), JsonSchema::array(anchorNode)},
                },
                {QStringLiteral("type"), QStringLiteral("curve_id"),
                 QStringLiteral("nodes")});
            return JsonSchema::oneOf(QJsonArray{draw, anchor});
        }

        QJsonObject parameterSnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("clip_id"), identifierSchema()},
                    {QStringLiteral("name"), parameterNameSchema()},
                    {QStringLiteral("type"), parameterLayerSchema()},
                    {QStringLiteral("curves"), JsonSchema::array(curveSnapshotSchema())},
                },
                {QStringLiteral("clip_id"), QStringLiteral("name"),
                 QStringLiteral("type"), QStringLiteral("curves")});
        }

        QJsonObject parameterCapabilitiesSchema() {
            const auto range = JsonSchema::object(
                {
                    {QStringLiteral("minimum"), JsonSchema::integer()},
                    {QStringLiteral("maximum"), JsonSchema::integer()},
                    {QStringLiteral("step"), JsonSchema::integer(1.0)},
                    {QStringLiteral("unit"), JsonSchema::string()},
                },
                {QStringLiteral("minimum"), QStringLiteral("maximum"),
                 QStringLiteral("step"), QStringLiteral("unit")});
            const auto parameter = JsonSchema::object(
                {
                    {QStringLiteral("name"), parameterNameSchema()},
                    {QStringLiteral("types"),
                     JsonSchema::array(parameterLayerSchema(), 1)},
                    {QStringLiteral("curve_types"),
                     JsonSchema::array(stringDomainSchema(PublicValueDomain::CurveType),
                                       1)},
                    {QStringLiteral("interpolations"),
                     JsonSchema::array(interpolationSchema(), 1)},
                    {QStringLiteral("editable"), JsonSchema::boolean()},
                    {QStringLiteral("range"), range},
                },
                {QStringLiteral("name"), QStringLiteral("types"),
                 QStringLiteral("curve_types"), QStringLiteral("interpolations"),
                 QStringLiteral("editable"), QStringLiteral("range")});
            return JsonSchema::object(
                {
                    {QStringLiteral("clip_id"), identifierSchema()},
                    {QStringLiteral("parameters"), JsonSchema::array(parameter)},
                },
                {QStringLiteral("clip_id"), QStringLiteral("parameters")});
        }

        QJsonObject timelineSnapshotSchema() {
            const auto tempo = JsonSchema::object(
                {
                    {QStringLiteral("tick"), JsonSchema::integer()},
                    {QStringLiteral("tempo"), JsonSchema::number(1.0)},
                },
                {QStringLiteral("tick"), QStringLiteral("tempo")});
            const auto signature = JsonSchema::object(
                {
                    {QStringLiteral("bar_index"), JsonSchema::integer(0.0)},
                    {QStringLiteral("numerator"), JsonSchema::integer(1.0)},
                    {QStringLiteral("denominator"), JsonSchema::integer(1.0)},
                },
                {QStringLiteral("bar_index"), QStringLiteral("numerator"),
                 QStringLiteral("denominator")});
            return JsonSchema::object(
                {
                    {QStringLiteral("tempos"), JsonSchema::array(tempo)},
                    {QStringLiteral("time_signatures"), JsonSchema::array(signature)},
                },
                {QStringLiteral("tempos"), QStringLiteral("time_signatures")});
        }

        QJsonObject historySnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("can_undo"), JsonSchema::boolean()},
                    {QStringLiteral("can_redo"), JsonSchema::boolean()},
                    {QStringLiteral("on_save_point"), JsonSchema::boolean()},
                    {QStringLiteral("undo_name"), JsonSchema::string()},
                    {QStringLiteral("redo_name"), JsonSchema::string()},
                },
                {QStringLiteral("can_undo"), QStringLiteral("can_redo"),
                 QStringLiteral("on_save_point"), QStringLiteral("undo_name"),
                 QStringLiteral("redo_name")});
        }

        QJsonObject voiceSummarySchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("package_id"), nonEmptyStringSchema()},
                    {QStringLiteral("singer_id"), nonEmptyStringSchema()},
                    {QStringLiteral("name"), nonEmptyStringSchema()},
                    {QStringLiteral("version"), nonEmptyStringSchema()},
                },
                {QStringLiteral("package_id"), QStringLiteral("singer_id"),
                 QStringLiteral("name"), QStringLiteral("version")});
        }

        QJsonObject voiceSnapshotSchema() {
            const auto optionalIdentifier = JsonSchema::oneOf(
                QJsonArray{nonEmptyStringSchema(), JsonSchema::null()});
            const auto speaker = JsonSchema::object(
                {
                    {QStringLiteral("speaker_id"), nonEmptyStringSchema()},
                    {QStringLiteral("name"), JsonSchema::string()},
                    {QStringLiteral("languages"),
                     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("mixable"), JsonSchema::boolean()},
                    {QStringLiteral("default"), JsonSchema::boolean()},
                },
                {QStringLiteral("speaker_id"), QStringLiteral("name"),
                 QStringLiteral("languages"), QStringLiteral("mixable"),
                 QStringLiteral("default")});
            const auto language = JsonSchema::object(
                {
                    {QStringLiteral("language_id"), nonEmptyStringSchema()},
                    {QStringLiteral("name"), JsonSchema::string()},
                    {QStringLiteral("g2p_id"), JsonSchema::string()},
                    {QStringLiteral("g2p_ready"), JsonSchema::boolean()},
                    {QStringLiteral("default"), JsonSchema::boolean()},
                },
                {QStringLiteral("language_id"), QStringLiteral("name"),
                 QStringLiteral("g2p_id"), QStringLiteral("g2p_ready"),
                 QStringLiteral("default")});
            return JsonSchema::object(
                {
                    {QStringLiteral("package_id"), nonEmptyStringSchema()},
                    {QStringLiteral("singer_id"), nonEmptyStringSchema()},
                    {QStringLiteral("name"), nonEmptyStringSchema()},
                    {QStringLiteral("version"), nonEmptyStringSchema()},
                    {QStringLiteral("speakers"), JsonSchema::array(speaker)},
                    {QStringLiteral("languages"), JsonSchema::array(language)},
                    {QStringLiteral("default_speaker_id"), optionalIdentifier},
                    {QStringLiteral("default_language"), optionalIdentifier},
                    {QStringLiteral("g2p_ready"), JsonSchema::boolean()},
                    {QStringLiteral("resolution_state"),
                     stringDomainSchema(PublicValueDomain::VoiceResolutionState)},
                    {QStringLiteral("mixing_supported"), JsonSchema::boolean()},
                },
                {QStringLiteral("package_id"), QStringLiteral("singer_id"),
                 QStringLiteral("name"), QStringLiteral("version"),
                 QStringLiteral("speakers"), QStringLiteral("languages"),
                 QStringLiteral("default_speaker_id"), QStringLiteral("default_language"),
                 QStringLiteral("g2p_ready"), QStringLiteral("resolution_state"),
                 QStringLiteral("mixing_supported")});
        }

        QJsonObject formatCapabilitySchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("id"), nonEmptyStringSchema()},
                    {QStringLiteral("display_name"), nonEmptyStringSchema()},
                    {QStringLiteral("extensions"),
                     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("can_open"), JsonSchema::boolean()},
                    {QStringLiteral("can_import"), JsonSchema::boolean()},
                    {QStringLiteral("can_export"), JsonSchema::boolean()},
                    {QStringLiteral("available"), JsonSchema::boolean()},
                    {QStringLiteral("unavailable_reason"), JsonSchema::string()},
                    {QStringLiteral("option_schema"),
                     JsonSchema::reference(QStringLiteral("#/$defs/schema"))},
                },
                {QStringLiteral("id"), QStringLiteral("display_name"),
                 QStringLiteral("extensions"), QStringLiteral("can_open"),
                 QStringLiteral("can_import"), QStringLiteral("can_export"),
                 QStringLiteral("available"), QStringLiteral("unavailable_reason"),
                 QStringLiteral("option_schema")});
        }

        QJsonObject formatsOutputSchema() {
            const auto root = JsonSchema::object(
                {{QStringLiteral("formats"),
                  JsonSchema::array(formatCapabilitySchema())}},
                {QStringLiteral("formats")});
            return JsonSchema::document(
                root,
                {
                    {QStringLiteral("json_value"), jsonValueMetaSchema()},
                    {QStringLiteral("schema"), jsonSchemaMetaSchema()},
                });
        }

        QJsonObject audioExportCapabilitiesSchema() {
            const auto source = JsonSchema::object(
                {
                    {QStringLiteral("id"), identifierSchema()},
                    {QStringLiteral("name"), nonEmptyStringSchema()},
                    {QStringLiteral("kind"),
                     stringDomainSchema(PublicValueDomain::AudioSourceKind)},
                    {QStringLiteral("available"), JsonSchema::boolean()},
                },
                {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("kind"),
                 QStringLiteral("available")});
            return JsonSchema::object(
                {
                    {QStringLiteral("formats"),
                     JsonSchema::array(nonEmptyStringSchema(), 1)},
                    {QStringLiteral("sample_rates"),
                     JsonSchema::array(
                         JsonSchema::integer(MinimumAudioSampleRate, MaximumAudioSampleRate), 1)},
                    {QStringLiteral("channel_modes"),
                     JsonSchema::array(stringDomainSchema(PublicValueDomain::ChannelMode),
                                       1)},
                    {QStringLiteral("mixing_modes"),
                     JsonSchema::array(
                         stringDomainSchema(PublicValueDomain::AudioMixingMode), 1)},
                    {QStringLiteral("source_modes"),
                     JsonSchema::array(
                         stringDomainSchema(PublicValueDomain::AudioSourceMode),
                                       1)},
                    {QStringLiteral("sources"), JsonSchema::array(source)},
                },
                {QStringLiteral("formats"), QStringLiteral("sample_rates"),
                 QStringLiteral("channel_modes"), QStringLiteral("mixing_modes"),
                 QStringLiteral("source_modes"), QStringLiteral("sources")});
        }

        QJsonObject audioExportPreviewSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("base_directory"), JsonSchema::string()},
                    {QStringLiteral("file_paths"),
                     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("warning_flags"),
                     JsonSchema::integer(0.0, static_cast<double>(MaximumSafeJsonInteger))},
                },
                {QStringLiteral("base_directory"), QStringLiteral("file_paths"),
                 QStringLiteral("warning_flags")});
        }

        QJsonObject modelCapabilitySchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("model_id"), nonEmptyStringSchema()},
                    {QStringLiteral("display_name"), nonEmptyStringSchema()},
                    {QStringLiteral("available"), JsonSchema::boolean()},
                    {QStringLiteral("unavailable_reason"), JsonSchema::string()},
                },
                {QStringLiteral("model_id"), QStringLiteral("display_name"),
                 QStringLiteral("available"), QStringLiteral("unavailable_reason")});
        }

        QJsonObject extractionCapabilitiesSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("clip_id"), identifierSchema()},
                    {QStringLiteral("pitch"), JsonSchema::boolean()},
                    {QStringLiteral("midi"), JsonSchema::boolean()},
                    {QStringLiteral("pitch_models"),
                     JsonSchema::array(modelCapabilitySchema())},
                    {QStringLiteral("midi_models"),
                     JsonSchema::array(modelCapabilitySchema())},
                    {QStringLiteral("languages"),
                     JsonSchema::array(nonEmptyStringSchema())},
                },
                {QStringLiteral("clip_id"), QStringLiteral("pitch"),
                 QStringLiteral("midi"), QStringLiteral("pitch_models"),
                 QStringLiteral("midi_models"), QStringLiteral("languages")});
        }

        QJsonObject inferenceCapabilitiesSchema() {
            const auto emptyScope = JsonSchema::object();
            const auto scope = JsonSchema::oneOf(
                QJsonArray{emptyScope, inferenceScopeSchema()});
            const auto namedCapability = JsonSchema::object(
                {
                    {QStringLiteral("id"), nonEmptyStringSchema()},
                    {QStringLiteral("display_name"), nonEmptyStringSchema()},
                    {QStringLiteral("available"), JsonSchema::boolean()},
                    {QStringLiteral("unavailable_reason"), JsonSchema::string()},
                },
                {QStringLiteral("id"), QStringLiteral("display_name"),
                 QStringLiteral("available"), QStringLiteral("unavailable_reason")});
            return JsonSchema::object(
                {
                    {QStringLiteral("scope"), scope},
                    {QStringLiteral("stages"),
                     JsonSchema::array(
                         stringDomainSchema(PublicValueDomain::InferenceStage), 1)},
                    {QStringLiteral("providers"), JsonSchema::array(namedCapability)},
                    {QStringLiteral("devices"), JsonSchema::array(namedCapability)},
                    {QStringLiteral("models"), JsonSchema::array(modelCapabilitySchema())},
                },
                {QStringLiteral("scope"), QStringLiteral("stages"),
                 QStringLiteral("providers"), QStringLiteral("devices"),
                 QStringLiteral("models")});
        }

        QJsonObject playbackSnapshotSchema() {
            const auto loop = JsonSchema::object(
                {
                    {QStringLiteral("enabled"), JsonSchema::boolean()},
                    {QStringLiteral("start"), JsonSchema::number(0.0)},
                    {QStringLiteral("end"), JsonSchema::number(0.0)},
                },
                {QStringLiteral("enabled"), QStringLiteral("start"),
                 QStringLiteral("end")});
            return JsonSchema::object(
                {
                    {QStringLiteral("state"),
                     stringDomainSchema(PublicValueDomain::PlaybackState)},
                    {QStringLiteral("position"), JsonSchema::number(0.0)},
                    {QStringLiteral("last_position"), JsonSchema::number(0.0)},
                    {QStringLiteral("loop"), loop},
                },
                {QStringLiteral("state"), QStringLiteral("position"),
                 QStringLiteral("last_position"), QStringLiteral("loop")});
        }

        QJsonObject queryOutputSchema(const QString &id) {
            if (id == QStringLiteral("application.get_info")) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("name"), nonEmptyStringSchema()},
                        {QStringLiteral("version"), nonEmptyStringSchema()},
                        {QStringLiteral("platform"), nonEmptyStringSchema()},
                    },
                    {QStringLiteral("name"), QStringLiteral("version"),
                     QStringLiteral("platform")}));
            }
            if (id == QStringLiteral("automation.get_status"))
                return statusOutputSchema();
            if (id == QStringLiteral("automation.get_manifest"))
                return manifestOutputSchema();
            if (id == QStringLiteral("automation.get_options"))
                return optionsOutputSchema();
            if (id == QStringLiteral("automation.get_file_access"))
                return fileAccessOutputSchema();
            if (id == QStringLiteral("documents.get"))
                return queryEnvelopeSchema(QStringLiteral("snapshot"), documentSnapshotSchema());
            if (id == QStringLiteral("project.get"))
                return queryEnvelopeSchema(QStringLiteral("snapshot"), projectSnapshotSchema());
            if (id == QStringLiteral("notes.get")) {
                return queryEnvelopeSchema(QStringLiteral("notes"),
                                           JsonSchema::array(noteSnapshotSchema()));
            }
            if (id == QStringLiteral("parameters.get"))
                return queryEnvelopeSchema(QStringLiteral("snapshot"), parameterSnapshotSchema());
            if (id == QStringLiteral("parameters.get_capabilities")) {
                return queryEnvelopeSchema(QStringLiteral("capabilities"),
                                           parameterCapabilitiesSchema());
            }
            if (id == QStringLiteral("timeline.get"))
                return queryEnvelopeSchema(QStringLiteral("snapshot"), timelineSnapshotSchema());
            if (id == QStringLiteral("history.get_state"))
                return queryEnvelopeSchema(QStringLiteral("snapshot"), historySnapshotSchema());
            if (id == QStringLiteral("voices.list")) {
                return JsonSchema::document(JsonSchema::object(
                    {{QStringLiteral("voices"), JsonSchema::array(voiceSummarySchema())}},
                    {QStringLiteral("voices")}));
            }
            if (id == QStringLiteral("voices.describe")) {
                return JsonSchema::document(JsonSchema::object(
                    {{QStringLiteral("snapshot"), voiceSnapshotSchema()}},
                    {QStringLiteral("snapshot")}));
            }
            if (id == QStringLiteral("formats.list"))
                return formatsOutputSchema();
            if (id == QStringLiteral("exports.audio.get_capabilities")) {
                return queryEnvelopeSchema(QStringLiteral("capabilities"),
                                           audioExportCapabilitiesSchema());
            }
            if (id == QStringLiteral("exports.audio.preview")) {
                return queryEnvelopeSchema(QStringLiteral("snapshot"),
                                           audioExportPreviewSchema());
            }
            if (id == QStringLiteral("extract.get_capabilities")) {
                return queryEnvelopeSchema(QStringLiteral("capabilities"),
                                           extractionCapabilitiesSchema());
            }
            if (id == QStringLiteral("inference.get_capabilities")) {
                return queryEnvelopeSchema(QStringLiteral("capabilities"),
                                           inferenceCapabilitiesSchema());
            }
            if (id == QStringLiteral("tasks.list")) {
                return queryEnvelopeSchema(QStringLiteral("tasks"),
                                           JsonSchema::array(taskSnapshotObjectSchema()), true);
            }
            if (id == QStringLiteral("tasks.get") || id == QStringLiteral("tasks.cancel"))
                return taskSnapshotSchema();
            if (id == QStringLiteral("playback.get"))
                return queryEnvelopeSchema(QStringLiteral("snapshot"), playbackSnapshotSchema());
            qFatal("No explicit public query output schema for operation '%s'", qPrintable(id));
            return {};
        }

        QJsonObject outputSchema(const QString &id, const OperationKind kind,
                                 const SyncMode syncMode) {
            if (syncMode == SyncMode::Asynchronous)
                return taskAcceptedSchema(id == QStringLiteral("documents.open"));
            if (kind == OperationKind::Command)
                return id == QStringLiteral("tasks.cancel")
                           ? taskSnapshotSchema()
                           : mutationSchema(id == QStringLiteral("documents.new"));
            return queryOutputSchema(id);
        }

        QString humanTitle(const QString &operationId) {
            auto result = operationId;
            result.replace(u'.', u' ');
            result.replace(u'_', u' ');
            if (!result.isEmpty())
                result[0] = result.at(0).toUpper();
            return result;
        }

        QJsonObject valueSource(const QString &fieldPath, const QString &sourceOperation,
                                const QStringList &contextFields = {}) {
            QJsonArray context;
            for (const auto &field : contextFields)
                context.append(field);
            const auto sourceProfile =
                sourceOperation == QStringLiteral("voices.list") ||
                        sourceOperation == QStringLiteral("voices.describe") ||
                        sourceOperation == QStringLiteral("parameters.get_capabilities")
                    ? AutomationProfile::L1
                    : AutomationProfile::L2;
            return {
                {QStringLiteral("field_path"), fieldPath},
                {QStringLiteral("operation_id"), sourceOperation},
                {QStringLiteral("context_fields"), context},
                {QStringLiteral("minimum_profile"), automationProfileName(sourceProfile)},
                {QStringLiteral("host_availability"), QStringLiteral("gui")},
            };
        }

        QJsonArray valueSources(const QString &id) {
            QJsonArray result;
            const auto add = [&](const QString &fieldPath, const QString &sourceOperation,
                                 const QStringList &contextFields = {}) {
                result.append(valueSource(fieldPath, sourceOperation, contextFields));
            };

            if (id == QStringLiteral("voices.describe"))
                add(QStringLiteral("/singer"), QStringLiteral("voices.list"));
            if (id == QStringLiteral("tracks.insert")) {
                add(QStringLiteral("/track/voice"), QStringLiteral("voices.list"));
                add(QStringLiteral("/track/default_language"), QStringLiteral("voices.describe"),
                    {QStringLiteral("/track/voice")});
            }
            if (id == QStringLiteral("tracks.set_default_language")) {
                add(QStringLiteral("/language"), QStringLiteral("voices.describe"),
                    {QStringLiteral("/document_id"), QStringLiteral("/track_id")});
            }
            if (id == QStringLiteral("clips.insert")) {
                add(QStringLiteral("/clips/*/clip/default_language"),
                    QStringLiteral("voices.describe"),
                    {QStringLiteral("/document_id"), QStringLiteral("/clips/*/track_id")});
                add(QStringLiteral("/clips/*/clip/notes/*/language"),
                    QStringLiteral("voices.describe"),
                    {QStringLiteral("/document_id"),
                     QStringLiteral("/clips/*/track_id")});
            }
            if (id == QStringLiteral("clips.set_default_language") ||
                id == QStringLiteral("notes.insert") ||
                id == QStringLiteral("notes.set_word_properties")) {
                add(id == QStringLiteral("clips.set_default_language")
                        ? QStringLiteral("/language")
                        : id == QStringLiteral("notes.insert")
                              ? QStringLiteral("/notes/*/language")
                              : QStringLiteral("/edits/*/language"),
                    QStringLiteral("voices.describe"),
                    {QStringLiteral("/document_id"), QStringLiteral("/clip_id")});
            }
            if (id.startsWith(QStringLiteral("parameters.")) &&
                id != QStringLiteral("parameters.get") &&
                id != QStringLiteral("parameters.get_capabilities")) {
                add(QStringLiteral("/name"), QStringLiteral("parameters.get_capabilities"),
                    {QStringLiteral("/document_id"), QStringLiteral("/clip_id")});
                if (id != QStringLiteral("parameters.bake")) {
                    add(QStringLiteral("/type"), QStringLiteral("parameters.get_capabilities"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name")});
                }
                if (id == QStringLiteral("parameters.replace")) {
                    add(QStringLiteral("/curves/*/type"),
                        QStringLiteral("parameters.get_capabilities"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/type")});
                    add(QStringLiteral("/curves/*/nodes/*/interpolation"),
                        QStringLiteral("parameters.get_capabilities"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/type")});
                    add(QStringLiteral("/curves/*/values/*"),
                        QStringLiteral("parameters.get_capabilities"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/type")});
                    add(QStringLiteral("/curves/*/nodes/*/value"),
                        QStringLiteral("parameters.get_capabilities"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/type")});
                }
                if (id == QStringLiteral("parameters.draw")) {
                    add(QStringLiteral("/values/*"),
                        QStringLiteral("parameters.get_capabilities"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/type")});
                }
                if (id == QStringLiteral("parameters.insert_anchor") ||
                    id == QStringLiteral("parameters.move_anchor")) {
                    add(QStringLiteral("/value"),
                        QStringLiteral("parameters.get_capabilities"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/type")});
                }
                if (id == QStringLiteral("parameters.insert_anchor") ||
                    id == QStringLiteral("parameters.set_anchor_interpolation")) {
                    add(QStringLiteral("/interpolation"),
                        QStringLiteral("parameters.get_capabilities"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/type")});
                }
            }
            if (id.startsWith(QStringLiteral("speaker_mix.")) &&
                id != QStringLiteral("speaker_mix.clip.use_track")) {
                if (!id.endsWith(QStringLiteral("replace"))) {
                    add(QStringLiteral("/singer"), QStringLiteral("voices.list"));
                    add(QStringLiteral("/speaker"), QStringLiteral("voices.describe"),
                        {QStringLiteral("/singer")});
                }
                if (id.endsWith(QStringLiteral("apply")) ||
                    id.endsWith(QStringLiteral("replace")) ||
                    id.endsWith(QStringLiteral("enable_dynamic"))) {
                    add(QStringLiteral("/mix/singer"), QStringLiteral("voices.list"));
                    add(QStringLiteral("/mix/speakers"), QStringLiteral("voices.describe"),
                        {QStringLiteral("/mix/singer")});
                }
            }
            if (id == QStringLiteral("exports.audio.preview") ||
                id == QStringLiteral("exports.audio.start")) {
                for (const auto &field : {QStringLiteral("format"),
                                          QStringLiteral("sample_rate"),
                                          QStringLiteral("channel_mode"),
                                          QStringLiteral("mixing_mode"),
                                          QStringLiteral("source"),
                                          QStringLiteral("source_ids")}) {
                    add(QStringLiteral("/options/%1").arg(field),
                        QStringLiteral("exports.audio.get_capabilities"),
                        {QStringLiteral("/document_id")});
                }
            }
            if (id == QStringLiteral("extract.pitch.start") ||
                id == QStringLiteral("extract.midi.start")) {
                add(QStringLiteral("/options/model_id"),
                    QStringLiteral("extract.get_capabilities"),
                    {QStringLiteral("/document_id"), QStringLiteral("/clip_id")});
                if (id == QStringLiteral("extract.midi.start")) {
                    add(QStringLiteral("/options/default_language"),
                        QStringLiteral("voices.describe"),
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id")});
                }
            }
            if (id == QStringLiteral("inference.start") ||
                id == QStringLiteral("inference.reset_stage")) {
                add(id == QStringLiteral("inference.start") ? QStringLiteral("/stages/*")
                                                             : QStringLiteral("/stage"),
                    QStringLiteral("inference.get_capabilities"),
                    {QStringLiteral("/document_id"), QStringLiteral("/scope")});
                if (id == QStringLiteral("inference.start")) {
                    for (const auto &field : {QStringLiteral("provider_id"),
                                              QStringLiteral("device_id"),
                                              QStringLiteral("model_id")}) {
                        add(QStringLiteral("/options/%1").arg(field),
                            QStringLiteral("inference.get_capabilities"),
                            {QStringLiteral("/document_id"), QStringLiteral("/scope")});
                    }
                }
            }
            return result;
        }

        QString documentPolicy(const ToolContract &tool) {
            if (tool.operationId == QStringLiteral("documents.new") ||
                tool.operationId == QStringLiteral("documents.open")) {
                return QStringLiteral("replace");
            }
            if (isDocumentQuery(tool.operationId))
                return QStringLiteral("read");
            if (isDocumentCommand(tool.operationId, tool.kind))
                return QStringLiteral("write");
            return QStringLiteral("none");
        }

        QString revisionPolicy(const ToolContract &tool) {
            if (isDocumentCommand(tool.operationId, tool.kind))
                return tool.syncMode == SyncMode::Asynchronous
                           ? QStringLiteral("check_and_revalidate")
                           : QStringLiteral("check");
            return QStringLiteral("none");
        }

        QString historyPolicy(const ToolContract &tool) {
            if (tool.kind == OperationKind::Query ||
                tool.operationId == QStringLiteral("tasks.cancel") ||
                tool.operationId == QStringLiteral("documents.new") ||
                tool.operationId == QStringLiteral("documents.open") ||
                tool.operationId == QStringLiteral("tracks.set_default_language") ||
                tool.operationId == QStringLiteral("clips.set_default_language") ||
                tool.operationId == QStringLiteral("audio_clips.confirm_path") ||
                tool.operationId == QStringLiteral("inference.reset_stage") ||
                tool.operationId == QStringLiteral("exports.midi.start") ||
                tool.operationId == QStringLiteral("exports.audio.start") ||
                tool.operationId == QStringLiteral("playback.play") ||
                tool.operationId == QStringLiteral("playback.pause") ||
                tool.operationId == QStringLiteral("playback.stop") ||
                tool.operationId == QStringLiteral("playback.set_position") ||
                tool.operationId == QStringLiteral("playback.set_last_position")) {
                return QStringLiteral("none");
            }
            if (tool.operationId == QStringLiteral("documents.save"))
                return QStringLiteral("savepoint");
            if (tool.operationId == QStringLiteral("history.undo") ||
                tool.operationId == QStringLiteral("history.redo")) {
                return QStringLiteral("navigate");
            }
            return tool.syncMode == SyncMode::Asynchronous ? QStringLiteral("commit_once")
                                                            : QStringLiteral("single_entry");
        }

        QString fileAccess(const QString &id) {
            if (id == QStringLiteral("documents.open") || id == QStringLiteral("documents.import") ||
                id.startsWith(QStringLiteral("audio_clips.")) ||
                id == QStringLiteral("exports.audio.preview") ||
                id.startsWith(QStringLiteral("extract."))) {
                return QStringLiteral("read");
            }
            if (id == QStringLiteral("documents.save") ||
                id == QStringLiteral("exports.midi.start") ||
                id == QStringLiteral("exports.audio.start")) {
                return QStringLiteral("write");
            }
            return QStringLiteral("none");
        }

        QString concurrencyScope(const ToolContract &tool) {
            if (tool.operationId.startsWith(QStringLiteral("playback.")))
                return QStringLiteral("playback");
            if (tool.operationId.startsWith(QStringLiteral("tasks.")))
                return QStringLiteral("task");
            if (tool.operationId.startsWith(QStringLiteral("exports.")))
                return QStringLiteral("export");
            if (tool.minimumProfile == AutomationProfile::Meta ||
                tool.operationId.startsWith(QStringLiteral("automation.")) ||
                tool.operationId.startsWith(QStringLiteral("voices.")) ||
                tool.operationId == QStringLiteral("formats.list")) {
                return QStringLiteral("application");
            }
            return QStringLiteral("document");
        }

        QString conflictPolicy(const ToolContract &tool) {
            if (tool.kind == OperationKind::Query)
                return QStringLiteral("none");
            if (tool.operationId == QStringLiteral("tasks.cancel"))
                return QStringLiteral("task_state");
            if (tool.operationId == QStringLiteral("exports.midi.start") ||
                tool.operationId == QStringLiteral("exports.audio.start")) {
                return QStringLiteral("snapshot");
            }
            if (tool.operationId == QStringLiteral("documents.new") ||
                tool.operationId == QStringLiteral("documents.open")) {
                return QStringLiteral("replacement");
            }
            if (tool.operationId == QStringLiteral("playback.play") ||
                tool.operationId == QStringLiteral("playback.pause") ||
                tool.operationId == QStringLiteral("playback.stop") ||
                tool.operationId == QStringLiteral("playback.set_position") ||
                tool.operationId == QStringLiteral("playback.set_last_position")) {
                return QStringLiteral("state_version");
            }
            return tool.syncMode == SyncMode::Asynchronous
                       ? QStringLiteral("revision_and_generation")
                       : QStringLiteral("revision");
        }

        QJsonObject toolAnnotations(const QString &id, const OperationKind kind) {
            static const QSet<QString> destructive{
                QStringLiteral("tracks.remove"),
                QStringLiteral("clips.remove"),
                QStringLiteral("notes.remove"),
                QStringLiteral("parameters.erase"),
                QStringLiteral("parameters.remove_anchor"),
                QStringLiteral("tempos.delete"),
                QStringLiteral("time_signatures.delete"),
                QStringLiteral("documents.new"),
                QStringLiteral("documents.open"),
                QStringLiteral("inference.reset_stage"),
            };
            return {
                {QStringLiteral("title"), humanTitle(id)},
                {QStringLiteral("readOnlyHint"), kind == OperationKind::Query},
                {QStringLiteral("destructiveHint"), destructive.contains(id)},
                {QStringLiteral("idempotentHint"), kind == OperationKind::Query},
                {QStringLiteral("openWorldHint"), false},
            };
        }

        QJsonObject manifestContent(const PublicManifest &manifest, const bool includeDigest) {
            QJsonArray operations;
            for (const auto &tool : manifest.operations)
                operations.append(tool.toManifestJson());
            QJsonObject result{
                {QStringLiteral("toolset_version"),
                 static_cast<qint64>(manifest.toolsetVersion)},
                {QStringLiteral("profile"), automationProfileName(manifest.profile)},
                {QStringLiteral("host_mode"), manifest.hostMode},
                {QStringLiteral("operations"), operations},
                {QStringLiteral("extensions"), manifest.extensions},
            };
            if (includeDigest)
                result.insert(QStringLiteral("digest"), manifest.digest);
            if (!manifest.nextCursor.isEmpty())
                result.insert(QStringLiteral("next_cursor"), manifest.nextCursor);
            return result;
        }
    }

    QString operationKindName(const OperationKind kind) {
        return publicStringValueDomainValues(PublicValueDomain::OperationKind)
            .value(static_cast<qsizetype>(kind));
    }

    QString syncModeName(const SyncMode mode) {
        return publicStringValueDomainValues(PublicValueDomain::SyncMode)
            .value(static_cast<qsizetype>(mode));
    }

    QJsonObject ToolContract::toMcpToolJson() const {
        return {
            {QStringLiteral("name"), operationId},
            {QStringLiteral("title"), title},
            {QStringLiteral("description"), description},
            {QStringLiteral("inputSchema"), inputSchema},
            {QStringLiteral("outputSchema"), outputSchema},
            {QStringLiteral("annotations"), annotations},
        };
    }

    QJsonObject ToolContract::toManifestJson() const {
        const auto inputDigest = sha256Digest(inputSchema);
        const auto outputDigest = sha256Digest(outputSchema);
        return {
            {QStringLiteral("operation_id"), operationId},
            {QStringLiteral("title"), title},
            {QStringLiteral("description"), description},
            {QStringLiteral("version"), static_cast<qint64>(PublicToolsetVersion)},
            {QStringLiteral("minimum_compatible_version"),
             static_cast<qint64>(PublicMinimumCompatibleVersion)},
            {QStringLiteral("category"), category},
            {QStringLiteral("kind"), operationKindName(kind)},
            {QStringLiteral("input_schema"), inputSchema},
            {QStringLiteral("output_schema"), outputSchema},
            {QStringLiteral("value_sources"), valueSources},
            {QStringLiteral("minimum_profile"), automationProfileName(minimumProfile)},
            {QStringLiteral("sync_mode"), syncModeName(syncMode)},
            {QStringLiteral("document_policy"), documentPolicy(*this)},
            {QStringLiteral("revision_policy"), revisionPolicy(*this)},
            {QStringLiteral("history_policy"), historyPolicy(*this)},
            {QStringLiteral("file_access"), fileAccess(operationId)},
            {QStringLiteral("host_availability"), QStringLiteral("gui")},
            {QStringLiteral("concurrency_scope"), concurrencyScope(*this)},
            {QStringLiteral("conflict_policy"), conflictPolicy(*this)},
            {QStringLiteral("safety_metadata"),
             QJsonObject{
                 {QStringLiteral("read_only"), kind == OperationKind::Query},
                 {QStringLiteral("destructive"),
                  annotations.value(QStringLiteral("destructiveHint")).toBool()},
                 {QStringLiteral("idempotent"),
                  annotations.value(QStringLiteral("idempotentHint")).toBool()},
                 {QStringLiteral("open_world"),
                  annotations.value(QStringLiteral("openWorldHint")).toBool()},
             }},
            {QStringLiteral("introduced_version"), 1},
            {QStringLiteral("schema_digest"),
             QJsonObject{{QStringLiteral("input"), inputDigest},
                         {QStringLiteral("output"), outputDigest}}},
        };
    }

    const QList<ToolContract> &publicToolContracts() {
        static const QList<ToolContract> tools = [] {
            QList<ToolContract> result;
            result.reserve(87);
#define AUTOMATION_WIRE_PUBLIC_TOOL(tracking, id, categoryValue, profile, kindValue, syncValue)     \
    do {                                                                                            \
        const auto operationId = QStringLiteral(id);                                                \
        const auto operationKind = OperationKind::kindValue;                                        \
        const auto operationSync = SyncMode::syncValue;                                             \
        const auto title = humanTitle(operationId);                                                  \
        result.append({                                                                             \
            .trackingId = QStringLiteral(tracking),                                                  \
            .operationId = operationId,                                                             \
            .title = title,                                                                         \
            .description =                                                                          \
                QStringLiteral("DS Editor Lite public automation operation: %1").arg(operationId), \
            .category = QStringLiteral(categoryValue),                                              \
            .minimumProfile = AutomationProfile::profile,                                           \
            .kind = operationKind,                                                                  \
            .syncMode = operationSync,                                                              \
            .inputSchema = inputSchema(operationId, operationKind),                                 \
            .outputSchema = outputSchema(operationId, operationKind, operationSync),                \
            .valueSources = valueSources(operationId),                                               \
            .annotations = toolAnnotations(operationId, operationKind),                              \
        });                                                                                         \
    } while (false);
#include "PublicToolDefinitions.inc"
#undef AUTOMATION_WIRE_PUBLIC_TOOL
            const auto options = std::find_if(
                result.begin(), result.end(), [](const auto &tool) {
                    return tool.operationId == QStringLiteral("automation.get_options");
                });
            Q_ASSERT(options != result.end());
            options->inputSchema = getOptionsInputSchema(result);
            return result;
        }();
        return tools;
    }

    const ToolContract *findPublicTool(const QString &operationId) {
        const auto &tools = publicToolContracts();
        const auto it = std::find_if(tools.constBegin(), tools.constEnd(), [&](const auto &tool) {
            return tool.operationId == operationId;
        });
        return it == tools.constEnd() ? nullptr : &*it;
    }

    QStringList publicToolIds() {
        QStringList result;
        result.reserve(publicToolContracts().size());
        for (const auto &tool : publicToolContracts())
            result.append(tool.operationId);
        return result;
    }

    QList<ToolContract> toolsForProfile(const AutomationProfile profile,
                                       const QSet<QString> &customEnabled) {
        QList<ToolContract> result;
        for (const auto &tool : publicToolContracts()) {
            const auto enabled = tool.minimumProfile == AutomationProfile::Meta ||
                                 (profile == AutomationProfile::Custom
                                      ? customEnabled.contains(tool.operationId)
                                      : presetIncludes(profile, tool.minimumProfile));
            if (enabled)
                result.append(tool);
        }
        return result;
    }

    QJsonObject PublicManifest::toJson() const {
        return manifestContent(*this, true);
    }

    PublicManifest buildPublicManifest(const AutomationProfile profile,
                                       const QSet<QString> &customEnabled, const QString &hostMode,
                                       qsizetype offset, const qsizetype limit) {
        const auto allOperations = toolsForProfile(profile, customEnabled);
        offset = std::clamp<qsizetype>(offset, 0, allOperations.size());
        const auto count = limit <= 0 ? allOperations.size() - offset
                                      : std::min(limit, allOperations.size() - offset);

        PublicManifest manifest{
            .profile = profile,
            .hostMode = hostMode,
            .operations = allOperations.mid(offset, count),
        };
        if (offset + count < allOperations.size())
            manifest.nextCursor = QString::number(offset + count);

        auto digestManifest = manifest;
        digestManifest.operations = allOperations;
        digestManifest.nextCursor.clear();
        manifest.digest = sha256Digest(manifestContent(digestManifest, false));
        return manifest;
    }

}
