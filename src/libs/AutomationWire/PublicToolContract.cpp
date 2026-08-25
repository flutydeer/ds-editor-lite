#include "PublicToolContract.h"

#include "CanonicalJson.h"
#include "JsonSchema.h"
#include "PublicConstants.h"
#include "PublicEnums.h"
#include "PublicToolNames.h"
#include "PublicValueDomains.h"

#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <functional>
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

        QJsonObject singerRefSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("package_id"), nonEmptyStringSchema()},
                    {QStringLiteral("singer_id"),  nonEmptyStringSchema()},
            },
                {QStringLiteral("package_id"), QStringLiteral("singer_id")});
        }

        QJsonObject speakerRefSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("speaker_id"), nonEmptyStringSchema()}
            },
                {QStringLiteral("speaker_id")});
        }

        QJsonObject voiceSelectionSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("singer"),  singerRefSchema()                         },
                    {QStringLiteral("speaker"),
                     JsonSchema::oneOf(QJsonArray{speakerRefSchema(), JsonSchema::null()})},
            },
                {QStringLiteral("singer")});
        }

        QJsonObject languageSelectionSchema() {
            const auto simple = [](const QString &mode) {
                return JsonSchema::object(
                    {
                        {QStringLiteral("mode"), JsonSchema::constant(mode)}
                },
                    {QStringLiteral("mode")});
            };
            return JsonSchema::oneOf(QJsonArray{
                simple(QStringLiteral("follow_singer")),
                simple(QStringLiteral("unknown")),
                JsonSchema::object(
                    {
                              {QStringLiteral("mode"), JsonSchema::constant(QStringLiteral("explicit"))},
                              {QStringLiteral("language_id"), nonEmptyStringSchema()},
                              },
                    {QStringLiteral("mode"), QStringLiteral("language_id")}
                    ),
            });
        }

        QJsonObject speakerMixTargetSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("type"),
                     JsonSchema::string({QStringLiteral("track"), QStringLiteral("clip")})},
                    {QStringLiteral("id"),   identifierSchema()                           },
            },
                {QStringLiteral("type"), QStringLiteral("id")});
        }

        QJsonObject speakerMixDraftSchema() {
            const auto source = JsonSchema::object(
                {
                    {QStringLiteral("speaker"), speakerRefSchema()},
                    {QStringLiteral("weight"),
                     JsonSchema::number(MinimumMixWeight, MaximumMixWeight)},
            },
                {QStringLiteral("speaker"), QStringLiteral("weight")});
            return JsonSchema::object(
                {
                    {QStringLiteral("singer"), singerRefSchema()},
                    {QStringLiteral("sources"),
                     JsonSchema::array(source, 1, MaximumCommandCollectionItems)},
            },
                {QStringLiteral("singer"), QStringLiteral("sources")});
        }

        QJsonObject voiceRefSchema() {
            return voiceSelectionSchema();
        }

        QJsonObject documentVersionSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("document_id"), uuidSchema()    },
                    {QStringLiteral("revision"),    revisionSchema()},
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
            return JsonSchema::object(
                {
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                    {QStringLiteral("solo"), JsonSchema::boolean()},
            },
                {QStringLiteral("gain"), QStringLiteral("pan"), QStringLiteral("mute"),
                 QStringLiteral("solo")});
        }

        QJsonObject clipPropertiesSchema(const bool requireId) {
            auto result = JsonSchema::object(
                {
                    {QStringLiteral("clip_id"),     identifierSchema()      },
                    {QStringLiteral("name"),        JsonSchema::string()    },
                    {QStringLiteral("start"),       JsonSchema::integer(0.0)},
                    {QStringLiteral("length"),      JsonSchema::integer(1.0)},
                    {QStringLiteral("clip_start"),  JsonSchema::integer(0.0)},
                    {QStringLiteral("clip_length"), JsonSchema::integer(1.0)},
                    {QStringLiteral("gain"),        JsonSchema::number()    },
                    {QStringLiteral("mute"),        JsonSchema::boolean()   },
            },
                requireId ? QStringList{QStringLiteral("clip_id")} : QStringList{});
            result.insert(QStringLiteral("minProperties"), requireId ? 2 : 1);
            return result;
        }

        QJsonObject noteDraftSchema() {
            const auto phoneme = JsonSchema::object(
                {
                    {QStringLiteral("symbol"),   nonEmptyStringSchema()},
                    {QStringLiteral("language"), JsonSchema::string()  },
                    {QStringLiteral("offset"),   JsonSchema::integer() },
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
                    {QStringLiteral("language"), languageSelectionSchema()},
                    {QStringLiteral("pronunciation"), JsonSchema::string()},
                    {QStringLiteral("pronunciation_candidates"),
                     JsonSchema::array(JsonSchema::string(), {}, MaximumCommandCollectionItems)},
                    {QStringLiteral("phonemes"),
                     JsonSchema::array(phoneme, {}, MaximumCommandCollectionItems)},
                    {QStringLiteral("line_feed"), JsonSchema::boolean()},
            },
                {QStringLiteral("local_start"), QStringLiteral("length"),
                 QStringLiteral("key_index")});
        }

        QJsonObject noteWordEditSchema() {
            const auto phoneme = JsonSchema::object(
                {
                    {QStringLiteral("symbol"),   nonEmptyStringSchema()},
                    {QStringLiteral("language"), JsonSchema::string()  },
                    {QStringLiteral("offset"),   JsonSchema::integer() },
            },
                {QStringLiteral("symbol")});
            auto result = JsonSchema::object(
                {
                    {QStringLiteral("note_id"), identifierSchema()},
                    {QStringLiteral("lyric"), JsonSchema::string()},
                    {QStringLiteral("language"), nonEmptyStringSchema()},
                    {QStringLiteral("pronunciation"), JsonSchema::string()},
                    {QStringLiteral("pronunciation_candidates"),
                     JsonSchema::array(JsonSchema::string(), {}, MaximumCommandCollectionItems)},
                    {QStringLiteral("phonemes"),
                     JsonSchema::array(phoneme, {}, MaximumCommandCollectionItems)},
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
                    {QStringLiteral("values"),
                     JsonSchema::array(JsonSchema::integer(), 1, MaximumCurveSampleItems)},
            },
                {QStringLiteral("type"), QStringLiteral("local_start"), QStringLiteral("step"),
                 QStringLiteral("values")});
            const auto anchorNode = JsonSchema::object(
                {
                    {QStringLiteral("position"),      JsonSchema::integer()},
                    {QStringLiteral("value"),         JsonSchema::integer()},
                    {QStringLiteral("interpolation"), interpolationSchema()},
            },
                {QStringLiteral("position"), QStringLiteral("value"),
                 QStringLiteral("interpolation")});
            const auto anchor = JsonSchema::object(
                {
                    {QStringLiteral("type"),
                     domainConstant(PublicValueDomain::CurveType, QStringLiteral("anchor"))},
                    {QStringLiteral("nodes"),
                     JsonSchema::array(anchorNode, 1, MaximumCommandCollectionItems)},
            },
                {QStringLiteral("type"), QStringLiteral("nodes")});
            return JsonSchema::oneOf(QJsonArray{draw, anchor});
        }

        QJsonObject speakerMixSchema() {
            return speakerMixDraftSchema();
        }

        QJsonObject clipDraftSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("client_ref"), JsonSchema::string()    },
                    {QStringLiteral("track_id"),   identifierSchema()      },
                    {QStringLiteral("start"),      JsonSchema::integer(0.0)},
                    {QStringLiteral("length"),     JsonSchema::integer(1.0)},
                    {QStringLiteral("name"),       JsonSchema::string()    },
            },
                {QStringLiteral("track_id"), QStringLiteral("start")});
        }

        QJsonObject trackDraftSchema() {
            return JsonSchema::object({
                {QStringLiteral("client_ref"), JsonSchema::string()},
                {QStringLiteral("name"), JsonSchema::string()},
                {QStringLiteral("color_index"),
                 JsonSchema::integer(0.0, TrackPaletteColorCount - 1)},
            });
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
            const auto commonProperties = QJsonObject{
                {QStringLiteral("format"), nonEmptyStringSchema()},
                {QStringLiteral("sample_rate"),
                 JsonSchema::integer(MinimumAudioSampleRate, MaximumAudioSampleRate)},
                {QStringLiteral("channel_mode"),
                 stringDomainSchema(PublicValueDomain::ChannelMode)},
                {QStringLiteral("mixing_mode"),
                 stringDomainSchema(PublicValueDomain::AudioMixingMode)},
                {QStringLiteral("mute_solo_enabled"), JsonSchema::boolean()},
            };
            const auto commonRequired = QStringList{
                QStringLiteral("format"),       QStringLiteral("sample_rate"),
                QStringLiteral("channel_mode"), QStringLiteral("mixing_mode"),
                QStringLiteral("source"),
            };

            auto allProperties = commonProperties;
            allProperties.insert(
                QStringLiteral("source"),
                domainConstant(PublicValueDomain::AudioSourceMode, QStringLiteral("all")));

            auto customProperties = commonProperties;
            customProperties.insert(
                QStringLiteral("source"),
                domainConstant(PublicValueDomain::AudioSourceMode, QStringLiteral("custom")));
            customProperties.insert(
                QStringLiteral("source_ids"),
                JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems));
            auto customRequired = commonRequired;
            customRequired.append(QStringLiteral("source_ids"));

            return JsonSchema::oneOf(QJsonArray{
                JsonSchema::object(allProperties, commonRequired),
                JsonSchema::object(customProperties, customRequired),
            });
        }

        QJsonObject inferenceScopeSchema() {
            const auto document = JsonSchema::object(
                {
                    {QStringLiteral("kind"), domainConstant(PublicValueDomain::InferenceScopeKind,
                     QStringLiteral("document"))}
            },
                {QStringLiteral("kind")});
            const auto track = JsonSchema::object(
                {
                    {QStringLiteral("kind"), domainConstant(PublicValueDomain::InferenceScopeKind,
                     QStringLiteral("track"))},
                    {QStringLiteral("track_ids"),
                     JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems)},
            },
                {QStringLiteral("kind"), QStringLiteral("track_ids")});
            const auto clip = JsonSchema::object(
                {
                    {QStringLiteral("kind"),
                     domainConstant(PublicValueDomain::InferenceScopeKind, QStringLiteral("clip"))},
                    {QStringLiteral("clip_ids"),
                     JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems)},
            },
                {QStringLiteral("kind"), QStringLiteral("clip_ids")});
            return JsonSchema::oneOf(QJsonArray{document, track, clip});
        }

        QJsonObject extractionOptionsSchema(const QString &id) {
            if (id == PublicToolNames::extract_pitch_start) {
                return JsonSchema::object({
                    {QStringLiteral("model_id"), nonEmptyStringSchema()},
                });
            }
            return JsonSchema::object({
                {QStringLiteral("model_id"), nonEmptyStringSchema()},
                {QStringLiteral("default_language"), nonEmptyStringSchema()},
                {QStringLiteral("default_lyric"), JsonSchema::string()},
                {QStringLiteral("minimum_note_length"),
                 JsonSchema::integer(1.0, std::numeric_limits<int>::max())},
                {QStringLiteral("client_ref"), JsonSchema::string()},
            });
        }

        QJsonObject audioImportItemSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("track_id"),   identifierSchema()         },
                    {QStringLiteral("path"),       nonEmptyStringSchema()     },
                    {QStringLiteral("properties"), clipPropertiesSchema(false)},
                    {QStringLiteral("client_ref"), JsonSchema::string()       },
            },
                {QStringLiteral("track_id"), QStringLiteral("path")});
        }

        QJsonObject formatOptionsSchema() {
            return JsonSchema::object({
                {QStringLiteral("encoding"),               nonEmptyStringSchema()},
                {QStringLiteral("import_tempo"),           JsonSchema::boolean() },
                {QStringLiteral("import_time_signatures"), JsonSchema::boolean() },
            });
        }

        QJsonObject midiExportOptionsSchema() {
            return JsonSchema::object({
                {QStringLiteral("include_tempo"), JsonSchema::boolean()},
                {QStringLiteral("include_time_signatures"), JsonSchema::boolean()},
                {QStringLiteral("include_lyrics"), JsonSchema::boolean()},
                {QStringLiteral("track_ids"),
                 JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems)},
                {QStringLiteral("clip_ids"),
                 JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems)},
            });
        }

        QJsonObject fillLyricsOptionsSchema() {
            return JsonSchema::object({
                {QStringLiteral("splitter_id"),
                 JsonSchema::string({QStringLiteral("auto"), QStringLiteral("character")})  },
                {QStringLiteral("tagger_id"),   JsonSchema::string({QStringLiteral("auto")})},
                {QStringLiteral("skip_slur"),   JsonSchema::boolean()                       },
                {QStringLiteral("language"),    languageSelectionSchema()                   },
            });
        }

        QJsonObject extractionDestinationSchema() {
            const auto createClip = JsonSchema::object(
                {
                    {QStringLiteral("target_track_id"), identifierSchema()                                 },
                    {QStringLiteral("start"),           JsonSchema::integer(0.0)                           },
                    {QStringLiteral("mode"),            JsonSchema::constant(QStringLiteral("create_clip"))},
            },
                {QStringLiteral("target_track_id"), QStringLiteral("start"),
                 QStringLiteral("mode")});
            const auto mergeIntoClip = JsonSchema::object(
                {
                    {QStringLiteral("target_track_id"), identifierSchema()      },
                    {QStringLiteral("start"),           JsonSchema::integer(0.0)},
                    {QStringLiteral("mode"),
                     JsonSchema::constant(QStringLiteral("merge_into_clip"))    },
                    {QStringLiteral("target_clip_id"),  identifierSchema()      },
            },
                {QStringLiteral("target_track_id"), QStringLiteral("start"), QStringLiteral("mode"),
                 QStringLiteral("target_clip_id")});
            return JsonSchema::oneOf(QJsonArray{createClip, mergeIntoClip});
        }

        const QSet<QString> &persistentPlaybackOperations() {
            static const QSet<QString> ids{
                PublicToolNames::playback_set_loop,
                PublicToolNames::playback_set_loop_enabled,
                PublicToolNames::playback_clear_loop,
            };
            return ids;
        }

        bool isPersistentPlaybackOperation(const QString &id) {
            return persistentPlaybackOperations().contains(id);
        }

        QJsonObject inputFieldSchema(const QString &id, const QString &name) {
            if (name == QStringLiteral("document_id") ||
                name == QStringLiteral("current_document_id") ||
                name == QStringLiteral("task_id")) {
                return uuidSchema();
            }
            if (name == QStringLiteral("expected_revision") ||
                name == QStringLiteral("expected_state_version")) {
                return revisionSchema();
            }
            if (name == QStringLiteral("track_id") || name == QStringLiteral("clip_id") ||
                name == QStringLiteral("note_id") || name == QStringLiteral("curve_id") ||
                name == QStringLiteral("anchor_id") || name == QStringLiteral("source_clip_id") ||
                name == QStringLiteral("target_clip_id") ||
                name == QStringLiteral("source_audio_clip_id") ||
                name == QStringLiteral("target_singing_clip_id") ||
                name == QStringLiteral("keyframe_id")) {
                return identifierSchema();
            }
            if (name == QStringLiteral("track_ids") || name == QStringLiteral("clip_ids") ||
                name == QStringLiteral("note_ids") || name == QStringLiteral("anchor_ids") ||
                name == QStringLiteral("keyframe_ids")) {
                return JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("index") || name == QStringLiteral("target_index") ||
                name == QStringLiteral("bar_index")) {
                return JsonSchema::integer(0.0, std::numeric_limits<int>::max());
            }
            if (name == QStringLiteral("limit"))
                return JsonSchema::integer(MinimumPageSize, MaximumPageSize);
            if (name == QStringLiteral("tick") || name == QStringLiteral("delta_tick") ||
                name == QStringLiteral("delta_key") || name == QStringLiteral("local_start") ||
                name == QStringLiteral("local_end") || name == QStringLiteral("local_position") ||
                name == QStringLiteral("value")) {
                return JsonSchema::integer(std::numeric_limits<int>::min(),
                                           std::numeric_limits<int>::max());
            }
            if (name == QStringLiteral("start") || name == QStringLiteral("end") ||
                name == QStringLiteral("position") || name == QStringLiteral("target_start")) {
                if (id == PublicToolNames::playback_set_loop &&
                    (name == QStringLiteral("start") || name == QStringLiteral("end"))) {
                    return JsonSchema::integer(0.0, std::numeric_limits<int>::max());
                }
                return id.startsWith(QStringLiteral("playback."))
                           ? JsonSchema::number(0.0)
                           : JsonSchema::integer(0.0, std::numeric_limits<int>::max());
            }
            if (name == QStringLiteral("step") || name == QStringLiteral("numerator") ||
                name == QStringLiteral("denominator")) {
                return JsonSchema::integer(1.0, std::numeric_limits<int>::max());
            }
            if (name == QStringLiteral("tempo"))
                return JsonSchema::number(1.0);
            if (name == QStringLiteral("gain"))
                return JsonSchema::number();
            if (name == QStringLiteral("pan"))
                return JsonSchema::number(MinimumPan, MaximumPan);
            if (name == QStringLiteral("color_index"))
                return JsonSchema::integer(0.0, TrackPaletteColorCount - 1);
            if (name == QStringLiteral("mute") || name == QStringLiteral("solo") ||
                name == QStringLiteral("bypassed") || name == QStringLiteral("case_sensitive") ||
                name == QStringLiteral("regex") || name == QStringLiteral("quantize_start") ||
                name == QStringLiteral("quantize_length") || name == QStringLiteral("enabled") ||
                name == QStringLiteral("validate_only")) {
                return JsonSchema::boolean();
            }
            if (name == QStringLiteral("name") && id.startsWith(QStringLiteral("parameters.")))
                return parameterNameSchema();
            if (name == QStringLiteral("operation_id") || name == QStringLiteral("field_path") ||
                name == QStringLiteral("idempotency_key") || name == QStringLiteral("cursor") ||
                name == QStringLiteral("query") || name == QStringLiteral("package_id") ||
                name == QStringLiteral("path") || name == QStringLiteral("name") ||
                name == QStringLiteral("language_id") || name == QStringLiteral("format_id") ||
                name == QStringLiteral("plan_digest") || name == QStringLiteral("text") ||
                name == QStringLiteral("lyric") || name == QStringLiteral("pronunciation")) {
                return name == QStringLiteral("name") || name == QStringLiteral("lyric") ||
                               name == QStringLiteral("pronunciation")
                           ? JsonSchema::string()
                           : nonEmptyStringSchema();
            }
            if (name == QStringLiteral("partial_arguments"))
                return JsonSchema::objectWithAdditionalSchema({}, {}, QJsonValue(true));
            if (name == QStringLiteral("unsaved_policy"))
                return stringDomainSchema(PublicValueDomain::UnsavedPolicy);
            if (name == QStringLiteral("template"))
                return stringDomainSchema(PublicValueDomain::DocumentTemplate);
            if (name == QStringLiteral("failure_policy"))
                return stringDomainSchema(PublicValueDomain::FailurePolicy);
            if (name == QStringLiteral("overwrite_policy"))
                return JsonSchema::string({QStringLiteral("reject"), QStringLiteral("overwrite")});
            if (name == QStringLiteral("extension_policy")) {
                return JsonSchema::string({QStringLiteral("preserve"),
                                           QStringLiteral("append_if_missing"),
                                           QStringLiteral("replace")});
            }
            if (name == QStringLiteral("purpose")) {
                return id == PublicToolNames::formats_inspect
                           ? JsonSchema::string({QStringLiteral("open"), QStringLiteral("import")})
                           : JsonSchema::string({QStringLiteral("open"), QStringLiteral("import"),
                                                 QStringLiteral("export")});
            }
            if (name == QStringLiteral("mode") && id == PublicToolNames::notes_search) {
                return JsonSchema::string({QStringLiteral("starts_with"), QStringLiteral("exact"),
                                           QStringLiteral("contains")});
            }
            if (name == QStringLiteral("state"))
                return stringDomainSchema(PublicValueDomain::TaskState);
            if (name == QStringLiteral("kind"))
                return stringDomainSchema(PublicValueDomain::TaskKind);
            if (name == QStringLiteral("type"))
                return stringDomainSchema(PublicValueDomain::ClipType);
            if (name == QStringLiteral("quantize"))
                return valueDomainSchema(PublicValueDomain::Quantize);
            if (name == QStringLiteral("merge_mode"))
                return stringDomainSchema(PublicValueDomain::CurveMergeMode);
            if (name == QStringLiteral("layer"))
                return parameterLayerSchema();
            if (name == QStringLiteral("interpolation"))
                return interpolationSchema();
            if (name == QStringLiteral("source"))
                return stringDomainSchema(PublicValueDomain::PronunciationSource);
            if (name == QStringLiteral("stage"))
                return stringDomainSchema(PublicValueDomain::InferenceStage);
            if (name == QStringLiteral("stages")) {
                return JsonSchema::array(
                    stringDomainSchema(PublicValueDomain::InferenceStage), 1,
                    publicValueDomainValues(PublicValueDomain::InferenceStage).size());
            }
            if (name == QStringLiteral("singer"))
                return singerRefSchema();
            if (name == QStringLiteral("voice"))
                return voiceSelectionSchema();
            if (name == QStringLiteral("language"))
                return languageSelectionSchema();
            if (name == QStringLiteral("target"))
                return speakerMixTargetSchema();
            if (name == QStringLiteral("mix"))
                return speakerMixDraftSchema();
            if (name == QStringLiteral("tracks")) {
                return JsonSchema::array(trackDraftSchema(), 1, MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("clips")) {
                return JsonSchema::array(clipDraftSchema(), 1, MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("notes")) {
                return JsonSchema::array(noteDraftSchema(), 1, MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("curves")) {
                return JsonSchema::array(curveDraftSchema(), 1, MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("scope"))
                return inferenceScopeSchema();
            if (name == QStringLiteral("values") || name == QStringLiteral("offsets")) {
                return JsonSchema::array(JsonSchema::integer(), {}, MaximumCurveSampleItems);
            }
            if (name == QStringLiteral("names")) {
                return JsonSchema::array(nonEmptyStringSchema(), 1, MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("weights")) {
                return JsonSchema::array(JsonSchema::number(MinimumMixWeight, MaximumMixWeight), 1,
                                         MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("range")) {
                return JsonSchema::object(
                    {
                        {QStringLiteral("start"), JsonSchema::integer(0.0)},
                        {QStringLiteral("end"),   JsonSchema::integer(0.0)},
                },
                    {QStringLiteral("start"), QStringLiteral("end")});
            }
            if (name == QStringLiteral("destination")) {
                if (id == PublicToolNames::extract_midi_start)
                    return extractionDestinationSchema();
                return JsonSchema::object(
                    {
                        {QStringLiteral("target_track_id"), identifierSchema()      },
                        {QStringLiteral("start"),           JsonSchema::integer(0.0)},
                },
                    {QStringLiteral("target_track_id"), QStringLiteral("start")});
            }
            if (name == QStringLiteral("moves")) {
                QJsonObject item;
                QStringList required;
                if (id == PublicToolNames::clips_move) {
                    item = JsonSchema::object(
                        {
                            {QStringLiteral("clip_id"),         identifierSchema()      },
                            {QStringLiteral("target_track_id"), identifierSchema()      },
                            {QStringLiteral("start"),           JsonSchema::integer(0.0)},
                    },
                        {QStringLiteral("clip_id"), QStringLiteral("target_track_id"),
                         QStringLiteral("start")});
                } else if (id == PublicToolNames::speaker_mix_keyframes_move) {
                    item = JsonSchema::object(
                        {
                            {QStringLiteral("keyframe_id"), identifierSchema()      },
                            {QStringLiteral("position"),    JsonSchema::integer(0.0)},
                    },
                        {QStringLiteral("keyframe_id"), QStringLiteral("position")});
                } else {
                    item = JsonSchema::object(
                        {
                            {QStringLiteral("anchor_id"), identifierSchema()   },
                            {QStringLiteral("position"),  JsonSchema::integer()},
                            {QStringLiteral("value"),     JsonSchema::integer()},
                    },
                        {QStringLiteral("anchor_id"), QStringLiteral("position"),
                         QStringLiteral("value")});
                }
                return JsonSchema::array(item, 1, MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("anchors")) {
                const auto anchor = JsonSchema::object(
                    {
                        {QStringLiteral("position"),      JsonSchema::integer()},
                        {QStringLiteral("value"),         JsonSchema::integer()},
                        {QStringLiteral("interpolation"), interpolationSchema()},
                },
                    {QStringLiteral("position"), QStringLiteral("value")});
                return JsonSchema::array(anchor, 1, MaximumCommandCollectionItems);
            }
            if (name == QStringLiteral("items")) {
                if (id == PublicToolNames::audio_clips_import_batch) {
                    const auto item = JsonSchema::object(
                        {
                            {QStringLiteral("track_id"), identifierSchema()      },
                            {QStringLiteral("start"),    JsonSchema::integer(0.0)},
                            {QStringLiteral("path"),     nonEmptyStringSchema()  },
                            {QStringLiteral("name"),     JsonSchema::string()    },
                            {QStringLiteral("gain"),     JsonSchema::number()    },
                            {QStringLiteral("mute"),     JsonSchema::boolean()   },
                    },
                        {QStringLiteral("track_id"), QStringLiteral("start"),
                         QStringLiteral("path")});
                    return JsonSchema::array(item, 1, MaximumAudioImportBatchItems);
                }
                const auto item = JsonSchema::object(
                    {
                        {QStringLiteral("path"),        nonEmptyStringSchema()},
                        {QStringLiteral("format_id"),   nonEmptyStringSchema()},
                        {QStringLiteral("options"),     formatOptionsSchema() },
                        {QStringLiteral("plan_digest"), nonEmptyStringSchema()},
                },
                    {QStringLiteral("path")});
                return JsonSchema::array(item, 1, MaximumAudioImportBatchItems);
            }
            if (name == QStringLiteral("options")) {
                if (id == PublicToolNames::documents_open ||
                    id == PublicToolNames::documents_import) {
                    return formatOptionsSchema();
                }
                if (id.startsWith(QStringLiteral("exports.audio.")))
                    return audioExportOptionsSchema();
                if (id.startsWith(QStringLiteral("exports.midi.")))
                    return midiExportOptionsSchema();
                if (id.startsWith(QStringLiteral("extract.")))
                    return extractionOptionsSchema(id);
                if (id == PublicToolNames::inference_start) {
                    return JsonSchema::object({
                        {QStringLiteral("provider_id"), nonEmptyStringSchema()},
                        {QStringLiteral("device_id"),   nonEmptyStringSchema()},
                        {QStringLiteral("model_id"),    nonEmptyStringSchema()},
                    });
                }
                if (id == PublicToolNames::notes_fill_lyrics)
                    return fillLyricsOptionsSchema();
            }
            qFatal("No authoritative public field schema for operation '%s', field '%s'",
                   qPrintable(id), qPrintable(name));
            return {};
        }

        const QSet<QString> &documentQueryOperations() {
            static const QSet<QString> ids{
                PublicToolNames::documents_get,
                PublicToolNames::project_get,
                PublicToolNames::tracks_list,
                PublicToolNames::tracks_get,
                PublicToolNames::tracks_get_voice_context,
                PublicToolNames::master_get,
                PublicToolNames::clips_list,
                PublicToolNames::clips_get,
                PublicToolNames::clips_get_voice_context,
                PublicToolNames::audio_clips_get,
                PublicToolNames::speaker_mix_get,
                PublicToolNames::notes_get,
                PublicToolNames::notes_search,
                PublicToolNames::parameters_get,
                PublicToolNames::parameters_get_capabilities,
                PublicToolNames::timeline_get,
                PublicToolNames::history_get_state,
                PublicToolNames::playback_get,
                PublicToolNames::exports_midi_get_capabilities,
                PublicToolNames::exports_midi_preview,
                PublicToolNames::exports_midi_start,
                PublicToolNames::exports_audio_get_capabilities,
                PublicToolNames::exports_audio_preview,
                PublicToolNames::exports_audio_start,
                PublicToolNames::extract_get_capabilities,
                PublicToolNames::inference_get_capabilities,
                PublicToolNames::inference_get_status,
                PublicToolNames::tasks_list,
                PublicToolNames::tasks_get,
                PublicToolNames::tasks_cancel,
            };
            return ids;
        }

        const QSet<QString> &documentWriteOperations() {
            static const QSet<QString> ids{
                PublicToolNames::documents_save,
                PublicToolNames::documents_save_as,
                PublicToolNames::documents_import,
                PublicToolNames::documents_import_batch,
                PublicToolNames::tracks_insert,
                PublicToolNames::tracks_remove,
                PublicToolNames::tracks_move,
                PublicToolNames::tracks_rename,
                PublicToolNames::tracks_set_color,
                PublicToolNames::tracks_set_gain,
                PublicToolNames::tracks_set_pan,
                PublicToolNames::tracks_set_mute,
                PublicToolNames::tracks_set_solo,
                PublicToolNames::tracks_set_default_language,
                PublicToolNames::tracks_set_voice,
                PublicToolNames::tracks_clear_voice,
                PublicToolNames::master_set_gain,
                PublicToolNames::master_set_pan,
                PublicToolNames::master_set_mute,
                PublicToolNames::master_set_solo,
                PublicToolNames::clips_insert,
                PublicToolNames::clips_duplicate,
                PublicToolNames::clips_remove,
                PublicToolNames::clips_move,
                PublicToolNames::clips_resize_left,
                PublicToolNames::clips_resize_right,
                PublicToolNames::clips_rename,
                PublicToolNames::clips_set_gain,
                PublicToolNames::clips_set_mute,
                PublicToolNames::clips_set_default_language,
                PublicToolNames::clips_use_track_voice,
                PublicToolNames::clips_set_voice,
                PublicToolNames::clips_clear_voice,
                PublicToolNames::audio_clips_import,
                PublicToolNames::audio_clips_import_batch,
                PublicToolNames::audio_clips_relocate,
                PublicToolNames::audio_clips_confirm_path,
                PublicToolNames::speaker_mix_set_fixed,
                PublicToolNames::speaker_mix_enable_dynamic,
                PublicToolNames::speaker_mix_disable_dynamic,
                PublicToolNames::speaker_mix_set_dynamic_bypass,
                PublicToolNames::speaker_mix_keyframes_insert,
                PublicToolNames::speaker_mix_keyframes_move,
                PublicToolNames::speaker_mix_keyframes_set_weights,
                PublicToolNames::speaker_mix_keyframes_remove,
                PublicToolNames::notes_insert,
                PublicToolNames::notes_duplicate,
                PublicToolNames::notes_remove,
                PublicToolNames::notes_move,
                PublicToolNames::notes_resize_left,
                PublicToolNames::notes_resize_right,
                PublicToolNames::notes_split_at,
                PublicToolNames::notes_quantize,
                PublicToolNames::notes_set_lyric,
                PublicToolNames::notes_set_language,
                PublicToolNames::notes_set_pronunciation,
                PublicToolNames::notes_reset_pronunciation,
                PublicToolNames::notes_set_phonemes,
                PublicToolNames::notes_set_phoneme_offsets,
                PublicToolNames::notes_reset_phoneme_offsets,
                PublicToolNames::notes_reset_phonemes,
                PublicToolNames::notes_fill_lyrics,
                PublicToolNames::parameters_replace,
                PublicToolNames::parameters_draw,
                PublicToolNames::parameters_erase,
                PublicToolNames::parameters_bake,
                PublicToolNames::parameters_insert_anchors,
                PublicToolNames::parameters_move_anchors,
                PublicToolNames::parameters_remove_anchors,
                PublicToolNames::parameters_set_anchor_interpolation,
                PublicToolNames::tempos_set,
                PublicToolNames::tempos_delete,
                PublicToolNames::time_signatures_set,
                PublicToolNames::time_signatures_delete,
                PublicToolNames::history_undo,
                PublicToolNames::history_redo,
                PublicToolNames::playback_set_loop,
                PublicToolNames::playback_set_loop_enabled,
                PublicToolNames::playback_clear_loop,
                PublicToolNames::extract_pitch_start,
                PublicToolNames::extract_midi_start,
                PublicToolNames::inference_start,
                PublicToolNames::inference_reset_stage,
            };
            return ids;
        }

        QHash<QString, QStringList> requiredInputFields() {
            return {
                {PublicToolNames::automation_get_options,
                 {QStringLiteral("operation_id"), QStringLiteral("field_path")}                                         },
                {PublicToolNames::documents_new,                       {QStringLiteral("unsaved_policy")}               },
                {PublicToolNames::documents_open,
                 {QStringLiteral("unsaved_policy"), QStringLiteral("path")}                                             },
                {PublicToolNames::documents_save_as,
                 {QStringLiteral("path"), QStringLiteral("overwrite_policy")}                                           },
                {PublicToolNames::documents_import,
                 {QStringLiteral("path"), QStringLiteral("options")}                                                    },
                {PublicToolNames::documents_import_batch,
                 {QStringLiteral("items"), QStringLiteral("failure_policy")}                                            },
                {PublicToolNames::formats_inspect,
                 {QStringLiteral("path"), QStringLiteral("purpose")}                                                    },
                {PublicToolNames::tracks_get,                          {QStringLiteral("track_id")}                     },
                {PublicToolNames::tracks_insert,
                 {QStringLiteral("index"), QStringLiteral("tracks")}                                                    },
                {PublicToolNames::tracks_remove,                       {QStringLiteral("track_ids")}                    },
                {PublicToolNames::tracks_move,
                 {QStringLiteral("track_ids"), QStringLiteral("target_index")}                                          },
                {PublicToolNames::tracks_rename,
                 {QStringLiteral("track_id"), QStringLiteral("name")}                                                   },
                {PublicToolNames::tracks_set_color,
                 {QStringLiteral("track_id"), QStringLiteral("color_index")}                                            },
                {PublicToolNames::tracks_set_gain,
                 {QStringLiteral("track_id"), QStringLiteral("gain")}                                                   },
                {PublicToolNames::tracks_set_pan,
                 {QStringLiteral("track_id"), QStringLiteral("pan")}                                                    },
                {PublicToolNames::tracks_set_mute,
                 {QStringLiteral("track_id"), QStringLiteral("mute")}                                                   },
                {PublicToolNames::tracks_set_solo,
                 {QStringLiteral("track_id"), QStringLiteral("solo")}                                                   },
                {PublicToolNames::tracks_set_default_language,
                 {QStringLiteral("track_id"), QStringLiteral("language_id")}                                            },
                {PublicToolNames::tracks_get_voice_context,            {QStringLiteral("track_id")}                     },
                {PublicToolNames::tracks_set_voice,
                 {QStringLiteral("track_id"), QStringLiteral("voice")}                                                  },
                {PublicToolNames::tracks_clear_voice,                  {QStringLiteral("track_id")}                     },
                {PublicToolNames::master_set_gain,                     {QStringLiteral("gain")}                         },
                {PublicToolNames::master_set_pan,                      {QStringLiteral("pan")}                          },
                {PublicToolNames::master_set_mute,                     {QStringLiteral("mute")}                         },
                {PublicToolNames::master_set_solo,                     {QStringLiteral("solo")}                         },
                {PublicToolNames::clips_get,                           {QStringLiteral("clip_id")}                      },
                {PublicToolNames::clips_insert,                        {QStringLiteral("clips")}                        },
                {PublicToolNames::clips_duplicate,
                 {QStringLiteral("clip_ids"), QStringLiteral("destination")}                                            },
                {PublicToolNames::clips_remove,                        {QStringLiteral("clip_ids")}                     },
                {PublicToolNames::clips_move,                          {QStringLiteral("moves")}                        },
                {PublicToolNames::clips_resize_left,
                 {QStringLiteral("clip_id"), QStringLiteral("start")}                                                   },
                {PublicToolNames::clips_resize_right,
                 {QStringLiteral("clip_id"), QStringLiteral("end")}                                                     },
                {PublicToolNames::clips_rename,
                 {QStringLiteral("clip_id"), QStringLiteral("name")}                                                    },
                {PublicToolNames::clips_set_gain,
                 {QStringLiteral("clip_id"), QStringLiteral("gain")}                                                    },
                {PublicToolNames::clips_set_mute,
                 {QStringLiteral("clip_id"), QStringLiteral("mute")}                                                    },
                {PublicToolNames::clips_set_default_language,
                 {QStringLiteral("clip_id"), QStringLiteral("language_id")}                                             },
                {PublicToolNames::clips_get_voice_context,             {QStringLiteral("clip_id")}                      },
                {PublicToolNames::clips_use_track_voice,               {QStringLiteral("clip_id")}                      },
                {PublicToolNames::clips_set_voice,
                 {QStringLiteral("clip_id"), QStringLiteral("voice")}                                                   },
                {PublicToolNames::clips_clear_voice,                   {QStringLiteral("clip_id")}                      },
                {PublicToolNames::audio_clips_get,                     {QStringLiteral("clip_id")}                      },
                {PublicToolNames::audio_clips_import,
                 {QStringLiteral("track_id"), QStringLiteral("start"), QStringLiteral("path")}                          },
                {PublicToolNames::audio_clips_import_batch,
                 {QStringLiteral("items"), QStringLiteral("failure_policy")}                                            },
                {PublicToolNames::audio_clips_relocate,
                 {QStringLiteral("clip_id"), QStringLiteral("path")}                                                    },
                {PublicToolNames::audio_clips_confirm_path,            {QStringLiteral("clip_id")}                      },
                {PublicToolNames::voices_describe,                     {QStringLiteral("singer")}                       },
                {PublicToolNames::speaker_mix_get,                     {QStringLiteral("target")}                       },
                {PublicToolNames::speaker_mix_set_fixed,
                 {QStringLiteral("target"), QStringLiteral("mix")}                                                      },
                {PublicToolNames::speaker_mix_enable_dynamic,          {QStringLiteral("clip_id")}                      },
                {PublicToolNames::speaker_mix_disable_dynamic,         {QStringLiteral("clip_id")}                      },
                {PublicToolNames::speaker_mix_set_dynamic_bypass,
                 {QStringLiteral("clip_id"), QStringLiteral("bypassed")}                                                },
                {PublicToolNames::speaker_mix_keyframes_insert,
                 {QStringLiteral("clip_id"), QStringLiteral("position")}                                                },
                {PublicToolNames::speaker_mix_keyframes_move,
                 {QStringLiteral("clip_id"), QStringLiteral("moves")}                                                   },
                {PublicToolNames::speaker_mix_keyframes_set_weights,
                 {QStringLiteral("clip_id"), QStringLiteral("keyframe_id"),
                  QStringLiteral("weights")}                                                                            },
                {PublicToolNames::speaker_mix_keyframes_remove,
                 {QStringLiteral("clip_id"), QStringLiteral("keyframe_ids")}                                            },
                {PublicToolNames::notes_get,                           {QStringLiteral("clip_id")}                      },
                {PublicToolNames::notes_search,
                 {QStringLiteral("clip_id"), QStringLiteral("query"), QStringLiteral("mode")}                           },
                {PublicToolNames::notes_insert,
                 {QStringLiteral("clip_id"), QStringLiteral("notes")}                                                   },
                {PublicToolNames::notes_duplicate,
                 {QStringLiteral("source_clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("target_clip_id"), QStringLiteral("target_start")}                                     },
                {PublicToolNames::notes_remove,
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids")}                                                },
                {PublicToolNames::notes_move,
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("delta_tick"), QStringLiteral("delta_key")}                                            },
                {PublicToolNames::notes_resize_left,
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("delta_tick")}                                                                         },
                {PublicToolNames::notes_resize_right,
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("delta_tick")}                                                                         },
                {PublicToolNames::notes_split_at,
                 {QStringLiteral("clip_id"), QStringLiteral("note_id"),
                  QStringLiteral("local_position")}                                                                     },
                {PublicToolNames::notes_quantize,
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"), QStringLiteral("quantize"),
                  QStringLiteral("quantize_start"), QStringLiteral("quantize_length")}                                  },
                {PublicToolNames::notes_set_lyric,
                 {QStringLiteral("clip_id"), QStringLiteral("note_id"), QStringLiteral("lyric")}                        },
                {PublicToolNames::notes_set_language,
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"),
                  QStringLiteral("language")}                                                                           },
                {PublicToolNames::notes_set_pronunciation,
                 {QStringLiteral("clip_id"), QStringLiteral("note_id"),
                  QStringLiteral("pronunciation"), QStringLiteral("source")}                                            },
                {PublicToolNames::notes_reset_pronunciation,
                 {QStringLiteral("clip_id"), QStringLiteral("note_id")}                                                 },
                {PublicToolNames::notes_set_phonemes,
                 {QStringLiteral("clip_id"), QStringLiteral("note_id"), QStringLiteral("names")}                        },
                {PublicToolNames::notes_set_phoneme_offsets,
                 {QStringLiteral("clip_id"), QStringLiteral("note_id"), QStringLiteral("offsets")}                      },
                {PublicToolNames::notes_reset_phoneme_offsets,
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids")}                                                },
                {PublicToolNames::notes_reset_phonemes,
                 {QStringLiteral("clip_id"), QStringLiteral("note_id")}                                                 },
                {PublicToolNames::notes_fill_lyrics,
                 {QStringLiteral("clip_id"), QStringLiteral("note_ids"), QStringLiteral("text")}                        },
                {PublicToolNames::parameters_get_capabilities,         {QStringLiteral("clip_id")}                      },
                {PublicToolNames::parameters_get,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer")}                           },
                {PublicToolNames::parameters_replace,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("curves")}                                                                             },
                {PublicToolNames::parameters_draw,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("local_start"), QStringLiteral("step"), QStringLiteral("values")}                      },
                {PublicToolNames::parameters_erase,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("local_start"), QStringLiteral("local_end")}                                           },
                {PublicToolNames::parameters_bake,
                 {QStringLiteral("clip_id"), QStringLiteral("name")}                                                    },
                {PublicToolNames::parameters_insert_anchors,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("anchors")}                                                                            },
                {PublicToolNames::parameters_move_anchors,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("moves")}                                                                              },
                {PublicToolNames::parameters_remove_anchors,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("anchor_ids")}                                                                         },
                {PublicToolNames::parameters_set_anchor_interpolation,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("anchor_ids"), QStringLiteral("interpolation")}                                        },
                {PublicToolNames::tempos_set,                          {QStringLiteral("tick"), QStringLiteral("tempo")}},
                {PublicToolNames::tempos_delete,                       {QStringLiteral("tick")}                         },
                {PublicToolNames::time_signatures_set,
                 {QStringLiteral("bar_index"), QStringLiteral("numerator"),
                  QStringLiteral("denominator")}                                                                        },
                {PublicToolNames::time_signatures_delete,              {QStringLiteral("bar_index")}                    },
                {PublicToolNames::playback_seek,                       {QStringLiteral("position")}                     },
                {PublicToolNames::playback_set_loop,
                 {QStringLiteral("start"), QStringLiteral("end")}                                                       },
                {PublicToolNames::playback_set_loop_enabled,           {QStringLiteral("enabled")}                      },
                {PublicToolNames::exports_midi_preview,
                 {QStringLiteral("path"), QStringLiteral("options")}                                                    },
                {PublicToolNames::exports_midi_start,
                 {QStringLiteral("path"), QStringLiteral("options"),
                  QStringLiteral("overwrite_policy")}                                                                   },
                {PublicToolNames::exports_audio_preview,
                 {QStringLiteral("path"), QStringLiteral("options")}                                                    },
                {PublicToolNames::exports_audio_start,
                 {QStringLiteral("path"), QStringLiteral("options"),
                  QStringLiteral("overwrite_policy")}                                                                   },
                {PublicToolNames::extract_get_capabilities,
                 {QStringLiteral("source_audio_clip_id")}                                                               },
                {PublicToolNames::extract_pitch_start,
                 {QStringLiteral("source_audio_clip_id"), QStringLiteral("target_singing_clip_id"),
                  QStringLiteral("options")}                                                                            },
                {PublicToolNames::extract_midi_start,
                 {QStringLiteral("source_audio_clip_id"), QStringLiteral("destination"),
                  QStringLiteral("options")}                                                                            },
                {PublicToolNames::inference_get_capabilities,          {QStringLiteral("scope")}                        },
                {PublicToolNames::inference_get_status,                {QStringLiteral("scope")}                        },
                {PublicToolNames::inference_start,
                 {QStringLiteral("scope"), QStringLiteral("options")}                                                   },
                {PublicToolNames::inference_reset_stage,
                 {QStringLiteral("scope"), QStringLiteral("stage")}                                                     },
                {PublicToolNames::tasks_get,                           {QStringLiteral("task_id")}                      },
                {PublicToolNames::tasks_cancel,                        {QStringLiteral("task_id")}                      },
            };
        }

        QHash<QString, QStringList> optionalInputFields() {
            return {
                {PublicToolNames::automation_get_manifest,
                 {QStringLiteral("cursor"), QStringLiteral("limit")}                                               },
                {PublicToolNames::automation_get_options,       {QStringLiteral("partial_arguments")}              },
                {PublicToolNames::documents_new,                {QStringLiteral("template")}                       },
                {PublicToolNames::documents_open,
                 {QStringLiteral("format_id"), QStringLiteral("options"),
                  QStringLiteral("plan_digest")}                                                                   },
                {PublicToolNames::documents_save,               {QStringLiteral("overwrite_policy")}               },
                {PublicToolNames::documents_save_as,            {QStringLiteral("extension_policy")}               },
                {PublicToolNames::documents_import,
                 {QStringLiteral("format_id"), QStringLiteral("plan_digest")}                                      },
                {PublicToolNames::formats_list,                 {QStringLiteral("purpose")}                        },
                {PublicToolNames::tracks_list,                  {QStringLiteral("cursor"), QStringLiteral("limit")}},
                {PublicToolNames::clips_list,
                 {QStringLiteral("track_id"), QStringLiteral("type"), QStringLiteral("range"),
                  QStringLiteral("cursor"), QStringLiteral("limit")}                                               },
                {PublicToolNames::audio_clips_import,
                 {QStringLiteral("name"), QStringLiteral("gain"), QStringLiteral("mute")}                          },
                {PublicToolNames::audio_clips_confirm_path,     {QStringLiteral("path")}                           },
                {PublicToolNames::voices_list,
                 {QStringLiteral("query"), QStringLiteral("package_id"), QStringLiteral("cursor"),
                  QStringLiteral("limit")}                                                                         },
                {PublicToolNames::speaker_mix_keyframes_insert, {QStringLiteral("weights")}                        },
                {PublicToolNames::notes_get,                    {QStringLiteral("cursor"), QStringLiteral("limit")}},
                {PublicToolNames::notes_search,
                 {QStringLiteral("case_sensitive"), QStringLiteral("regex")}                                       },
                {PublicToolNames::notes_fill_lyrics,            {QStringLiteral("options")}                        },
                {PublicToolNames::parameters_draw,              {QStringLiteral("merge_mode")}                     },
                {PublicToolNames::parameters_bake,
                 {QStringLiteral("local_start"), QStringLiteral("local_end")}                                      },
                {PublicToolNames::parameters_insert_anchors,    {QStringLiteral("curve_id")}                       },
                {PublicToolNames::inference_start,              {QStringLiteral("stages")}                         },
                {PublicToolNames::tasks_list,
                 {QStringLiteral("state"), QStringLiteral("kind"), QStringLiteral("cursor"),
                  QStringLiteral("limit")}                                                                         },
            };
        }

        QJsonObject authoritativeInputSchema(const QString &id, const OperationKind kind) {
            QJsonObject properties;
            QStringList required;
            const auto add = [&](const QString &name, const bool requiredField) {
                if (properties.contains(name))
                    return;
                properties.insert(name, inputFieldSchema(id, name));
                if (requiredField)
                    required.append(name);
            };

            const bool lifecycle =
                id == PublicToolNames::documents_new || id == PublicToolNames::documents_open;
            const bool playbackWrite =
                id.startsWith(QStringLiteral("playback.")) && kind == OperationKind::Command;
            if (lifecycle) {
                add(QStringLiteral("current_document_id"), false);
                add(QStringLiteral("expected_revision"), false);
                add(QStringLiteral("validate_only"), false);
            } else if (documentQueryOperations().contains(id) ||
                       documentWriteOperations().contains(id) || playbackWrite) {
                add(QStringLiteral("document_id"), true);
            }
            if (documentWriteOperations().contains(id)) {
                add(QStringLiteral("expected_revision"), true);
                add(QStringLiteral("validate_only"), false);
                add(QStringLiteral("idempotency_key"), false);
            }
            if (playbackWrite) {
                add(QStringLiteral("expected_state_version"), false);
                add(QStringLiteral("validate_only"), false);
            }
            const auto requiredFields = requiredInputFields().value(id);
            for (const auto &name : requiredFields)
                add(name, true);
            const auto optionalFields = optionalInputFields().value(id);
            for (const auto &name : optionalFields)
                add(name, false);
            return JsonSchema::document(JsonSchema::object(properties, required));
        }

        QJsonObject inputSchema(const QString &id, const OperationKind kind) {
            return authoritativeInputSchema(id, kind);
        }

        QJsonObject partialRootSchema(QJsonObject schema) {
            std::function<QJsonValue(const QJsonValue &)> relax;
            relax = [&relax](const QJsonValue &value) -> QJsonValue {
                if (!value.isObject())
                    return value;
                auto result = value.toObject();
                result.remove(QStringLiteral("required"));
                result.remove(QStringLiteral("minProperties"));

                for (const auto &keyword :
                     {QStringLiteral("items"), QStringLiteral("additionalProperties"),
                      QStringLiteral("contains"), QStringLiteral("not"), QStringLiteral("if"),
                      QStringLiteral("then"), QStringLiteral("else")}) {
                    if (result.value(keyword).isObject())
                        result.insert(keyword, relax(result.value(keyword)));
                }
                for (const auto &keyword :
                     {QStringLiteral("oneOf"), QStringLiteral("anyOf"), QStringLiteral("allOf"),
                      QStringLiteral("prefixItems")}) {
                    if (!result.value(keyword).isArray())
                        continue;
                    QJsonArray relaxed;
                    for (const auto &entry : result.value(keyword).toArray())
                        relaxed.append(relax(entry));
                    result.insert(keyword, relaxed);
                }
                for (const auto &keyword :
                     {QStringLiteral("properties"), QStringLiteral("$defs")}) {
                    if (!result.value(keyword).isObject())
                        continue;
                    auto entries = result.value(keyword).toObject();
                    for (auto it = entries.begin(); it != entries.end(); ++it)
                        it.value() = relax(it.value());
                    result.insert(keyword, entries);
                }
                return result;
            };

            schema.remove(QStringLiteral("$schema"));
            if (schema.contains(QStringLiteral("oneOf"))) {
                QJsonObject mergedProperties;
                for (const auto &value : schema.value(QStringLiteral("oneOf")).toArray()) {
                    const auto branchProperties =
                        value.toObject().value(QStringLiteral("properties")).toObject();
                    for (auto it = branchProperties.constBegin(); it != branchProperties.constEnd();
                         ++it) {
                        mergedProperties.insert(it.key(), relax(it.value()));
                    }
                }
                return JsonSchema::object(mergedProperties);
            }
            return relax(schema).toObject();
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
                if (target.operationId == PublicToolNames::automation_get_options) {
                    continue;
                }
                QSet<QString> paths;
                for (const auto &sourceValue : target.valueSources) {
                    paths.insert(
                        sourceValue.toObject().value(QStringLiteral("field_path")).toString());
                }
                if (paths.isEmpty())
                    continue;
                const auto definitionKey = target.operationId;
                definitions.insert(definitionKey, partialRootSchema(target.inputSchema));
                auto orderedPaths = paths.values();
                std::sort(orderedPaths.begin(), orderedPaths.end());
                for (const auto &fieldPath : std::as_const(orderedPaths)) {
                    branches.append(JsonSchema::object(
                        {
                            {QStringLiteral("operation_id"),
                             JsonSchema::constant(target.operationId)                         },
                            {QStringLiteral("field_path"),        fieldPathSchema(fieldPath)  },
                            {QStringLiteral("partial_arguments"),
                             JsonSchema::reference(QStringLiteral("#/$defs/") + definitionKey)},
                    },
                        {QStringLiteral("operation_id"), QStringLiteral("field_path")}));
                }
            }
            auto root = JsonSchema::object(
                {
                    {QStringLiteral("operation_id"), nonEmptyStringSchema()},
                    {QStringLiteral("field_path"), nonEmptyStringSchema()},
                    {QStringLiteral("partial_arguments"),
                     JsonSchema::objectWithAdditionalSchema({}, {}, QJsonValue(true))},
            },
                {QStringLiteral("operation_id"), QStringLiteral("field_path")});
            root.insert(QStringLiteral("oneOf"), branches);
            return JsonSchema::document(std::move(root), definitions);
        }

        QJsonObject objectRefSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("kind"), JsonSchema::string(publicObjectKindValues())},
                    {QStringLiteral("id"),   identifierSchema()                          },
            },
                {QStringLiteral("kind"), QStringLiteral("id")});
        }

        QJsonObject mutationObjectSchema(const bool nullableVersions = false) {
            const auto createdObject = JsonSchema::object(
                {
                    {QStringLiteral("client_ref"), JsonSchema::string()},
                    {QStringLiteral("object"),     objectRefSchema()   },
            },
                {QStringLiteral("client_ref"), QStringLiteral("object")});
            const auto version =
                nullableVersions
                    ? JsonSchema::oneOf(QJsonArray{documentVersionSchema(), JsonSchema::null()})
                    : documentVersionSchema();
            const auto resolvedValue = JsonSchema::oneOf(QJsonArray{
                JsonSchema::null(),
                JsonSchema::boolean(),
                JsonSchema::number(),
                JsonSchema::string(),
            });
            const auto resolvedField = JsonSchema::object(
                {
                    {QStringLiteral("field_path"), nonEmptyStringSchema()},
                    {QStringLiteral("value"),      resolvedValue         },
            },
                {QStringLiteral("field_path"), QStringLiteral("value")});
            return JsonSchema::object(
                {
                    {QStringLiteral("previous"),             version                                },
                    {QStringLiteral("current"),              version                                },
                    {QStringLiteral("changed"),              JsonSchema::boolean()                  },
                    {QStringLiteral("validated_only"),       JsonSchema::boolean()                  },
                    {QStringLiteral("affected_objects"),     JsonSchema::array(objectRefSchema())   },
                    {QStringLiteral("created_objects"),      JsonSchema::array(createdObject)       },
                    {QStringLiteral("resolved_values"),      JsonSchema::array(resolvedField)       },
                    {QStringLiteral("presentation_effects"),
                     JsonSchema::array(nonEmptyStringSchema())                                      },
                    {QStringLiteral("warnings"),             JsonSchema::array(JsonSchema::string())},
            },
                {QStringLiteral("previous"), QStringLiteral("current"), QStringLiteral("changed"),
                 QStringLiteral("validated_only"), QStringLiteral("affected_objects"),
                 QStringLiteral("created_objects"), QStringLiteral("resolved_values"),
                 QStringLiteral("presentation_effects"), QStringLiteral("warnings")});
        }

        QJsonObject mutationSchema(const bool nullableVersions = false) {
            return JsonSchema::document(mutationObjectSchema(nullableVersions));
        }

        QJsonObject documentLifecycleResultSchema() {
            const auto nullableDocument =
                JsonSchema::oneOf(QJsonArray{documentVersionSchema(), JsonSchema::null()});
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("previous"),             nullableDocument                       },
                    {QStringLiteral("current"),              nullableDocument                       },
                    {QStringLiteral("path"),                 JsonSchema::string()                   },
                    {QStringLiteral("dirty"),                JsonSchema::boolean()                  },
                    {QStringLiteral("changed"),              JsonSchema::boolean()                  },
                    {QStringLiteral("validated_only"),       JsonSchema::boolean()                  },
                    {QStringLiteral("presentation_effects"),
                     JsonSchema::array(nonEmptyStringSchema())                                      },
                    {QStringLiteral("warnings"),             JsonSchema::array(JsonSchema::string())},
            },
                {QStringLiteral("previous"), QStringLiteral("current"), QStringLiteral("path"),
                 QStringLiteral("dirty"), QStringLiteral("changed"),
                 QStringLiteral("validated_only"), QStringLiteral("presentation_effects"),
                 QStringLiteral("warnings")}));
        }

        QJsonObject taskAcceptedSchema(const bool nullableDocument = false) {
            const auto acceptedDocument =
                nullableDocument
                    ? JsonSchema::oneOf(QJsonArray{documentVersionSchema(), JsonSchema::null()})
                    : documentVersionSchema();
            const auto accepted = JsonSchema::object(
                {
                    {QStringLiteral("task_id"),        uuidSchema()               },
                    {QStringLiteral("document"),       acceptedDocument           },
                    {QStringLiteral("validated_only"), JsonSchema::constant(false)},
            },
                {QStringLiteral("task_id"), QStringLiteral("document"),
                 QStringLiteral("validated_only")});
            const auto validated = JsonSchema::object(
                {
                    {QStringLiteral("document"),
                     JsonSchema::oneOf(QJsonArray{documentVersionSchema(), JsonSchema::null()})},
                    {QStringLiteral("validated_only"), JsonSchema::constant(true)              },
            },
                {QStringLiteral("document"), QStringLiteral("validated_only")});
            return JsonSchema::document(JsonSchema::oneOf(QJsonArray{accepted, validated}));
        }

        QJsonObject taskSnapshotObjectSchema() {
            const auto progress = JsonSchema::object(
                {
                    {QStringLiteral("minimum"),       JsonSchema::integer()},
                    {QStringLiteral("maximum"),       JsonSchema::integer()},
                    {QStringLiteral("value"),         JsonSchema::integer()},
                    {QStringLiteral("indeterminate"), JsonSchema::boolean()},
            },
                {QStringLiteral("minimum"), QStringLiteral("maximum"), QStringLiteral("value"),
                 QStringLiteral("indeterminate")});
            const auto taskError = JsonSchema::object(
                {
                    {QStringLiteral("code"),       nonEmptyStringSchema()},
                    {QStringLiteral("message"),    JsonSchema::string()  },
                    {QStringLiteral("field_path"), JsonSchema::string()  },
            },
                {QStringLiteral("code"), QStringLiteral("message")});
            QJsonArray branches;
            for (const auto &operation : publicValueDomainValues(PublicValueDomain::TaskKind)) {
                const bool opensDocument = operation == PublicToolNames::documents_open;
                const auto document =
                    opensDocument
                        ? JsonSchema::oneOf(QJsonArray{documentVersionSchema(), JsonSchema::null()})
                        : documentVersionSchema();
                branches.append(JsonSchema::object(
                    {
                        {QStringLiteral("task_id"),              uuidSchema()                                    },
                        {QStringLiteral("operation_id"),         JsonSchema::constant(operation)                 },
                        {QStringLiteral("document"),             document                                        },
                        {QStringLiteral("state"),                stringDomainSchema(PublicValueDomain::TaskState)},
                        {QStringLiteral("progress"),             progress                                        },
                        {QStringLiteral("result"),               mutationObjectSchema(opensDocument)             },
                        {QStringLiteral("error"),                taskError                                       },
                        {QStringLiteral("created_by_client_id"), JsonSchema::string()                            },
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
                QStringLiteral("null"),
                QStringLiteral("boolean"),
                QStringLiteral("object"),
                QStringLiteral("array"),
                QStringLiteral("number"),
                QStringLiteral("integer"),
                QStringLiteral("string"),
            });
            auto typeNames = JsonSchema::array(typeName, 1);
            typeNames.insert(QStringLiteral("uniqueItems"), true);
            const auto schemaObject = JsonSchema::object({
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
                {QStringLiteral("type"), JsonSchema::oneOf(QJsonArray{typeName, typeNames})},
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
                    {QStringLiteral("field_path"),        nonEmptyStringSchema()                   },
                    {QStringLiteral("operation_id"),      nonEmptyStringSchema()                   },
                    {QStringLiteral("context_fields"),    JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("minimum_profile"),   automationProfileSchema()                },
                    {QStringLiteral("host_availability"),
                     stringDomainSchema(PublicValueDomain::HostAvailability)                       },
            },
                {QStringLiteral("field_path"), QStringLiteral("operation_id"),
                 QStringLiteral("context_fields"), QStringLiteral("minimum_profile"),
                 QStringLiteral("host_availability")});
            const auto safety = JsonSchema::object(
                {
                    {QStringLiteral("read_only"),   JsonSchema::boolean()},
                    {QStringLiteral("destructive"), JsonSchema::boolean()},
                    {QStringLiteral("idempotent"),  JsonSchema::boolean()},
                    {QStringLiteral("open_world"),  JsonSchema::boolean()},
            },
                {QStringLiteral("read_only"), QStringLiteral("destructive"),
                 QStringLiteral("idempotent"), QStringLiteral("open_world")});
            const auto schemaDigest = JsonSchema::object(
                {
                    {QStringLiteral("input"),  digestSchema()},
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
                    {QStringLiteral("kind"), stringDomainSchema(PublicValueDomain::OperationKind)},
                    {QStringLiteral("input_schema"),
                     JsonSchema::reference(QStringLiteral("#/$defs/schema"))},
                    {QStringLiteral("output_schema"),
                     JsonSchema::reference(QStringLiteral("#/$defs/schema"))},
                    {QStringLiteral("value_sources"), JsonSchema::array(valueSource)},
                    {QStringLiteral("minimum_profile"), automationProfileSchema()},
                    {QStringLiteral("sync_mode"), stringDomainSchema(PublicValueDomain::SyncMode)},
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
                    QStringLiteral("operation_id"),
                    QStringLiteral("title"),
                    QStringLiteral("description"),
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
                     JsonSchema::array(JsonSchema::reference(QStringLiteral("#/$defs/operation")))},
                    {QStringLiteral("extensions"),
                     JsonSchema::objectWithAdditionalSchema({}, {}, QJsonValue(true))},
                    {QStringLiteral("next_cursor"), JsonSchema::string()},
            },
                {QStringLiteral("toolset_version"), QStringLiteral("digest"),
                 QStringLiteral("profile"), QStringLiteral("host_mode"),
                 QStringLiteral("operations"), QStringLiteral("extensions")});
            return JsonSchema::document(
                root, {
                          {QStringLiteral("json_value"), jsonValueMetaSchema()    },
                          {QStringLiteral("schema"),     jsonSchemaMetaSchema()   },
                          {QStringLiteral("operation"),  manifestOperationSchema()},
            });
        }

        QJsonObject queryEnvelopeSchema(const QString &name, const QJsonValue &valueSchema,
                                        const bool nextCursor = false) {
            QJsonObject properties{
                {QStringLiteral("document"), documentVersionSchema()},
                {name,                       valueSchema            },
            };
            if (nextCursor)
                properties.insert(QStringLiteral("next_cursor"), JsonSchema::string());
            return JsonSchema::document(
                JsonSchema::object(properties, {QStringLiteral("document"), name}));
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
                    {QStringLiteral("document_id"), uuidSchema()         },
                    {QStringLiteral("revision"),    revisionSchema()     },
                    {QStringLiteral("active"),      JsonSchema::boolean()},
            },
                {QStringLiteral("document_id"), QStringLiteral("revision")});
            const auto window = JsonSchema::object(
                {
                    {QStringLiteral("window_id"),   nonEmptyStringSchema()},
                    {QStringLiteral("document_id"), uuidSchema()          },
                    {QStringLiteral("active"),      JsonSchema::boolean() },
            },
                {QStringLiteral("window_id"), QStringLiteral("document_id")});
            auto documents = JsonSchema::array(document);
            documents.insert(QStringLiteral("maxItems"), 1);
            auto windows = JsonSchema::array(window);
            windows.insert(QStringLiteral("maxItems"), 1);
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("editor_instance_id"), uuidSchema()             },
                    {QStringLiteral("host_mode"),          hostModeSchema()         },
                    {QStringLiteral("profile"),            automationProfileSchema()},
                    {QStringLiteral("manifest"),           manifest                 },
                    {QStringLiteral("documents"),          documents                },
                    {QStringLiteral("windows"),            windows                  },
            },
                {QStringLiteral("editor_instance_id"), QStringLiteral("host_mode"),
                 QStringLiteral("profile"), QStringLiteral("manifest"), QStringLiteral("documents"),
                 QStringLiteral("windows")}));
        }

        QJsonObject optionsOutputSchema() {
            const auto optionValue = JsonSchema::oneOf(QJsonArray{
                JsonSchema::null(),
                JsonSchema::boolean(),
                JsonSchema::number(),
                JsonSchema::string(),
                singerRefSchema(),
                speakerRefSchema(),
                voiceRefSchema(),
            });
            const auto metadata = JsonSchema::object({
                {QStringLiteral("description"), JsonSchema::string()    },
                {QStringLiteral("group"),       JsonSchema::string()    },
                {QStringLiteral("deprecated"),  JsonSchema::boolean()   },
                {QStringLiteral("minimum"),     JsonSchema::integer()   },
                {QStringLiteral("maximum"),     JsonSchema::integer()   },
                {QStringLiteral("step"),        JsonSchema::integer(1.0)},
                {QStringLiteral("unit"),        JsonSchema::string()    },
            });
            const auto option = JsonSchema::object(
                {
                    {QStringLiteral("value"),              optionValue          },
                    {QStringLiteral("label"),              JsonSchema::string() },
                    {QStringLiteral("available"),          JsonSchema::boolean()},
                    {QStringLiteral("unavailable_reason"), JsonSchema::string() },
                    {QStringLiteral("metadata"),           metadata             },
            },
                {QStringLiteral("value"), QStringLiteral("label"), QStringLiteral("available")});
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("operation_id"),   nonEmptyStringSchema()                   },
                    {QStringLiteral("field_path"),     nonEmptyStringSchema()                   },
                    {QStringLiteral("options"),        JsonSchema::array(option)                },
                    {QStringLiteral("dependencies"),   JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("context_digest"), digestSchema()                           },
            },
                {QStringLiteral("operation_id"), QStringLiteral("field_path"),
                 QStringLiteral("options"), QStringLiteral("dependencies")}));
        }

        QJsonObject fileAccessOutputSchema() {
            const auto grant = JsonSchema::object(
                {
                    {QStringLiteral("path"),   nonEmptyStringSchema()        },
                    {QStringLiteral("access"),
                     stringDomainSchema(PublicValueDomain::GrantedFileAccess)},
            },
                {QStringLiteral("path"), QStringLiteral("access")});
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("read_roots"),     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("write_roots"),    JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("session_grants"), JsonSchema::array(grant)                 },
            },
                {QStringLiteral("read_roots"), QStringLiteral("write_roots"),
                 QStringLiteral("session_grants")}));
        }

        QJsonObject documentSnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("path"),          JsonSchema::string()                         },
                    {QStringLiteral("project_name"),  JsonSchema::string()                         },
                    {QStringLiteral("busy"),          JsonSchema::boolean()                        },
                    {QStringLiteral("saved"),         JsonSchema::boolean()                        },
                    {QStringLiteral("dirty"),         JsonSchema::boolean()                        },
                    {QStringLiteral("on_save_point"), JsonSchema::boolean()                        },
                    {QStringLiteral("lifecycle"),     JsonSchema::string(documentLifecycleValues())},
            },
                {QStringLiteral("path"), QStringLiteral("project_name"), QStringLiteral("busy"),
                 QStringLiteral("saved"), QStringLiteral("dirty"), QStringLiteral("on_save_point"),
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
                    {QStringLiteral("name"),        JsonSchema::string()    },
                    {QStringLiteral("start"),       JsonSchema::integer(0.0)},
                    {QStringLiteral("length"),      JsonSchema::integer(1.0)},
                    {QStringLiteral("clip_start"),  JsonSchema::integer(0.0)},
                    {QStringLiteral("clip_length"), JsonSchema::integer(1.0)},
                    {QStringLiteral("gain"),        JsonSchema::number()    },
                    {QStringLiteral("mute"),        JsonSchema::boolean()   },
            },
                {QStringLiteral("name"), QStringLiteral("start"), QStringLiteral("length"),
                 QStringLiteral("clip_start"), QStringLiteral("clip_length"),
                 QStringLiteral("gain"), QStringLiteral("mute")});
            const auto clip = JsonSchema::object(
                {
                    {QStringLiteral("clip_id"),          identifierSchema()                             },
                    {QStringLiteral("track_id"),         identifierSchema()                             },
                    {QStringLiteral("type"),             stringDomainSchema(PublicValueDomain::ClipType)},
                    {QStringLiteral("properties"),       clipProperties                                 },
                    {QStringLiteral("default_language"), JsonSchema::string()                           },
            },
                {QStringLiteral("clip_id"), QStringLiteral("track_id"), QStringLiteral("type"),
                 QStringLiteral("properties"), QStringLiteral("default_language")});
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
                {
                    {QStringLiteral("tracks"), JsonSchema::array(track)}
            },
                {QStringLiteral("tracks")});
        }

        QJsonObject noteSnapshotSchema() {
            const auto pronunciation = JsonSchema::object(
                {
                    {QStringLiteral("value"),  JsonSchema::string()            },
                    {QStringLiteral("source"),
                     stringDomainSchema(PublicValueDomain::PronunciationSource)},
            },
                {QStringLiteral("value")});
            const auto phoneme = JsonSchema::object(
                {
                    {QStringLiteral("symbol"),   nonEmptyStringSchema()},
                    {QStringLiteral("language"), JsonSchema::string()  },
                    {QStringLiteral("offset"),   JsonSchema::integer() },
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
                {QStringLiteral("note_id"), QStringLiteral("clip_id"), QStringLiteral("client_ref"),
                 QStringLiteral("local_start"), QStringLiteral("length"),
                 QStringLiteral("key_index"), QStringLiteral("cent_shift"), QStringLiteral("lyric"),
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
                {QStringLiteral("type"), QStringLiteral("curve_id"), QStringLiteral("local_start"),
                 QStringLiteral("step"), QStringLiteral("values")});
            const auto anchorNode = JsonSchema::object(
                {
                    {QStringLiteral("anchor_id"),     identifierSchema()   },
                    {QStringLiteral("position"),      JsonSchema::integer()},
                    {QStringLiteral("value"),         JsonSchema::integer()},
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
                {QStringLiteral("type"), QStringLiteral("curve_id"), QStringLiteral("nodes")});
            return JsonSchema::oneOf(QJsonArray{draw, anchor});
        }

        QJsonObject parameterSnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("clip_id"), identifierSchema()                      },
                    {QStringLiteral("name"),    parameterNameSchema()                   },
                    {QStringLiteral("layer"),   parameterLayerSchema()                  },
                    {QStringLiteral("curves"),  JsonSchema::array(curveSnapshotSchema())},
            },
                {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                 QStringLiteral("curves")});
        }

        QJsonObject parameterCapabilitiesSchema() {
            const auto range = JsonSchema::object(
                {
                    {QStringLiteral("minimum"), JsonSchema::integer()   },
                    {QStringLiteral("maximum"), JsonSchema::integer()   },
                    {QStringLiteral("step"),    JsonSchema::integer(1.0)},
                    {QStringLiteral("unit"),    JsonSchema::string()    },
            },
                {QStringLiteral("minimum"), QStringLiteral("maximum"), QStringLiteral("step"),
                 QStringLiteral("unit")});
            const auto parameter = JsonSchema::object(
                {
                    {QStringLiteral("name"), parameterNameSchema()},
                    {QStringLiteral("layers"), JsonSchema::array(parameterLayerSchema(), 1)},
                    {QStringLiteral("curve_types"),
                     JsonSchema::array(stringDomainSchema(PublicValueDomain::CurveType), 1)},
                    {QStringLiteral("interpolations"), JsonSchema::array(interpolationSchema(), 1)},
                    {QStringLiteral("editable"), JsonSchema::boolean()},
                    {QStringLiteral("range"), range},
            },
                {QStringLiteral("name"), QStringLiteral("layers"), QStringLiteral("curve_types"),
                 QStringLiteral("interpolations"), QStringLiteral("editable"),
                 QStringLiteral("range")});
            return JsonSchema::object(
                {
                    {QStringLiteral("clip_id"),    identifierSchema()          },
                    {QStringLiteral("parameters"), JsonSchema::array(parameter)},
            },
                {QStringLiteral("clip_id"), QStringLiteral("parameters")});
        }

        QJsonObject timelineSnapshotSchema() {
            const auto tempo = JsonSchema::object(
                {
                    {QStringLiteral("tick"),  JsonSchema::integer()  },
                    {QStringLiteral("tempo"), JsonSchema::number(1.0)},
            },
                {QStringLiteral("tick"), QStringLiteral("tempo")});
            const auto signature = JsonSchema::object(
                {
                    {QStringLiteral("bar_index"),   JsonSchema::integer(0.0)},
                    {QStringLiteral("numerator"),   JsonSchema::integer(1.0)},
                    {QStringLiteral("denominator"), JsonSchema::integer(1.0)},
            },
                {QStringLiteral("bar_index"), QStringLiteral("numerator"),
                 QStringLiteral("denominator")});
            return JsonSchema::object(
                {
                    {QStringLiteral("tempos"),          JsonSchema::array(tempo)    },
                    {QStringLiteral("time_signatures"), JsonSchema::array(signature)},
            },
                {QStringLiteral("tempos"), QStringLiteral("time_signatures")});
        }

        QJsonObject historySnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("can_undo"),      JsonSchema::boolean()},
                    {QStringLiteral("can_redo"),      JsonSchema::boolean()},
                    {QStringLiteral("on_save_point"), JsonSchema::boolean()},
                    {QStringLiteral("undo_name"),     JsonSchema::string() },
                    {QStringLiteral("redo_name"),     JsonSchema::string() },
            },
                {QStringLiteral("can_undo"), QStringLiteral("can_redo"),
                 QStringLiteral("on_save_point"), QStringLiteral("undo_name"),
                 QStringLiteral("redo_name")});
        }

        QJsonObject voiceSummarySchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("package_id"), nonEmptyStringSchema()},
                    {QStringLiteral("singer_id"),  nonEmptyStringSchema()},
                    {QStringLiteral("name"),       nonEmptyStringSchema()},
                    {QStringLiteral("version"),    nonEmptyStringSchema()},
            },
                {QStringLiteral("package_id"), QStringLiteral("singer_id"), QStringLiteral("name"),
                 QStringLiteral("version")});
        }

        QJsonObject voiceSnapshotSchema() {
            const auto optionalIdentifier =
                JsonSchema::oneOf(QJsonArray{nonEmptyStringSchema(), JsonSchema::null()});
            const auto speaker = JsonSchema::object(
                {
                    {QStringLiteral("speaker_id"), nonEmptyStringSchema()                   },
                    {QStringLiteral("name"),       JsonSchema::string()                     },
                    {QStringLiteral("languages"),  JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("mixable"),    JsonSchema::boolean()                    },
                    {QStringLiteral("default"),    JsonSchema::boolean()                    },
            },
                {QStringLiteral("speaker_id"), QStringLiteral("name"), QStringLiteral("languages"),
                 QStringLiteral("mixable"), QStringLiteral("default")});
            const auto language = JsonSchema::object(
                {
                    {QStringLiteral("language_id"), nonEmptyStringSchema()},
                    {QStringLiteral("name"),        JsonSchema::string()  },
                    {QStringLiteral("g2p_id"),      JsonSchema::string()  },
                    {QStringLiteral("g2p_ready"),   JsonSchema::boolean() },
                    {QStringLiteral("default"),     JsonSchema::boolean() },
            },
                {QStringLiteral("language_id"), QStringLiteral("name"), QStringLiteral("g2p_id"),
                 QStringLiteral("g2p_ready"), QStringLiteral("default")});
            return JsonSchema::object(
                {
                    {QStringLiteral("package_id"),         nonEmptyStringSchema()     },
                    {QStringLiteral("singer_id"),          nonEmptyStringSchema()     },
                    {QStringLiteral("name"),               nonEmptyStringSchema()     },
                    {QStringLiteral("version"),            nonEmptyStringSchema()     },
                    {QStringLiteral("speakers"),           JsonSchema::array(speaker) },
                    {QStringLiteral("languages"),          JsonSchema::array(language)},
                    {QStringLiteral("default_speaker_id"), optionalIdentifier         },
                    {QStringLiteral("default_language"),   optionalIdentifier         },
                    {QStringLiteral("g2p_ready"),          JsonSchema::boolean()      },
                    {QStringLiteral("resolution_state"),
                     stringDomainSchema(PublicValueDomain::VoiceResolutionState)      },
                    {QStringLiteral("mixing_supported"),   JsonSchema::boolean()      },
            },
                {QStringLiteral("package_id"), QStringLiteral("singer_id"), QStringLiteral("name"),
                 QStringLiteral("version"), QStringLiteral("speakers"), QStringLiteral("languages"),
                 QStringLiteral("default_speaker_id"), QStringLiteral("default_language"),
                 QStringLiteral("g2p_ready"), QStringLiteral("resolution_state"),
                 QStringLiteral("mixing_supported")});
        }

        QJsonObject formatCapabilitySchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("id"),                 nonEmptyStringSchema()                   },
                    {QStringLiteral("display_name"),       nonEmptyStringSchema()                   },
                    {QStringLiteral("extensions"),         JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("can_open"),           JsonSchema::boolean()                    },
                    {QStringLiteral("can_import"),         JsonSchema::boolean()                    },
                    {QStringLiteral("can_export"),         JsonSchema::boolean()                    },
                    {QStringLiteral("available"),          JsonSchema::boolean()                    },
                    {QStringLiteral("unavailable_reason"), JsonSchema::string()                     },
                    {QStringLiteral("option_schema"),
                     JsonSchema::reference(QStringLiteral("#/$defs/schema"))                        },
            },
                {QStringLiteral("id"), QStringLiteral("display_name"), QStringLiteral("extensions"),
                 QStringLiteral("can_open"), QStringLiteral("can_import"),
                 QStringLiteral("can_export"), QStringLiteral("available"),
                 QStringLiteral("unavailable_reason"), QStringLiteral("option_schema")});
        }

        QJsonObject formatsOutputSchema() {
            const auto root = JsonSchema::object(
                {
                    {QStringLiteral("formats"), JsonSchema::array(formatCapabilitySchema())}
            },
                {QStringLiteral("formats")});
            return JsonSchema::document(root,
                                        {
                                            {QStringLiteral("json_value"), jsonValueMetaSchema() },
                                            {QStringLiteral("schema"),     jsonSchemaMetaSchema()},
            });
        }

        QJsonObject audioExportCapabilitiesSchema() {
            const auto source = JsonSchema::object(
                {
                    {QStringLiteral("id"),        identifierSchema()       },
                    {QStringLiteral("name"),      nonEmptyStringSchema()   },
                    {QStringLiteral("kind"),
                     stringDomainSchema(PublicValueDomain::AudioSourceKind)},
                    {QStringLiteral("available"), JsonSchema::boolean()    },
            },
                {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("kind"),
                 QStringLiteral("available")});
            return JsonSchema::object(
                {
                    {QStringLiteral("formats"), JsonSchema::array(nonEmptyStringSchema(), 1)},
                    {QStringLiteral("sample_rates"),
                     JsonSchema::array(
                         JsonSchema::integer(MinimumAudioSampleRate, MaximumAudioSampleRate), 1)},
                    {QStringLiteral("channel_modes"),
                     JsonSchema::array(stringDomainSchema(PublicValueDomain::ChannelMode), 1)},
                    {QStringLiteral("mixing_modes"),
                     JsonSchema::array(stringDomainSchema(PublicValueDomain::AudioMixingMode), 1)},
                    {QStringLiteral("source_modes"),
                     JsonSchema::array(stringDomainSchema(PublicValueDomain::AudioSourceMode), 1)},
                    {QStringLiteral("sources"), JsonSchema::array(source)},
            },
                {QStringLiteral("formats"), QStringLiteral("sample_rates"),
                 QStringLiteral("channel_modes"), QStringLiteral("mixing_modes"),
                 QStringLiteral("source_modes"), QStringLiteral("sources")});
        }

        QJsonObject audioExportPreviewSchema() {
            const auto target = JsonSchema::object(
                {
                    {QStringLiteral("path"), nonEmptyStringSchema()}
            },
                {QStringLiteral("path")});
            const auto plan = JsonSchema::object(
                {
                    {QStringLiteral("base_directory"), JsonSchema::string()     },
                    {QStringLiteral("targets"),        JsonSchema::array(target)},
            },
                {QStringLiteral("base_directory"), QStringLiteral("targets")});
            const auto diagnostic = JsonSchema::object(
                {
                    {QStringLiteral("code"),
                     JsonSchema::string(
                         {QStringLiteral("no_files"), QStringLiteral("duplicate_paths"),
                          QStringLiteral("will_overwrite"), QStringLiteral("unrecognized_template"),
                          QStringLiteral("lossy_format")})                                      },
                    {QStringLiteral("severity"), JsonSchema::constant(QStringLiteral("warning"))},
                    {QStringLiteral("message"),  nonEmptyStringSchema()                         },
                    {QStringLiteral("blocking"), JsonSchema::boolean()                          },
            },
                {QStringLiteral("code"), QStringLiteral("severity"), QStringLiteral("message"),
                 QStringLiteral("blocking")});
            return JsonSchema::object(
                {
                    {QStringLiteral("plan"),        plan                         },
                    {QStringLiteral("diagnostics"), JsonSchema::array(diagnostic)},
            },
                {QStringLiteral("plan"), QStringLiteral("diagnostics")});
        }

        QJsonObject modelCapabilitySchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("model_id"),           nonEmptyStringSchema()},
                    {QStringLiteral("display_name"),       nonEmptyStringSchema()},
                    {QStringLiteral("available"),          JsonSchema::boolean() },
                    {QStringLiteral("unavailable_reason"), JsonSchema::string()  },
            },
                {QStringLiteral("model_id"), QStringLiteral("display_name"),
                 QStringLiteral("available"), QStringLiteral("unavailable_reason")});
        }

        QJsonObject extractionCapabilitiesSchema() {
            const auto extractionModel = JsonSchema::object(
                {
                    {QStringLiteral("model_id"),           nonEmptyStringSchema()},
                    {QStringLiteral("display_name"),       nonEmptyStringSchema()},
                    {QStringLiteral("configured"),         JsonSchema::boolean() },
                    {QStringLiteral("available"),          JsonSchema::boolean() },
                    {QStringLiteral("unavailable_reason"), JsonSchema::string()  },
            },
                {QStringLiteral("model_id"), QStringLiteral("display_name"),
                 QStringLiteral("configured"), QStringLiteral("available"),
                 QStringLiteral("unavailable_reason")});
            const auto commonProperties = QJsonObject{
                {QStringLiteral("supported"),          JsonSchema::boolean()                 },
                {QStringLiteral("source_supported"),   JsonSchema::boolean()                 },
                {QStringLiteral("available"),          JsonSchema::boolean()                 },
                {QStringLiteral("module_state"),
                 JsonSchema::string({QStringLiteral("ready"), QStringLiteral("unavailable")})},
                {QStringLiteral("unavailable_reason"), JsonSchema::string()                  },
                {QStringLiteral("models"),             JsonSchema::array(extractionModel)    },
                {QStringLiteral("option_schema"),
                 JsonSchema::reference(QStringLiteral("#/$defs/schema"))                     },
            };
            const auto commonRequired = QStringList{
                QStringLiteral("supported"),          QStringLiteral("source_supported"),
                QStringLiteral("available"),          QStringLiteral("module_state"),
                QStringLiteral("unavailable_reason"), QStringLiteral("models"),
                QStringLiteral("option_schema"),      QStringLiteral("range_support"),
            };
            auto pitchProperties = commonProperties;
            pitchProperties.insert(
                QStringLiteral("range_support"),
                JsonSchema::object(
                    {
                        {QStringLiteral("source_range"),
                         JsonSchema::constant(QStringLiteral("visible_audio_clip"))     },
                        {QStringLiteral("custom_frequency"), JsonSchema::constant(false)},
            },
                    {QStringLiteral("source_range"), QStringLiteral("custom_frequency")}));
            auto midiProperties = commonProperties;
            midiProperties.insert(
                QStringLiteral("range_support"),
                JsonSchema::object(
                    {
                        {QStringLiteral("source_range"),
                         JsonSchema::constant(QStringLiteral("visible_audio_clip"))},
                        {QStringLiteral("minimum_note_length"),
                         JsonSchema::object(
                             {
                                 {QStringLiteral("minimum"), JsonSchema::constant(1)},
                                 {QStringLiteral("maximum"),
                                  JsonSchema::constant(std::numeric_limits<int>::max())},
                             }, {QStringLiteral("minimum"), QStringLiteral("maximum")})},
            },
                    {QStringLiteral("source_range"), QStringLiteral("minimum_note_length")}));
            return JsonSchema::object(
                {
                    {QStringLiteral("source_audio_clip_id"), identifierSchema()},
                    {QStringLiteral("pitch"), JsonSchema::object(pitchProperties, commonRequired)},
                    {QStringLiteral("midi"), JsonSchema::object(midiProperties, commonRequired)},
                    {QStringLiteral("languages"), JsonSchema::array(nonEmptyStringSchema())},
            },
                {QStringLiteral("source_audio_clip_id"), QStringLiteral("pitch"),
                 QStringLiteral("midi"), QStringLiteral("languages")});
        }

        QJsonObject extractionCapabilitiesOutputSchema() {
            const auto root = JsonSchema::object(
                {
                    {QStringLiteral("document"),     documentVersionSchema()       },
                    {QStringLiteral("capabilities"), extractionCapabilitiesSchema()},
            },
                {QStringLiteral("document"), QStringLiteral("capabilities")});
            return JsonSchema::document(root,
                                        {
                                            {QStringLiteral("json_value"), jsonValueMetaSchema() },
                                            {QStringLiteral("schema"),     jsonSchemaMetaSchema()},
            });
        }

        QJsonObject inferenceCapabilitiesSchema() {
            const auto emptyScope = JsonSchema::object();
            const auto scope = JsonSchema::oneOf(QJsonArray{emptyScope, inferenceScopeSchema()});
            const auto namedCapability = JsonSchema::object(
                {
                    {QStringLiteral("id"),                 nonEmptyStringSchema()},
                    {QStringLiteral("display_name"),       nonEmptyStringSchema()},
                    {QStringLiteral("available"),          JsonSchema::boolean() },
                    {QStringLiteral("unavailable_reason"), JsonSchema::string()  },
            },
                {QStringLiteral("id"), QStringLiteral("display_name"), QStringLiteral("available"),
                 QStringLiteral("unavailable_reason")});
            return JsonSchema::object(
                {
                    {QStringLiteral("scope"), scope},
                    {QStringLiteral("stages"),
                     JsonSchema::array(stringDomainSchema(PublicValueDomain::InferenceStage), 1)},
                    {QStringLiteral("providers"), JsonSchema::array(namedCapability)},
                    {QStringLiteral("devices"), JsonSchema::array(namedCapability)},
                    {QStringLiteral("models"), JsonSchema::array(modelCapabilitySchema())},
            },
                {QStringLiteral("scope"), QStringLiteral("stages"), QStringLiteral("providers"),
                 QStringLiteral("devices"), QStringLiteral("models")});
        }

        QJsonObject playbackSnapshotSchema() {
            const auto loop = JsonSchema::object(
                {
                    {QStringLiteral("enabled"), JsonSchema::boolean()},
                    {QStringLiteral("start"),
                     JsonSchema::integer(0.0, std::numeric_limits<int>::max())},
                    {QStringLiteral("end"),
                     JsonSchema::integer(0.0, std::numeric_limits<int>::max())},
            },
                {QStringLiteral("enabled"), QStringLiteral("start"), QStringLiteral("end")});
            return JsonSchema::object(
                {
                    {QStringLiteral("state_version"), revisionSchema()                                    },
                    {QStringLiteral("state"),         stringDomainSchema(PublicValueDomain::PlaybackState)},
                    {QStringLiteral("playable"),      JsonSchema::boolean()                               },
                    {QStringLiteral("position"),      JsonSchema::number(0.0)                             },
                    {QStringLiteral("last_position"), JsonSchema::number(0.0)                             },
                    {QStringLiteral("loop"),          loop                                                },
            },
                {QStringLiteral("state_version"), QStringLiteral("state"),
                 QStringLiteral("playable"), QStringLiteral("position"),
                 QStringLiteral("last_position"), QStringLiteral("loop")});
        }

        QJsonObject playbackMutationSchema() {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("state_version"), revisionSchema()                       },
                    {QStringLiteral("changed"),       JsonSchema::boolean()                  },
                    {QStringLiteral("playback"),      playbackSnapshotSchema()               },
                    {QStringLiteral("warnings"),      JsonSchema::array(JsonSchema::string())},
            },
                {QStringLiteral("state_version"), QStringLiteral("changed"),
                 QStringLiteral("playback"), QStringLiteral("warnings")}));
        }

        QJsonObject persistentPlaybackMutationSchema() {
            auto root = mutationObjectSchema();
            auto properties = root.value(QStringLiteral("properties")).toObject();
            properties.insert(QStringLiteral("state_version"), revisionSchema());
            properties.insert(QStringLiteral("playback"), playbackSnapshotSchema());
            root.insert(QStringLiteral("properties"), properties);
            auto required = root.value(QStringLiteral("required")).toArray();
            required.append(QStringLiteral("state_version"));
            required.append(QStringLiteral("playback"));
            root.insert(QStringLiteral("required"), required);
            return JsonSchema::document(root);
        }

        QJsonObject trackSnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("track_id"), identifierSchema()},
                    {QStringLiteral("index"), JsonSchema::integer(0.0)},
                    {QStringLiteral("name"), JsonSchema::string()},
                    {QStringLiteral("color_index"),
                     JsonSchema::integer(0.0, TrackPaletteColorCount - 1)},
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                    {QStringLiteral("solo"), JsonSchema::boolean()},
                    {QStringLiteral("default_language_id"), JsonSchema::string()},
                    {QStringLiteral("clip_count"), JsonSchema::integer(0.0)},
            },
                {QStringLiteral("track_id"), QStringLiteral("index"), QStringLiteral("name"),
                 QStringLiteral("color_index"), QStringLiteral("gain"), QStringLiteral("pan"),
                 QStringLiteral("mute"), QStringLiteral("solo"),
                 QStringLiteral("default_language_id"), QStringLiteral("clip_count")});
        }

        QJsonObject masterSnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("gain"), JsonSchema::number()},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
                    {QStringLiteral("mute"), JsonSchema::boolean()},
                    {QStringLiteral("solo"), JsonSchema::boolean()},
                    {QStringLiteral("metering_available"), JsonSchema::boolean()},
            },
                {QStringLiteral("gain"), QStringLiteral("pan"), QStringLiteral("mute"),
                 QStringLiteral("solo"), QStringLiteral("metering_available")});
        }

        QJsonObject clipSnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("clip_id"),             identifierSchema()                             },
                    {QStringLiteral("track_id"),            identifierSchema()                             },
                    {QStringLiteral("type"),                stringDomainSchema(PublicValueDomain::ClipType)},
                    {QStringLiteral("name"),                JsonSchema::string()                           },
                    {QStringLiteral("start"),               JsonSchema::integer(0.0)                       },
                    {QStringLiteral("length"),              JsonSchema::integer(1.0)                       },
                    {QStringLiteral("gain"),                JsonSchema::number()                           },
                    {QStringLiteral("mute"),                JsonSchema::boolean()                          },
                    {QStringLiteral("default_language_id"), JsonSchema::string()                           },
            },
                {QStringLiteral("clip_id"), QStringLiteral("track_id"), QStringLiteral("type"),
                 QStringLiteral("name"), QStringLiteral("start"), QStringLiteral("length"),
                 QStringLiteral("gain"), QStringLiteral("mute"),
                 QStringLiteral("default_language_id")});
        }

        QJsonObject voiceContextSnapshotSchema() {
            const auto nullableVoice =
                JsonSchema::oneOf(QJsonArray{voiceSelectionSchema(), JsonSchema::null()});
            const auto nullableLanguage =
                JsonSchema::oneOf(QJsonArray{languageSelectionSchema(), JsonSchema::null()});
            return JsonSchema::object(
                {
                    {QStringLiteral("own_voice"),          nullableVoice        },
                    {QStringLiteral("effective_voice"),    nullableVoice        },
                    {QStringLiteral("inherits_track"),     JsonSchema::boolean()},
                    {QStringLiteral("default_language"),   nullableLanguage     },
                    {QStringLiteral("available"),          JsonSchema::boolean()},
                    {QStringLiteral("unavailable_reason"), JsonSchema::string() },
            },
                {QStringLiteral("own_voice"), QStringLiteral("effective_voice"),
                 QStringLiteral("inherits_track"), QStringLiteral("default_language"),
                 QStringLiteral("available"), QStringLiteral("unavailable_reason")});
        }

        QJsonObject audioClipSnapshotSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("clip_id"),          identifierSchema()                       },
                    {QStringLiteral("path"),             JsonSchema::string()                     },
                    {QStringLiteral("path_status"),
                     JsonSchema::string({QStringLiteral("resolved"), QStringLiteral("missing"),
                                         QStringLiteral("candidate")})                            },
                    {QStringLiteral("candidate_paths"),  JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("hash_exists"),      JsonSchema::boolean()                    },
                    {QStringLiteral("duration_seconds"),
                     JsonSchema::oneOf(QJsonArray{JsonSchema::number(0.0), JsonSchema::null()})   },
                    {QStringLiteral("sample_rate"),
                     JsonSchema::oneOf(QJsonArray{JsonSchema::integer(1.0), JsonSchema::null()})  },
                    {QStringLiteral("channels"),
                     JsonSchema::oneOf(QJsonArray{JsonSchema::integer(1.0), JsonSchema::null()})  },
            },
                {QStringLiteral("clip_id"), QStringLiteral("path"), QStringLiteral("path_status"),
                 QStringLiteral("candidate_paths"), QStringLiteral("hash_exists"),
                 QStringLiteral("duration_seconds"), QStringLiteral("sample_rate"),
                 QStringLiteral("channels")});
        }

        QJsonObject speakerMixSnapshotSchema() {
            const auto keyframe = JsonSchema::object(
                {
                    {QStringLiteral("keyframe_id"), identifierSchema()},
                    {QStringLiteral("position"), JsonSchema::integer(0.0)},
                    {QStringLiteral("weights"),
                     JsonSchema::array(JsonSchema::number(MinimumMixWeight, MaximumMixWeight), 1,
                     MaximumCommandCollectionItems)},
            },
                {QStringLiteral("keyframe_id"), QStringLiteral("position"),
                 QStringLiteral("weights")});
            return JsonSchema::object(
                {
                    {QStringLiteral("target"),          speakerMixTargetSchema()   },
                    {QStringLiteral("mix"),             speakerMixDraftSchema()    },
                    {QStringLiteral("dynamic_enabled"), JsonSchema::boolean()      },
                    {QStringLiteral("bypassed"),        JsonSchema::boolean()      },
                    {QStringLiteral("keyframes"),       JsonSchema::array(keyframe)},
            },
                {QStringLiteral("target"), QStringLiteral("mix"), QStringLiteral("dynamic_enabled"),
                 QStringLiteral("bypassed"), QStringLiteral("keyframes")});
        }

        QJsonObject formatInspectionOutputSchema() {
            const auto source = JsonSchema::object(
                {
                    {QStringLiteral("index"), JsonSchema::integer(0.0)                       },
                    {QStringLiteral("name"),  JsonSchema::string()                           },
                    {QStringLiteral("kind"),
                     JsonSchema::string({QStringLiteral("track"), QStringLiteral("channel")})},
            },
                {QStringLiteral("index"), QStringLiteral("name"), QStringLiteral("kind")});
            auto root = JsonSchema::object(
                {
                    {QStringLiteral("path"),               nonEmptyStringSchema()                 },
                    {QStringLiteral("purpose"),
                     JsonSchema::string({QStringLiteral("open"), QStringLiteral("import")})       },
                    {QStringLiteral("format_id"),          nonEmptyStringSchema()                 },
                    {QStringLiteral("sources"),            JsonSchema::array(source)              },
                    {QStringLiteral("encoding"),           JsonSchema::string()                   },
                    {QStringLiteral("lyrics_preview"),     JsonSchema::array(JsonSchema::string())},
                    {QStringLiteral("timeline_supported"), JsonSchema::boolean()                  },
                    {QStringLiteral("option_schema"),
                     JsonSchema::reference(QStringLiteral("#/$defs/schema"))                      },
                    {QStringLiteral("plan_digest"),        digestSchema()                         },
            },
                {QStringLiteral("path"), QStringLiteral("purpose"), QStringLiteral("format_id"),
                 QStringLiteral("sources"), QStringLiteral("encoding"),
                 QStringLiteral("lyrics_preview"), QStringLiteral("timeline_supported"),
                 QStringLiteral("option_schema"), QStringLiteral("plan_digest")});
            return JsonSchema::document(root,
                                        {
                                            {QStringLiteral("json_value"), jsonValueMetaSchema() },
                                            {QStringLiteral("schema"),     jsonSchemaMetaSchema()},
            });
        }

        QJsonObject noteSearchOutputSchema() {
            const auto match = JsonSchema::object(
                {
                    {QStringLiteral("note_id"),     identifierSchema()      },
                    {QStringLiteral("local_start"), JsonSchema::integer(0.0)},
                    {QStringLiteral("length"),      JsonSchema::integer(1.0)},
                    {QStringLiteral("lyric"),       JsonSchema::string()    },
            },
                {QStringLiteral("note_id"), QStringLiteral("local_start"), QStringLiteral("length"),
                 QStringLiteral("lyric")});
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("document"), documentVersionSchema() },
                    {QStringLiteral("matches"),  JsonSchema::array(match)},
            },
                {QStringLiteral("document"), QStringLiteral("matches")}));
        }

        QJsonObject midiExportCapabilitiesSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("track_ids"), JsonSchema::array(identifierSchema())},
                    {QStringLiteral("clip_ids"), JsonSchema::array(identifierSchema())},
                    {QStringLiteral("formats"), JsonSchema::array(nonEmptyStringSchema(), 1)},
                    {QStringLiteral("lyrics_supported"), JsonSchema::boolean()},
                    {QStringLiteral("option_schema"),
                     JsonSchema::reference(QStringLiteral("#/$defs/schema"))},
            },
                {QStringLiteral("track_ids"), QStringLiteral("clip_ids"), QStringLiteral("formats"),
                 QStringLiteral("lyrics_supported"), QStringLiteral("option_schema")});
        }

        QJsonObject midiExportCapabilitiesOutputSchema() {
            const auto root = JsonSchema::object(
                {
                    {QStringLiteral("document"),     documentVersionSchema()       },
                    {QStringLiteral("capabilities"), midiExportCapabilitiesSchema()},
            },
                {QStringLiteral("document"), QStringLiteral("capabilities")});
            return JsonSchema::document(root,
                                        {
                                            {QStringLiteral("json_value"), jsonValueMetaSchema() },
                                            {QStringLiteral("schema"),     jsonSchemaMetaSchema()},
            });
        }

        QJsonObject exportPlanSchema() {
            const auto diagnostic = JsonSchema::object(
                {
                    {QStringLiteral("code"),     nonEmptyStringSchema()},
                    {QStringLiteral("message"),  JsonSchema::string()  },
                    {QStringLiteral("blocking"), JsonSchema::boolean() },
            },
                {QStringLiteral("code"), QStringLiteral("message"), QStringLiteral("blocking")});
            return JsonSchema::object(
                {
                    {QStringLiteral("targets"),     JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("diagnostics"), JsonSchema::array(diagnostic)            },
                    {QStringLiteral("plan_digest"), digestSchema()                           },
            },
                {QStringLiteral("targets"), QStringLiteral("diagnostics"),
                 QStringLiteral("plan_digest")});
        }

        QJsonObject midiExportPlanSchema() {
            const auto diagnostic = JsonSchema::object(
                {
                    {QStringLiteral("code"),     nonEmptyStringSchema()},
                    {QStringLiteral("message"),  JsonSchema::string()  },
                    {QStringLiteral("blocking"), JsonSchema::boolean() },
            },
                {QStringLiteral("code"), QStringLiteral("message"), QStringLiteral("blocking")});
            return JsonSchema::object(
                {
                    {QStringLiteral("targets"),                 JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("diagnostics"),             JsonSchema::array(diagnostic)            },
                    {QStringLiteral("track_ids"),               JsonSchema::array(identifierSchema())    },
                    {QStringLiteral("clip_ids"),                JsonSchema::array(identifierSchema())    },
                    {QStringLiteral("include_tempo"),           JsonSchema::boolean()                    },
                    {QStringLiteral("include_time_signatures"), JsonSchema::boolean()                    },
                    {QStringLiteral("include_lyrics"),          JsonSchema::boolean()                    },
                    {QStringLiteral("plan_digest"),             digestSchema()                           },
            },
                {QStringLiteral("targets"), QStringLiteral("diagnostics"),
                 QStringLiteral("track_ids"), QStringLiteral("clip_ids"),
                 QStringLiteral("include_tempo"), QStringLiteral("include_time_signatures"),
                 QStringLiteral("include_lyrics"), QStringLiteral("plan_digest")});
        }

        QJsonObject inferenceStatusSchema() {
            const auto stage = JsonSchema::object(
                {
                    {QStringLiteral("stage"),
                     stringDomainSchema(PublicValueDomain::InferenceStage)                  },
                    {QStringLiteral("state"),
                     JsonSchema::string({QStringLiteral("idle"), QStringLiteral("stale"),
                                         QStringLiteral("queued"), QStringLiteral("running"),
                                         QStringLiteral("ready"), QStringLiteral("failed")})},
                    {QStringLiteral("reason"),  JsonSchema::string()                        },
                    {QStringLiteral("task_id"),
                     JsonSchema::oneOf(QJsonArray{uuidSchema(), JsonSchema::null()})        },
            },
                {QStringLiteral("stage"), QStringLiteral("state"), QStringLiteral("reason"),
                 QStringLiteral("task_id")});
            return JsonSchema::object(
                {
                    {QStringLiteral("scope"),  inferenceScopeSchema()  },
                    {QStringLiteral("stages"), JsonSchema::array(stage)},
            },
                {QStringLiteral("scope"), QStringLiteral("stages")});
        }

        QJsonObject queryOutputSchema(const QString &id) {
            if (id == PublicToolNames::application_get_info) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("name"),     nonEmptyStringSchema()},
                        {QStringLiteral("version"),  nonEmptyStringSchema()},
                        {QStringLiteral("platform"), nonEmptyStringSchema()},
                        {QStringLiteral("build_id"), nonEmptyStringSchema()},
                },
                    {QStringLiteral("name"), QStringLiteral("version"), QStringLiteral("platform"),
                     QStringLiteral("build_id")}));
            }
            if (id == PublicToolNames::automation_get_status)
                return statusOutputSchema();
            if (id == PublicToolNames::automation_get_manifest)
                return manifestOutputSchema();
            if (id == PublicToolNames::automation_get_options)
                return optionsOutputSchema();
            if (id == PublicToolNames::automation_get_file_access)
                return fileAccessOutputSchema();
            if (id == PublicToolNames::documents_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), documentSnapshotSchema());
            if (id == PublicToolNames::project_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), projectSnapshotSchema());
            if (id == PublicToolNames::formats_list)
                return formatsOutputSchema();
            if (id == PublicToolNames::formats_inspect)
                return formatInspectionOutputSchema();
            if (id == PublicToolNames::tracks_list) {
                return queryEnvelopeSchema(QStringLiteral("tracks"),
                                           JsonSchema::array(trackSnapshotSchema()), true);
            }
            if (id == PublicToolNames::tracks_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), trackSnapshotSchema());
            if (id == PublicToolNames::tracks_get_voice_context ||
                id == PublicToolNames::clips_get_voice_context) {
                return queryEnvelopeSchema(QStringLiteral("snapshot"),
                                           voiceContextSnapshotSchema());
            }
            if (id == PublicToolNames::master_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), masterSnapshotSchema());
            if (id == PublicToolNames::clips_list) {
                return queryEnvelopeSchema(QStringLiteral("clips"),
                                           JsonSchema::array(clipSnapshotSchema()), true);
            }
            if (id == PublicToolNames::clips_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), clipSnapshotSchema());
            if (id == PublicToolNames::audio_clips_get) {
                return queryEnvelopeSchema(QStringLiteral("snapshot"), audioClipSnapshotSchema());
            }
            if (id == PublicToolNames::speaker_mix_get) {
                return queryEnvelopeSchema(QStringLiteral("snapshot"), speakerMixSnapshotSchema());
            }
            if (id == PublicToolNames::notes_get) {
                return queryEnvelopeSchema(QStringLiteral("notes"),
                                           JsonSchema::array(noteSnapshotSchema()), true);
            }
            if (id == PublicToolNames::notes_search)
                return noteSearchOutputSchema();
            if (id == PublicToolNames::parameters_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), parameterSnapshotSchema());
            if (id == PublicToolNames::parameters_get_capabilities) {
                return queryEnvelopeSchema(QStringLiteral("capabilities"),
                                           parameterCapabilitiesSchema());
            }
            if (id == PublicToolNames::timeline_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), timelineSnapshotSchema());
            if (id == PublicToolNames::history_get_state)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), historySnapshotSchema());
            if (id == PublicToolNames::voices_list) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("singers"),     JsonSchema::array(voiceSummarySchema())},
                        {QStringLiteral("next_cursor"), JsonSchema::string()                   },
                },
                    {QStringLiteral("singers")}));
            }
            if (id == PublicToolNames::voices_describe) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("snapshot"), voiceSnapshotSchema()}
                },
                    {QStringLiteral("snapshot")}));
            }
            if (id == PublicToolNames::exports_midi_get_capabilities) {
                return midiExportCapabilitiesOutputSchema();
            }
            if (id == PublicToolNames::exports_midi_preview) {
                return queryEnvelopeSchema(QStringLiteral("plan"), midiExportPlanSchema());
            }
            if (id == PublicToolNames::exports_audio_get_capabilities) {
                return queryEnvelopeSchema(QStringLiteral("capabilities"),
                                           audioExportCapabilitiesSchema());
            }
            if (id == PublicToolNames::exports_audio_preview) {
                return queryEnvelopeSchema(QStringLiteral("plan"), exportPlanSchema());
            }
            if (id == PublicToolNames::extract_get_capabilities) {
                return extractionCapabilitiesOutputSchema();
            }
            if (id == PublicToolNames::inference_get_capabilities) {
                return queryEnvelopeSchema(QStringLiteral("capabilities"),
                                           inferenceCapabilitiesSchema());
            }
            if (id == PublicToolNames::inference_get_status) {
                return queryEnvelopeSchema(QStringLiteral("status"), inferenceStatusSchema());
            }
            if (id == PublicToolNames::tasks_list) {
                return queryEnvelopeSchema(QStringLiteral("tasks"),
                                           JsonSchema::array(taskSnapshotObjectSchema()), true);
            }
            if (id == PublicToolNames::tasks_get || id == PublicToolNames::tasks_cancel)
                return taskSnapshotSchema();
            if (id == PublicToolNames::playback_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), playbackSnapshotSchema());
            qFatal("No explicit public query output schema for operation '%s'", qPrintable(id));
            return {};
        }

        QJsonObject outputSchema(const QString &id, const OperationKind kind,
                                 const SyncMode syncMode) {
            if (syncMode == SyncMode::Asynchronous)
                return taskAcceptedSchema(id == PublicToolNames::documents_open);
            if (kind == OperationKind::Command) {
                if (id == PublicToolNames::documents_new)
                    return documentLifecycleResultSchema();
                if (isPersistentPlaybackOperation(id))
                    return persistentPlaybackMutationSchema();
                if (id.startsWith(QStringLiteral("playback.")))
                    return playbackMutationSchema();
                return id == PublicToolNames::tasks_cancel ? taskSnapshotSchema()
                                                           : mutationSchema();
            }
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

        QString toolDescription(const QString &operationId, const QString &category,
                                const OperationKind kind, const SyncMode syncMode) {
            if (operationId == PublicToolNames::application_get_info) {
                return QStringLiteral("Read the editor product name, version, platform, and build "
                                      "identifier without changing editor state.");
            }
            if (operationId == PublicToolNames::automation_get_status) {
                return QStringLiteral(
                    "Read the active editor instance, host, access profile, Manifest "
                    "summary, and stable document/window identities.");
            }
            if (operationId == PublicToolNames::automation_get_manifest) {
                return QStringLiteral(
                    "Page through the effective public automation contracts, including "
                    "per-tool versions, schemas, policies, availability, and digests.");
            }
            if (operationId == PublicToolNames::automation_get_options) {
                return QStringLiteral(
                    "Resolve legal values for one dynamic field of an exposed target "
                    "operation using the supplied partial arguments and editor context.");
            }
            if (operationId == PublicToolNames::automation_get_file_access) {
                return QStringLiteral(
                    "Read the canonical automation read/write roots and temporary file "
                    "grants enforced by the editor File Guard.");
            }

            const auto action = humanTitle(operationId).toLower();
            if (kind == OperationKind::Query) {
                return QStringLiteral(
                           "Read %1 from the authoritative %2 domain using explicit stable "
                           "identifiers. This is read-only, never infers targets from GUI focus, "
                           "and does not modify History.")
                    .arg(action, category);
            }
            if (isPersistentPlaybackOperation(operationId)) {
                return QStringLiteral(
                           "Apply the GUI-equivalent %1 loop edit, commit it as one atomic History "
                           "entry, and return both the resulting document revision and playback "
                           "state version.")
                    .arg(action);
            }
            if (operationId.startsWith(QStringLiteral("playback."))) {
                return QStringLiteral(
                           "Apply the GUI-equivalent %1 playback transition and return the "
                           "new state version. It does not change document revision or History.")
                    .arg(action);
            }
            if (operationId.startsWith(QStringLiteral("history."))) {
                return QStringLiteral(
                           "Perform the GUI-equivalent %1 navigation as one History step and "
                           "return the resulting document revision and affected objects.")
                    .arg(action);
            }
            if (operationId == PublicToolNames::tasks_cancel) {
                return QStringLiteral(
                    "Request cancellation of an explicit editor task and return its "
                    "current or final snapshot without creating a History entry.");
            }
            if (operationId == PublicToolNames::notes_reset_phoneme_offsets) {
                return QStringLiteral(
                    "Reset the selected word roots to their inferred phoneme positions and "
                    "atomically reset any edited right neighbors required to avoid overlap. "
                    "The command is non-interactive and never opens a confirmation dialog.");
            }
            if (operationId == PublicToolNames::documents_new ||
                operationId == PublicToolNames::documents_open) {
                return QStringLiteral(
                           "Run the GUI-equivalent %1 lifecycle flow without modal prompts. "
                           "The unsaved-document policy and defaults are explicit, and the "
                           "result reports the replacement document identity.")
                    .arg(action);
            }
            if (operationId == PublicToolNames::documents_save ||
                operationId == PublicToolNames::documents_save_as) {
                return QStringLiteral(
                           "Run the GUI-equivalent %1 flow with explicit overwrite and path "
                           "policy, update the save point, and return the resulting revision.")
                    .arg(action);
            }
            if (operationId == PublicToolNames::exports_midi_start ||
                operationId == PublicToolNames::exports_audio_start) {
                return QStringLiteral(
                           "Start the GUI-equivalent %1 export with explicit target and "
                           "overwrite policy. The asynchronous task writes reported artifacts "
                           "without changing document revision or History.")
                    .arg(action);
            }
            if (syncMode == SyncMode::Asynchronous) {
                return QStringLiteral(
                           "Start the GUI-equivalent %1 workflow without modal prompts. File "
                           "policy and defaults are explicit; acceptance returns a task, and "
                           "the final domain commit is performed atomically.")
                    .arg(action);
            }
            return QStringLiteral(
                       "Apply the GUI-equivalent %1 action through the shared %2 domain facade. "
                       "The editor resolves GUI defaults, validates the expected revision, and "
                       "commits the request as one atomic History entry.")
                .arg(action, category);
        }

        QJsonObject valueSource(const QString &fieldPath, const QString &sourceOperation,
                                const QStringList &contextFields = {}) {
            QJsonArray context;
            for (const auto &field : contextFields)
                context.append(field);
            const auto sourceProfile =
                sourceOperation == PublicToolNames::voices_list ||
                        sourceOperation == PublicToolNames::voices_describe ||
                        sourceOperation == PublicToolNames::parameters_get_capabilities
                    ? AutomationProfile::L1
                    : AutomationProfile::L2;
            return {
                {QStringLiteral("field_path"),        fieldPath                           },
                {QStringLiteral("operation_id"),      sourceOperation                     },
                {QStringLiteral("context_fields"),    context                             },
                {QStringLiteral("minimum_profile"),   automationProfileName(sourceProfile)},
                {QStringLiteral("host_availability"), QStringLiteral("gui")               },
            };
        }

        QJsonArray valueSources(const QString &id) {
            QJsonArray result;
            const auto add = [&](const QString &fieldPath, const QString &sourceOperation,
                                 const QStringList &contextFields = {}) {
                result.append(valueSource(fieldPath, sourceOperation, contextFields));
            };

            if (id == PublicToolNames::voices_describe)
                add(QStringLiteral("/singer"), PublicToolNames::voices_list);
            if (id == PublicToolNames::tracks_set_default_language) {
                add(QStringLiteral("/language_id"), PublicToolNames::voices_describe,
                    {QStringLiteral("/document_id"), QStringLiteral("/track_id")});
            }
            if (id == PublicToolNames::tracks_set_voice) {
                add(QStringLiteral("/voice/singer"), PublicToolNames::voices_list);
                add(QStringLiteral("/voice/speaker"), PublicToolNames::voices_describe,
                    {QStringLiteral("/voice/singer")});
            }
            if (id == PublicToolNames::clips_set_default_language) {
                add(QStringLiteral("/language_id"), PublicToolNames::voices_describe,
                    {QStringLiteral("/document_id"), QStringLiteral("/clip_id")});
            }
            if (id == PublicToolNames::clips_set_voice) {
                add(QStringLiteral("/voice/singer"), PublicToolNames::voices_list);
                add(QStringLiteral("/voice/speaker"), PublicToolNames::voices_describe,
                    {QStringLiteral("/voice/singer")});
            }
            if (id == PublicToolNames::notes_set_language) {
                add(QStringLiteral("/language/language_id"), PublicToolNames::voices_describe,
                    {QStringLiteral("/document_id"), QStringLiteral("/clip_id")});
            }
            if (id == PublicToolNames::notes_fill_lyrics) {
                add(QStringLiteral("/options/language/language_id"),
                    PublicToolNames::voices_describe,
                    {QStringLiteral("/document_id"), QStringLiteral("/clip_id")});
            }
            if (id.startsWith(QStringLiteral("parameters.")) &&
                id != PublicToolNames::parameters_get &&
                id != PublicToolNames::parameters_get_capabilities) {
                add(QStringLiteral("/name"), PublicToolNames::parameters_get_capabilities,
                    {QStringLiteral("/document_id"), QStringLiteral("/clip_id")});
                if (id != PublicToolNames::parameters_bake) {
                    add(QStringLiteral("/layer"), PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name")});
                }
                if (id == PublicToolNames::parameters_replace) {
                    add(QStringLiteral("/curves/*/type"),
                        PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                    add(QStringLiteral("/curves/*/nodes/*/interpolation"),
                        PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                    add(QStringLiteral("/curves/*/values/*"),
                        PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                    add(QStringLiteral("/curves/*/nodes/*/value"),
                        PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                }
                if (id == PublicToolNames::parameters_draw) {
                    add(QStringLiteral("/values/*"), PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                }
                if (id == PublicToolNames::parameters_insert_anchors) {
                    add(QStringLiteral("/anchors/*/value"),
                        PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                    add(QStringLiteral("/anchors/*/interpolation"),
                        PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                }
                if (id == PublicToolNames::parameters_move_anchors) {
                    add(QStringLiteral("/moves/*/value"),
                        PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                }
                if (id == PublicToolNames::parameters_set_anchor_interpolation) {
                    add(QStringLiteral("/interpolation"),
                        PublicToolNames::parameters_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/clip_id"),
                         QStringLiteral("/name"), QStringLiteral("/layer")});
                }
            }
            if (id == PublicToolNames::speaker_mix_set_fixed) {
                add(QStringLiteral("/mix/singer"), PublicToolNames::voices_list);
                add(QStringLiteral("/mix/sources/*/speaker"), PublicToolNames::voices_describe,
                    {QStringLiteral("/mix/singer")});
            }
            if (id == PublicToolNames::exports_audio_preview ||
                id == PublicToolNames::exports_audio_start) {
                for (const auto &field :
                     {QStringLiteral("format"), QStringLiteral("sample_rate"),
                      QStringLiteral("channel_mode"), QStringLiteral("mixing_mode"),
                      QStringLiteral("source"), QStringLiteral("source_ids")}) {
                    add(QStringLiteral("/options/%1").arg(field),
                        PublicToolNames::exports_audio_get_capabilities,
                        {QStringLiteral("/document_id")});
                }
            }
            if (id == PublicToolNames::extract_pitch_start ||
                id == PublicToolNames::extract_midi_start) {
                add(QStringLiteral("/options/model_id"), PublicToolNames::extract_get_capabilities,
                    {QStringLiteral("/document_id"), QStringLiteral("/source_audio_clip_id")});
                if (id == PublicToolNames::extract_midi_start) {
                    add(QStringLiteral("/options/default_language"),
                        PublicToolNames::extract_get_capabilities,
                        {QStringLiteral("/document_id"), QStringLiteral("/source_audio_clip_id")});
                }
            }
            if (id == PublicToolNames::inference_start ||
                id == PublicToolNames::inference_reset_stage) {
                add(id == PublicToolNames::inference_start ? QStringLiteral("/stages/*")
                                                           : QStringLiteral("/stage"),
                    PublicToolNames::inference_get_capabilities,
                    {QStringLiteral("/document_id"), QStringLiteral("/scope")});
                if (id == PublicToolNames::inference_start) {
                    for (const auto &field :
                         {QStringLiteral("provider_id"), QStringLiteral("device_id"),
                          QStringLiteral("model_id")}) {
                        add(QStringLiteral("/options/%1").arg(field),
                            PublicToolNames::inference_get_capabilities,
                            {QStringLiteral("/document_id"), QStringLiteral("/scope")});
                    }
                }
            }
            return result;
        }

        QString documentPolicy(const ToolContract &tool) {
            if (tool.operationId == PublicToolNames::documents_new ||
                tool.operationId == PublicToolNames::documents_open) {
                return QStringLiteral("replace");
            }
            if (documentWriteOperations().contains(tool.operationId))
                return QStringLiteral("write");
            if (documentQueryOperations().contains(tool.operationId) ||
                (tool.operationId.startsWith(QStringLiteral("playback.")) &&
                 !isPersistentPlaybackOperation(tool.operationId))) {
                return QStringLiteral("read");
            }
            return QStringLiteral("none");
        }

        QString revisionPolicy(const ToolContract &tool) {
            if (tool.operationId == PublicToolNames::documents_open ||
                tool.operationId == PublicToolNames::exports_midi_start ||
                tool.operationId == PublicToolNames::exports_audio_start) {
                return QStringLiteral("check_and_revalidate");
            }
            if (tool.operationId == PublicToolNames::documents_new)
                return QStringLiteral("check");
            if (documentWriteOperations().contains(tool.operationId))
                return tool.syncMode == SyncMode::Asynchronous
                           ? QStringLiteral("check_and_revalidate")
                           : QStringLiteral("check");
            return QStringLiteral("none");
        }

        QString historyPolicy(const ToolContract &tool) {
            if (tool.kind == OperationKind::Query ||
                tool.operationId == PublicToolNames::tasks_cancel ||
                tool.operationId == PublicToolNames::documents_new ||
                tool.operationId == PublicToolNames::documents_open ||
                tool.operationId == PublicToolNames::exports_midi_start ||
                tool.operationId == PublicToolNames::exports_audio_start ||
                tool.operationId == PublicToolNames::playback_play ||
                tool.operationId == PublicToolNames::playback_pause ||
                tool.operationId == PublicToolNames::playback_stop ||
                tool.operationId == PublicToolNames::playback_seek) {
                return QStringLiteral("none");
            }
            if (tool.operationId == PublicToolNames::documents_save ||
                tool.operationId == PublicToolNames::documents_save_as) {
                return QStringLiteral("savepoint");
            }
            if (tool.operationId == PublicToolNames::history_undo ||
                tool.operationId == PublicToolNames::history_redo) {
                return QStringLiteral("navigate");
            }
            return tool.syncMode == SyncMode::Asynchronous ? QStringLiteral("commit_once")
                                                           : QStringLiteral("single_entry");
        }

        QString fileAccess(const QString &id) {
            if (id == PublicToolNames::documents_open || id == PublicToolNames::documents_import ||
                id == PublicToolNames::documents_import_batch ||
                id == PublicToolNames::formats_inspect ||
                id == PublicToolNames::audio_clips_import ||
                id == PublicToolNames::audio_clips_import_batch ||
                id == PublicToolNames::audio_clips_relocate ||
                id == PublicToolNames::audio_clips_confirm_path ||
                id.startsWith(QStringLiteral("extract."))) {
                return QStringLiteral("read");
            }
            if (id == PublicToolNames::documents_save || id == PublicToolNames::documents_save_as ||
                id == PublicToolNames::exports_midi_preview ||
                id == PublicToolNames::exports_midi_start ||
                id == PublicToolNames::exports_audio_preview ||
                id == PublicToolNames::exports_audio_start) {
                return QStringLiteral("write");
            }
            return QStringLiteral("none");
        }

        QString concurrencyScope(const ToolContract &tool) {
            if (tool.operationId.startsWith(QStringLiteral("playback.")) &&
                !isPersistentPlaybackOperation(tool.operationId)) {
                return QStringLiteral("playback");
            }
            if (tool.operationId.startsWith(QStringLiteral("tasks.")))
                return QStringLiteral("task");
            if (tool.operationId.startsWith(QStringLiteral("exports.")))
                return QStringLiteral("export");
            if (tool.minimumProfile == AutomationProfile::Meta ||
                tool.operationId.startsWith(QStringLiteral("automation.")) ||
                tool.operationId.startsWith(QStringLiteral("voices.")) ||
                tool.operationId.startsWith(QStringLiteral("formats."))) {
                return QStringLiteral("application");
            }
            return QStringLiteral("document");
        }

        QString conflictPolicy(const ToolContract &tool) {
            if (tool.kind == OperationKind::Query)
                return QStringLiteral("none");
            if (tool.operationId == PublicToolNames::tasks_cancel)
                return QStringLiteral("task_state");
            if (tool.operationId == PublicToolNames::exports_midi_start ||
                tool.operationId == PublicToolNames::exports_audio_start) {
                return QStringLiteral("snapshot");
            }
            if (tool.operationId == PublicToolNames::documents_new ||
                tool.operationId == PublicToolNames::documents_open) {
                return QStringLiteral("replacement");
            }
            if (isPersistentPlaybackOperation(tool.operationId))
                return QStringLiteral("revision_and_state_version");
            if (tool.operationId.startsWith(QStringLiteral("playback."))) {
                return QStringLiteral("state_version");
            }
            return tool.syncMode == SyncMode::Asynchronous
                       ? QStringLiteral("revision_and_generation")
                       : QStringLiteral("revision");
        }

        QJsonObject toolAnnotations(const QString &id, const OperationKind kind) {
            static const QSet<QString> destructive{
                PublicToolNames::tracks_remove,
                PublicToolNames::clips_remove,
                PublicToolNames::notes_remove,
                PublicToolNames::parameters_erase,
                PublicToolNames::parameters_remove_anchors,
                PublicToolNames::tempos_delete,
                PublicToolNames::time_signatures_delete,
                PublicToolNames::documents_new,
                PublicToolNames::documents_open,
                PublicToolNames::inference_reset_stage,
            };
            return {
                {QStringLiteral("title"),           humanTitle(id)              },
                {QStringLiteral("readOnlyHint"),    kind == OperationKind::Query},
                {QStringLiteral("destructiveHint"), destructive.contains(id)    },
                {QStringLiteral("idempotentHint"),  kind == OperationKind::Query},
                {QStringLiteral("openWorldHint"),   false                       },
            };
        }

        QJsonObject manifestContent(const PublicManifest &manifest) {
            QJsonArray operations;
            for (const auto &tool : manifest.operations)
                operations.append(tool.toManifestJson());
            QJsonObject result{
                {QStringLiteral("toolset_version"), static_cast<qint64>(manifest.toolsetVersion)},
                {QStringLiteral("profile"),         automationProfileName(manifest.profile)     },
                {QStringLiteral("host_mode"),       manifest.hostMode                           },
                {QStringLiteral("operations"),      operations                                  },
                {QStringLiteral("extensions"),      manifest.extensions                         },
            };
            result.insert(QStringLiteral("digest"), manifest.digest);
            if (!manifest.nextCursor.isEmpty())
                result.insert(QStringLiteral("next_cursor"), manifest.nextCursor);
            return result;
        }

        QJsonObject manifestDigestContent(const PublicManifest &manifest) {
            static const QHash<QString, QJsonObject> operationCache = [] {
                QHash<QString, QJsonObject> result;
                for (const auto &tool : publicToolContracts()) {
                    auto operation = tool.toManifestJson();
                    operation.remove(QStringLiteral("input_schema"));
                    operation.remove(QStringLiteral("output_schema"));
                    result.insert(tool.operationId, operation);
                }
                return result;
            }();
            QJsonArray operations;
            for (const auto &tool : manifest.operations)
                operations.append(operationCache.value(tool.operationId));
            return {
                {QStringLiteral("toolset_version"), static_cast<qint64>(manifest.toolsetVersion)},
                {QStringLiteral("profile"),         automationProfileName(manifest.profile)     },
                {QStringLiteral("host_mode"),       manifest.hostMode                           },
                {QStringLiteral("operations"),      operations                                  },
                {QStringLiteral("extensions"),      manifest.extensions                         },
            };
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
            {QStringLiteral("name"),         operationId },
            {QStringLiteral("title"),        title       },
            {QStringLiteral("description"),  description },
            {QStringLiteral("inputSchema"),  inputSchema },
            {QStringLiteral("outputSchema"), outputSchema},
            {QStringLiteral("annotations"),  annotations },
            {QStringLiteral("_meta"),
             QJsonObject{
                 {QStringLiteral("io.openvpi.ds-editor-lite/tool"),
                  QJsonObject{
                      {QStringLiteral("version"), static_cast<qint64>(version)},
                      {QStringLiteral("introduced_version"),
                       static_cast<qint64>(introducedVersion)},
                      {QStringLiteral("minimum_compatible_version"),
                       static_cast<qint64>(minimumCompatibleVersion)},
                      {QStringLiteral("category"), category},
                      {QStringLiteral("minimum_profile"), automationProfileName(minimumProfile)},
                      {QStringLiteral("kind"), operationKindName(kind)},
                      {QStringLiteral("sync_mode"), syncModeName(syncMode)},
                      {QStringLiteral("host_availability"), QStringLiteral("gui")},
                      {QStringLiteral("value_sources"), valueSources},
                  }},
             }                                           },
        };
    }

    QJsonObject ToolContract::toManifestJson() const {
        const auto inputDigest = sha256Digest(inputSchema);
        const auto outputDigest = sha256Digest(outputSchema);
        return {
            {QStringLiteral("operation_id"),               operationId                           },
            {QStringLiteral("title"),                      title                                 },
            {QStringLiteral("description"),                description                           },
            {QStringLiteral("version"),                    static_cast<qint64>(version)          },
            {QStringLiteral("minimum_compatible_version"),
             static_cast<qint64>(minimumCompatibleVersion)                                       },
            {QStringLiteral("category"),                   category                              },
            {QStringLiteral("kind"),                       operationKindName(kind)               },
            {QStringLiteral("input_schema"),               inputSchema                           },
            {QStringLiteral("output_schema"),              outputSchema                          },
            {QStringLiteral("value_sources"),              valueSources                          },
            {QStringLiteral("minimum_profile"),            automationProfileName(minimumProfile) },
            {QStringLiteral("sync_mode"),                  syncModeName(syncMode)                },
            {QStringLiteral("document_policy"),            documentPolicy(*this)                 },
            {QStringLiteral("revision_policy"),            revisionPolicy(*this)                 },
            {QStringLiteral("history_policy"),             historyPolicy(*this)                  },
            {QStringLiteral("file_access"),                fileAccess(operationId)               },
            {QStringLiteral("host_availability"),          QStringLiteral("gui")                 },
            {QStringLiteral("concurrency_scope"),          concurrencyScope(*this)               },
            {QStringLiteral("conflict_policy"),            conflictPolicy(*this)                 },
            {QStringLiteral("safety_metadata"),
             QJsonObject{
                 {QStringLiteral("read_only"), kind == OperationKind::Query},
                 {QStringLiteral("destructive"),
                  annotations.value(QStringLiteral("destructiveHint")).toBool()},
                 {QStringLiteral("idempotent"),
                  annotations.value(QStringLiteral("idempotentHint")).toBool()},
                 {QStringLiteral("open_world"),
                  annotations.value(QStringLiteral("openWorldHint")).toBool()},
             }                                                                                   },
            {QStringLiteral("introduced_version"),         static_cast<qint64>(introducedVersion)},
            {QStringLiteral("schema_digest"),
             QJsonObject{{QStringLiteral("input"), inputDigest},
                         {QStringLiteral("output"), outputDigest}}                               },
        };
    }

    const QList<ToolContract> &publicToolContracts() {
        static const QList<ToolContract> tools = [] {
            QList<ToolContract> result;
            result.reserve(128);
#define AUTOMATION_WIRE_PUBLIC_TOOL(symbol, name, categoryValue, profile, kindValue, syncValue,    \
                                    versionValue, introducedValue, minimumValue)                   \
    do {                                                                                           \
        const QString operationId(PublicToolNames::symbol);                                        \
        const auto operationKind = OperationKind::kindValue;                                       \
        const auto operationSync = SyncMode::syncValue;                                            \
        const auto title = humanTitle(operationId);                                                \
        result.append({                                                                            \
            .operationId = operationId,                                                            \
            .version = versionValue,                                                               \
            .introducedVersion = introducedValue,                                                  \
            .minimumCompatibleVersion = minimumValue,                                              \
            .title = title,                                                                        \
            .description = toolDescription(operationId, QStringLiteral(categoryValue),             \
                                           operationKind, operationSync),                          \
            .category = QStringLiteral(categoryValue),                                             \
            .minimumProfile = AutomationProfile::profile,                                          \
            .kind = operationKind,                                                                 \
            .syncMode = operationSync,                                                             \
            .inputSchema = inputSchema(operationId, operationKind),                                \
            .outputSchema = outputSchema(operationId, operationKind, operationSync),               \
            .valueSources = valueSources(operationId),                                             \
            .annotations = toolAnnotations(operationId, operationKind),                            \
        });                                                                                        \
    } while (false);
#include "PublicToolDefinitions.inc"
#undef AUTOMATION_WIRE_PUBLIC_TOOL
            const auto options = std::find_if(result.begin(), result.end(), [](const auto &tool) {
                return tool.operationId == PublicToolNames::automation_get_options;
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
        return manifestContent(*this);
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
        manifest.digest = sha256Digest(manifestDigestContent(digestManifest));
        return manifest;
    }

}
