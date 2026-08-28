#include "PublicToolContract.h"

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
        enum class ToolEffect {
            ReadOnly,
            NonDestructive,
            Destructive,
        };

        enum class ToolRepeatability {
            Idempotent,
            NonIdempotent,
        };

        enum class ToolWorldAccess {
            ClosedWorld,
            OpenWorld,
        };

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

        QJsonObject omissionEquivalentStringSchema() {
            return JsonSchema::string();
        }

        QJsonObject omissionEquivalentStringSchema(QStringList values) {
            values.prepend(QString());
            return JsonSchema::string(values);
        }

        QJsonObject omissionEquivalentStringDomainSchema(const PublicValueDomain domain) {
            return omissionEquivalentStringSchema(publicStringValueDomainValues(domain));
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
                    {QStringLiteral("package_id"),      nonEmptyStringSchema()},
                    {QStringLiteral("package_version"), nonEmptyStringSchema()},
                    {QStringLiteral("singer_id"),       nonEmptyStringSchema()},
            },
                {QStringLiteral("package_id"), QStringLiteral("package_version"),
                 QStringLiteral("singer_id")});
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

        QJsonObject speakerMixPresetDraftSchema() {
            const auto source = JsonSchema::object(
                {
                    {QStringLiteral("speaker"), speakerRefSchema()},
                    {QStringLiteral("weight"),
                     JsonSchema::number(MinimumMixWeight, MaximumMixWeight)},
            },
                {QStringLiteral("speaker"), QStringLiteral("weight")});
            return JsonSchema::object(
                {
                    {QStringLiteral("preset_id"), nonEmptyStringSchema()},
                    {QStringLiteral("name"), nonEmptyStringSchema()},
                    {QStringLiteral("singer"), singerRefSchema()},
                    {QStringLiteral("sources"),
                     JsonSchema::array(source, 2, MaximumCommandCollectionItems)},
            },
                {QStringLiteral("name"), QStringLiteral("singer"), QStringLiteral("sources")});
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
        QJsonObject l3InputSchema(const QString &id);
        QJsonObject l3OutputSchema(const QString &id);
        QJsonObject packageRefreshResultSchema();

        bool isL3Operation(const QString &id) {
            return id.startsWith(QStringLiteral("workspace.")) ||
                   id.startsWith(QStringLiteral("track_panel.")) ||
                   id.startsWith(QStringLiteral("clip_editor.")) ||
                   id.startsWith(QStringLiteral("settings.")) ||
                   id.startsWith(QStringLiteral("packages.")) ||
                   id.startsWith(QStringLiteral("lyric_rules."));
        }

        bool isL3GuiOperation(const QString &id) {
            return id.startsWith(QStringLiteral("workspace.")) ||
                   id.startsWith(QStringLiteral("track_panel.")) ||
                   id.startsWith(QStringLiteral("clip_editor."));
        }

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
                    {QStringLiteral("model_id"), omissionEquivalentStringSchema()},
                });
            }
            return JsonSchema::object({
                {QStringLiteral("model_id"), omissionEquivalentStringSchema()},
                {QStringLiteral("default_language"), omissionEquivalentStringSchema()},
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
                {QStringLiteral("encoding"),               omissionEquivalentStringSchema()},
                {QStringLiteral("import_tempo"),           JsonSchema::boolean()           },
                {QStringLiteral("import_time_signatures"), JsonSchema::boolean()           },
            });
        }

        QJsonObject midiExportOptionsSchema() {
            return JsonSchema::object({
                {QStringLiteral("include_tempo"), JsonSchema::boolean()},
                {QStringLiteral("include_time_signatures"), JsonSchema::boolean()},
                {QStringLiteral("include_lyrics"), JsonSchema::boolean()},
                {QStringLiteral("track_ids"),
                 JsonSchema::array(identifierSchema(), 0, MaximumCommandCollectionItems)},
                {QStringLiteral("clip_ids"),
                 JsonSchema::array(identifierSchema(), 0, MaximumCommandCollectionItems)},
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
                name == QStringLiteral("target_curve_id") ||
                name == QStringLiteral("source_curve_id") || name == QStringLiteral("anchor_id") ||
                name == QStringLiteral("source_clip_id") ||
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
            if (name == QStringLiteral("max_points"))
                return JsonSchema::integer(1.0, MaximumCurveSampleItems);
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
            if (name == QStringLiteral("idempotency_key") || name == QStringLiteral("cursor") ||
                (id == PublicToolNames::voices_list &&
                 (name == QStringLiteral("query") || name == QStringLiteral("package_id"))) ||
                ((id == PublicToolNames::documents_open ||
                  id == PublicToolNames::documents_import) &&
                 (name == QStringLiteral("format_id") || name == QStringLiteral("plan_digest"))) ||
                (id == PublicToolNames::audio_clips_confirm_path &&
                 name == QStringLiteral("path"))) {
                return omissionEquivalentStringSchema();
            }
            if (name == QStringLiteral("operation_id") || name == QStringLiteral("field_path") ||
                name == QStringLiteral("query") || name == QStringLiteral("package_id") ||
                name == QStringLiteral("path") || name == QStringLiteral("name") ||
                name == QStringLiteral("client_ref") || name == QStringLiteral("preset_id") ||
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
                           : omissionEquivalentStringSchema({QStringLiteral("open"),
                                                             QStringLiteral("import"),
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
                return id == PublicToolNames::clips_list
                           ? omissionEquivalentStringDomainSchema(PublicValueDomain::ClipType)
                           : stringDomainSchema(PublicValueDomain::ClipType);
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
                    stringDomainSchema(PublicValueDomain::InferenceStage), 0,
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
            if (name == QStringLiteral("preset"))
                return speakerMixPresetDraftSchema();
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
                return JsonSchema::array(
                    anchor, id == PublicToolNames::parameters_create_anchor_curve ? 2 : 1,
                    MaximumCommandCollectionItems);
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
                        {QStringLiteral("path"),        nonEmptyStringSchema()          },
                        {QStringLiteral("format_id"),   omissionEquivalentStringSchema()},
                        {QStringLiteral("options"),     formatOptionsSchema()           },
                        {QStringLiteral("plan_digest"), omissionEquivalentStringSchema()},
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
                        {QStringLiteral("provider_id"), omissionEquivalentStringSchema()},
                        {QStringLiteral("device_id"),   omissionEquivalentStringSchema()},
                        {QStringLiteral("model_id"),    omissionEquivalentStringSchema()},
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
                PublicToolNames::tracks_list,
                PublicToolNames::tracks_get,
                PublicToolNames::master_get,
                PublicToolNames::clips_list,
                PublicToolNames::clips_get,
                PublicToolNames::audio_clips_get,
                PublicToolNames::speaker_mix_get,
                PublicToolNames::notes_list,
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
                PublicToolNames::speaker_mix_presets_apply,
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
                PublicToolNames::parameters_create_anchor_curve,
                PublicToolNames::parameters_insert_anchors,
                PublicToolNames::parameters_merge_anchor_curves,
                PublicToolNames::parameters_move_anchors,
                PublicToolNames::parameters_remove_anchors,
                PublicToolNames::parameters_set_anchor_interpolation,
                PublicToolNames::tempos_set,
                PublicToolNames::tempos_remove,
                PublicToolNames::time_signatures_set,
                PublicToolNames::time_signatures_remove,
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

        const QSet<QString> &documentValidateOnlyOperations() {
            static const QSet<QString> ids{
                PublicToolNames::documents_save_as,
                PublicToolNames::documents_import_batch,
                PublicToolNames::audio_clips_import_batch,
            };
            return ids;
        }

        const QSet<QString> &documentIdempotencyKeyOperations() {
            static const QSet<QString> ids{
                PublicToolNames::tracks_insert,
                PublicToolNames::clips_insert,
                PublicToolNames::clips_duplicate,
                PublicToolNames::audio_clips_import,
                PublicToolNames::audio_clips_import_batch,
                PublicToolNames::speaker_mix_keyframes_insert,
                PublicToolNames::notes_insert,
                PublicToolNames::notes_duplicate,
                PublicToolNames::notes_split_at,
                PublicToolNames::parameters_create_anchor_curve,
                PublicToolNames::parameters_insert_anchors,
                PublicToolNames::extract_pitch_start,
                PublicToolNames::extract_midi_start,
            };
            return ids;
        }

        QHash<QString, QStringList> requiredInputFields() {
            return {
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
                {PublicToolNames::speaker_mix_presets_save,            {QStringLiteral("preset")}                       },
                {PublicToolNames::speaker_mix_presets_delete,          {QStringLiteral("preset_id")}                    },
                {PublicToolNames::speaker_mix_presets_apply,
                 {QStringLiteral("target"), QStringLiteral("preset_id")}                                                },
                {PublicToolNames::notes_list,                          {QStringLiteral("clip_id")}                      },
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
                {PublicToolNames::parameters_create_anchor_curve,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("client_ref"), QStringLiteral("anchors")}                                              },
                {PublicToolNames::parameters_insert_anchors,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("curve_id"), QStringLiteral("anchors")}                                                },
                {PublicToolNames::parameters_merge_anchor_curves,
                 {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                  QStringLiteral("target_curve_id"), QStringLiteral("source_curve_id")}                                 },
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
                {PublicToolNames::tempos_remove,                       {QStringLiteral("tick")}                         },
                {PublicToolNames::time_signatures_set,
                 {QStringLiteral("bar_index"), QStringLiteral("numerator"),
                  QStringLiteral("denominator")}                                                                        },
                {PublicToolNames::time_signatures_remove,              {QStringLiteral("bar_index")}                    },
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
                {PublicToolNames::speaker_mix_presets_list,     {QStringLiteral("singer")}                         },
                {PublicToolNames::speaker_mix_keyframes_insert, {QStringLiteral("weights")}                        },
                {PublicToolNames::notes_list,                   {QStringLiteral("cursor"), QStringLiteral("limit")}},
                {PublicToolNames::notes_search,
                 {QStringLiteral("case_sensitive"), QStringLiteral("regex")}                                       },
                {PublicToolNames::notes_fill_lyrics,            {QStringLiteral("options")}                        },
                {PublicToolNames::parameters_get,
                 {QStringLiteral("range"), QStringLiteral("max_points")}                                           },
                {PublicToolNames::parameters_draw,              {QStringLiteral("merge_mode")}                     },
                {PublicToolNames::parameters_bake,
                 {QStringLiteral("local_start"), QStringLiteral("local_end")}                                      },
                {PublicToolNames::inference_start,              {QStringLiteral("stages")}                         },
                {PublicToolNames::tasks_list,
                 {QStringLiteral("state"), QStringLiteral("kind"), QStringLiteral("cursor"),
                  QStringLiteral("limit")}                                                                         },
            };
        }

        QJsonObject tasksInputSchema(const QString &id) {
            QJsonObject common{
                {QStringLiteral("scope"),
                 JsonSchema::string({QStringLiteral("document"), QStringLiteral("application")})},
            };
            QStringList required{QStringLiteral("scope")};
            if (id == PublicToolNames::tasks_get || id == PublicToolNames::tasks_cancel) {
                common.insert(QStringLiteral("task_id"), uuidSchema());
                required.append(QStringLiteral("task_id"));
            } else {
                common.insert(QStringLiteral("state"),
                              omissionEquivalentStringDomainSchema(PublicValueDomain::TaskState));
                common.insert(QStringLiteral("kind"),
                              omissionEquivalentStringDomainSchema(PublicValueDomain::TaskKind));
                common.insert(QStringLiteral("cursor"), omissionEquivalentStringSchema());
                common.insert(QStringLiteral("limit"),
                              JsonSchema::integer(MinimumPageSize, MaximumPageSize));
            }

            auto documentProperties = common;
            documentProperties.insert(QStringLiteral("scope"),
                                      JsonSchema::constant(QStringLiteral("document")));
            documentProperties.insert(QStringLiteral("document_id"), uuidSchema());
            auto documentRequired = required;
            documentRequired.append(QStringLiteral("document_id"));

            auto applicationProperties = common;
            applicationProperties.insert(QStringLiteral("scope"),
                                         JsonSchema::constant(QStringLiteral("application")));
            auto rootProperties = common;
            rootProperties.insert(QStringLiteral("document_id"), uuidSchema());
            auto root = JsonSchema::object(rootProperties);
            root.insert(QStringLiteral("oneOf"),
                        QJsonArray{
                            JsonSchema::object(documentProperties, documentRequired),
                            JsonSchema::object(applicationProperties, required),
                        });
            return JsonSchema::document(root);
        }

        QJsonObject authoritativeInputSchema(const QString &id, const OperationKind kind) {
            if (id.startsWith(QStringLiteral("tasks.")))
                return tasksInputSchema(id);
            if (isL3Operation(id))
                return l3InputSchema(id);
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
            } else if (documentQueryOperations().contains(id) ||
                       documentWriteOperations().contains(id) || playbackWrite) {
                add(QStringLiteral("document_id"), true);
            }
            if (documentWriteOperations().contains(id)) {
                add(QStringLiteral("expected_revision"), true);
            }
            if (documentIdempotencyKeyOperations().contains(id)) {
                add(QStringLiteral("idempotency_key"), false);
            }
            if (playbackWrite)
                add(QStringLiteral("expected_state_version"), false);
            if (documentValidateOnlyOperations().contains(id))
                add(QStringLiteral("validate_only"), false);
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

        QJsonObject taskAcceptedSchema(const bool nullableDocument = false,
                                       const bool applicationScope = false) {
            if (applicationScope) {
                const auto accepted = JsonSchema::object(
                    {
                        {QStringLiteral("task_id"),        uuidSchema()               },
                        {QStringLiteral("scope"),
                         JsonSchema::constant(QStringLiteral("application"))          },
                        {QStringLiteral("document"),       JsonSchema::null()         },
                        {QStringLiteral("validated_only"), JsonSchema::constant(false)},
                },
                    {QStringLiteral("task_id"), QStringLiteral("scope"), QStringLiteral("document"),
                     QStringLiteral("validated_only")});
                const auto validated = JsonSchema::object(
                    {
                        {QStringLiteral("scope"),
                         JsonSchema::constant(QStringLiteral("application"))         },
                        {QStringLiteral("document"),       JsonSchema::null()        },
                        {QStringLiteral("validated_only"), JsonSchema::constant(true)},
                },
                    {QStringLiteral("scope"), QStringLiteral("document"),
                     QStringLiteral("validated_only")});
                return JsonSchema::document(JsonSchema::oneOf(QJsonArray{accepted, validated}));
            }
            const auto acceptedDocument =
                nullableDocument
                    ? JsonSchema::oneOf(QJsonArray{documentVersionSchema(), JsonSchema::null()})
                    : documentVersionSchema();
            const auto accepted = JsonSchema::object(
                {
                    {QStringLiteral("task_id"),        uuidSchema()                                    },
                    {QStringLiteral("scope"),          JsonSchema::constant(QStringLiteral("document"))},
                    {QStringLiteral("document"),       acceptedDocument                                },
                    {QStringLiteral("validated_only"), JsonSchema::constant(false)                     },
            },
                {QStringLiteral("task_id"), QStringLiteral("scope"), QStringLiteral("document"),
                 QStringLiteral("validated_only")});
            const auto validated = JsonSchema::object(
                {
                    {QStringLiteral("scope"),          JsonSchema::constant(QStringLiteral("document"))},
                    {QStringLiteral("document"),
                     JsonSchema::oneOf(QJsonArray{documentVersionSchema(), JsonSchema::null()})        },
                    {QStringLiteral("validated_only"), JsonSchema::constant(true)                      },
            },
                {QStringLiteral("scope"), QStringLiteral("document"),
                 QStringLiteral("validated_only")});
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
                if (operation == PublicToolNames::packages_refresh)
                    continue;
                const bool opensDocument = operation == PublicToolNames::documents_open;
                const auto document =
                    opensDocument
                        ? JsonSchema::oneOf(QJsonArray{documentVersionSchema(), JsonSchema::null()})
                        : documentVersionSchema();
                branches.append(JsonSchema::object(
                    {
                        {QStringLiteral("task_id"),              uuidSchema()                                    },
                        {QStringLiteral("operation_id"),         JsonSchema::constant(operation)                 },
                        {QStringLiteral("scope"),                JsonSchema::constant(QStringLiteral("document"))},
                        {QStringLiteral("document"),             document                                        },
                        {QStringLiteral("state"),                stringDomainSchema(PublicValueDomain::TaskState)},
                        {QStringLiteral("progress"),             progress                                        },
                        {QStringLiteral("message"),              JsonSchema::string()                            },
                        {QStringLiteral("result"),               mutationObjectSchema(opensDocument)             },
                        {QStringLiteral("error"),                taskError                                       },
                        {QStringLiteral("created_by_client_id"), JsonSchema::string()                            },
                        {QStringLiteral("cancelable"),           JsonSchema::boolean()                           },
                        {QStringLiteral("validated_only"),       JsonSchema::boolean()                           },
                },
                    {QStringLiteral("task_id"), QStringLiteral("operation_id"),
                     QStringLiteral("scope"), QStringLiteral("document"), QStringLiteral("state"),
                     QStringLiteral("progress")}));
            }
            branches.append(JsonSchema::object(
                {
                    {QStringLiteral("task_id"),              uuidSchema()                                       },
                    {QStringLiteral("operation_id"),
                     JsonSchema::constant(PublicToolNames::packages_refresh)                                    },
                    {QStringLiteral("scope"),                JsonSchema::constant(QStringLiteral("application"))},
                    {QStringLiteral("document"),             JsonSchema::null()                                 },
                    {QStringLiteral("state"),                stringDomainSchema(PublicValueDomain::TaskState)   },
                    {QStringLiteral("progress"),             progress                                           },
                    {QStringLiteral("message"),              JsonSchema::string()                               },
                    {QStringLiteral("application_result"),   packageRefreshResultSchema()                       },
                    {QStringLiteral("error"),                taskError                                          },
                    {QStringLiteral("created_by_client_id"), JsonSchema::string()                               },
                    {QStringLiteral("cancelable"),           JsonSchema::boolean()                              },
                    {QStringLiteral("validated_only"),       JsonSchema::boolean()                              },
            },
                {QStringLiteral("task_id"), QStringLiteral("operation_id"), QStringLiteral("scope"),
                 QStringLiteral("document"), QStringLiteral("state"), QStringLiteral("progress")}));
            return JsonSchema::oneOf(branches);
        }

        QJsonObject taskSnapshotSchema() {
            return JsonSchema::document(taskSnapshotObjectSchema());
        }

        QJsonObject tasksListOutputSchema() {
            const auto tasks = JsonSchema::array(taskSnapshotObjectSchema());
            return JsonSchema::document(JsonSchema::oneOf(QJsonArray{
                JsonSchema::object(
                    {
                              {QStringLiteral("scope"), JsonSchema::constant(QStringLiteral("document"))},
                              {QStringLiteral("document"), documentVersionSchema()},
                              {QStringLiteral("tasks"), tasks},
                              {QStringLiteral("next_cursor"), JsonSchema::string()},
                              },
                    {QStringLiteral("scope"),                                                                           QStringLiteral("document"),                                                                                     QStringLiteral("tasks")                                                     }
                    ),
                JsonSchema::object(
                    {
                              {QStringLiteral("scope"),
                         JsonSchema::constant(QStringLiteral("application"))},
                              {QStringLiteral("document"), JsonSchema::null()},
                              {QStringLiteral("tasks"), tasks},
                              {QStringLiteral("next_cursor"), JsonSchema::string()},
                              },
                    {QStringLiteral("scope"),                                            QStringLiteral("document"), QStringLiteral("tasks")                                              }
                    ),
            }));
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
                    {QStringLiteral("editor_instance_id"), uuidSchema()},
                    {QStringLiteral("host_mode"), hostModeSchema()},
                    {QStringLiteral("profile"), automationProfileSchema()},
                    {QStringLiteral("toolset_version"),
                     JsonSchema::integer(1.0, static_cast<double>(MaximumSafeJsonInteger))},
                    {QStringLiteral("documents"), documents},
                    {QStringLiteral("windows"), windows},
            },
                {QStringLiteral("editor_instance_id"), QStringLiteral("host_mode"),
                 QStringLiteral("profile"), QStringLiteral("toolset_version"),
                 QStringLiteral("documents"), QStringLiteral("windows")}));
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
            const auto statistics = JsonSchema::object(
                {
                    {QStringLiteral("length_ticks"),             JsonSchema::integer(0.0)},
                    {QStringLiteral("track_count"),              JsonSchema::integer(0.0)},
                    {QStringLiteral("empty_track_count"),        JsonSchema::integer(0.0)},
                    {QStringLiteral("singing_only_track_count"), JsonSchema::integer(0.0)},
                    {QStringLiteral("audio_only_track_count"),   JsonSchema::integer(0.0)},
                    {QStringLiteral("mixed_track_count"),        JsonSchema::integer(0.0)},
                    {QStringLiteral("clip_count"),               JsonSchema::integer(0.0)},
                    {QStringLiteral("singing_clip_count"),       JsonSchema::integer(0.0)},
                    {QStringLiteral("audio_clip_count"),         JsonSchema::integer(0.0)},
            },
                {QStringLiteral("length_ticks"), QStringLiteral("track_count"),
                 QStringLiteral("empty_track_count"), QStringLiteral("singing_only_track_count"),
                 QStringLiteral("audio_only_track_count"), QStringLiteral("mixed_track_count"),
                 QStringLiteral("clip_count"), QStringLiteral("singing_clip_count"),
                 QStringLiteral("audio_clip_count")});
            return JsonSchema::object(
                {
                    {QStringLiteral("path"),          JsonSchema::string()                         },
                    {QStringLiteral("project_name"),  JsonSchema::string()                         },
                    {QStringLiteral("busy"),          JsonSchema::boolean()                        },
                    {QStringLiteral("saved"),         JsonSchema::boolean()                        },
                    {QStringLiteral("dirty"),         JsonSchema::boolean()                        },
                    {QStringLiteral("on_save_point"), JsonSchema::boolean()                        },
                    {QStringLiteral("lifecycle"),     JsonSchema::string(documentLifecycleValues())},
                    {QStringLiteral("statistics"),    statistics                                   },
            },
                {QStringLiteral("path"), QStringLiteral("project_name"), QStringLiteral("busy"),
                 QStringLiteral("saved"), QStringLiteral("dirty"), QStringLiteral("on_save_point"),
                 QStringLiteral("lifecycle"), QStringLiteral("statistics")});
        }

        QJsonObject recentDocumentsOutputSchema() {
            const auto project = JsonSchema::object(
                {
                    {QStringLiteral("path"),      nonEmptyStringSchema()},
                    {QStringLiteral("file_name"), JsonSchema::string()  },
                    {QStringLiteral("exists"),    JsonSchema::boolean() },
            },
                {QStringLiteral("path"), QStringLiteral("file_name"), QStringLiteral("exists")});
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("projects"), JsonSchema::array(project)}
            },
                {QStringLiteral("projects")}));
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

        QJsonObject parameterForegroundNameSchema() {
            auto values = publicStringValueDomainValues(PublicValueDomain::ParameterName);
            values.removeAll(QStringLiteral("pitch"));
            return JsonSchema::string(values);
        }

        QJsonObject parameterBackgroundNameSchema() {
            auto values = publicStringValueDomainValues(PublicValueDomain::ParameterName);
            values.removeAll(QStringLiteral("pitch"));
            values.removeAll(QStringLiteral("speaker_mix"));
            return JsonSchema::string(values);
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
            const auto range = JsonSchema::object(
                {
                    {QStringLiteral("start"), JsonSchema::integer(0.0)},
                    {QStringLiteral("end"),   JsonSchema::integer(0.0)},
            },
                {QStringLiteral("start"), QStringLiteral("end")});
            return JsonSchema::object(
                {
                    {QStringLiteral("clip_id"),              identifierSchema()                      },
                    {QStringLiteral("name"),                 parameterNameSchema()                   },
                    {QStringLiteral("layer"),                parameterLayerSchema()                  },
                    {QStringLiteral("curves"),               JsonSchema::array(curveSnapshotSchema())},
                    {QStringLiteral("range"),                range                                   },
                    {QStringLiteral("source_point_count"),   JsonSchema::integer(0.0)                },
                    {QStringLiteral("returned_point_count"), JsonSchema::integer(0.0)                },
                    {QStringLiteral("downsampled"),          JsonSchema::boolean()                   },
            },
                {QStringLiteral("clip_id"), QStringLiteral("name"), QStringLiteral("layer"),
                 QStringLiteral("curves"), QStringLiteral("source_point_count"),
                 QStringLiteral("returned_point_count"), QStringLiteral("downsampled")});
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
                    {QStringLiteral("package_id"),      nonEmptyStringSchema()},
                    {QStringLiteral("package_version"), nonEmptyStringSchema()},
                    {QStringLiteral("singer_id"),       nonEmptyStringSchema()},
                    {QStringLiteral("name"),            nonEmptyStringSchema()},
            },
                {QStringLiteral("package_id"), QStringLiteral("package_version"),
                 QStringLiteral("singer_id"), QStringLiteral("name")});
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
                    {QStringLiteral("package_version"),    nonEmptyStringSchema()     },
                    {QStringLiteral("singer_id"),          nonEmptyStringSchema()     },
                    {QStringLiteral("name"),               nonEmptyStringSchema()     },
                    {QStringLiteral("speakers"),           JsonSchema::array(speaker) },
                    {QStringLiteral("languages"),          JsonSchema::array(language)},
                    {QStringLiteral("default_speaker_id"), optionalIdentifier         },
                    {QStringLiteral("default_language"),   optionalIdentifier         },
                    {QStringLiteral("g2p_ready"),          JsonSchema::boolean()      },
                    {QStringLiteral("resolution_state"),
                     stringDomainSchema(PublicValueDomain::VoiceResolutionState)      },
                    {QStringLiteral("mixing_supported"),   JsonSchema::boolean()      },
            },
                {QStringLiteral("package_id"), QStringLiteral("package_version"),
                 QStringLiteral("singer_id"), QStringLiteral("name"), QStringLiteral("speakers"),
                 QStringLiteral("languages"), QStringLiteral("default_speaker_id"),
                 QStringLiteral("default_language"), QStringLiteral("g2p_ready"),
                 QStringLiteral("resolution_state"), QStringLiteral("mixing_supported")});
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

        QJsonObject voiceContextSnapshotSchema();

        QJsonObject trackSnapshotSchema(const bool includeVoiceContext = false) {
            QJsonObject properties{
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
            };
            QStringList required{QStringLiteral("track_id"),
                                 QStringLiteral("index"),
                                 QStringLiteral("name"),
                                 QStringLiteral("color_index"),
                                 QStringLiteral("gain"),
                                 QStringLiteral("pan"),
                                 QStringLiteral("mute"),
                                 QStringLiteral("solo"),
                                 QStringLiteral("default_language_id"),
                                 QStringLiteral("clip_count")};
            if (includeVoiceContext) {
                properties.insert(QStringLiteral("voice_context"), voiceContextSnapshotSchema());
                required.append(QStringLiteral("voice_context"));
            }
            return JsonSchema::object(properties, required);
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

        QJsonObject clipSnapshotSchema(const bool includeVoiceContext = false) {
            QJsonObject properties{
                {QStringLiteral("clip_id"),             identifierSchema()                             },
                {QStringLiteral("track_id"),            identifierSchema()                             },
                {QStringLiteral("type"),                stringDomainSchema(PublicValueDomain::ClipType)},
                {QStringLiteral("name"),                JsonSchema::string()                           },
                {QStringLiteral("start"),               JsonSchema::integer(0.0)                       },
                {QStringLiteral("length"),              JsonSchema::integer(1.0)                       },
                {QStringLiteral("gain"),                JsonSchema::number()                           },
                {QStringLiteral("mute"),                JsonSchema::boolean()                          },
                {QStringLiteral("default_language_id"), JsonSchema::string()                           },
            };
            QStringList required{QStringLiteral("clip_id"),
                                 QStringLiteral("track_id"),
                                 QStringLiteral("type"),
                                 QStringLiteral("name"),
                                 QStringLiteral("start"),
                                 QStringLiteral("length"),
                                 QStringLiteral("gain"),
                                 QStringLiteral("mute"),
                                 QStringLiteral("default_language_id")};
            if (includeVoiceContext) {
                properties.insert(QStringLiteral("voice_context"),
                                  JsonSchema::oneOf(QJsonArray{voiceContextSnapshotSchema(),
                                                               JsonSchema::null()}));
                required.append(QStringLiteral("voice_context"));
            }
            return JsonSchema::object(properties, required);
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
            const auto sourcePreset = JsonSchema::object(
                {
                    {QStringLiteral("preset_id"), nonEmptyStringSchema()},
                    {QStringLiteral("name"),      JsonSchema::string()  },
                    {QStringLiteral("dirty"),     JsonSchema::boolean() },
            },
                {QStringLiteral("preset_id"), QStringLiteral("name"), QStringLiteral("dirty")});
            return JsonSchema::object(
                {
                    {QStringLiteral("target"),          speakerMixTargetSchema()    },
                    {QStringLiteral("mix"),             speakerMixDraftSchema()     },
                    {QStringLiteral("dynamic_enabled"), JsonSchema::boolean()       },
                    {QStringLiteral("bypassed"),        JsonSchema::boolean()       },
                    {QStringLiteral("keyframes"),       JsonSchema::array(keyframe) },
                    {QStringLiteral("source_preset"),
                     JsonSchema::oneOf(QJsonArray{sourcePreset, JsonSchema::null()})},
            },
                {QStringLiteral("target"), QStringLiteral("mix"), QStringLiteral("dynamic_enabled"),
                 QStringLiteral("bypassed"), QStringLiteral("keyframes"),
                 QStringLiteral("source_preset")});
        }

        QJsonObject speakerMixPresetSnapshotSchema() {
            const auto source = JsonSchema::object(
                {
                    {QStringLiteral("speaker"), speakerRefSchema()},
                    {QStringLiteral("speaker_name"), JsonSchema::string()},
                    {QStringLiteral("weight"),
                     JsonSchema::number(MinimumMixWeight, MaximumMixWeight)},
            },
                {QStringLiteral("speaker"), QStringLiteral("speaker_name"),
                 QStringLiteral("weight")});
            return JsonSchema::object(
                {
                    {QStringLiteral("preset_id"), JsonSchema::string()},
                    {QStringLiteral("name"), nonEmptyStringSchema()},
                    {QStringLiteral("singer"), singerRefSchema()},
                    {QStringLiteral("sources"), JsonSchema::array(source, 2)},
                    {QStringLiteral("created_at"), JsonSchema::string()},
                    {QStringLiteral("updated_at"), JsonSchema::string()},
            },
                {QStringLiteral("preset_id"), QStringLiteral("name"), QStringLiteral("singer"),
                 QStringLiteral("sources"), QStringLiteral("created_at"),
                 QStringLiteral("updated_at")});
        }

        QJsonObject speakerMixPresetsOutputSchema() {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("presets"),
                     JsonSchema::array(speakerMixPresetSnapshotSchema())},
            },
                {QStringLiteral("presets")}));
        }

        QJsonObject speakerMixPresetSaveOutputSchema() {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("preset"),         speakerMixPresetSnapshotSchema()},
                    {QStringLiteral("changed"),        JsonSchema::boolean()           },
                    {QStringLiteral("validated_only"), JsonSchema::boolean()           },
            },
                {QStringLiteral("preset"), QStringLiteral("changed"),
                 QStringLiteral("validated_only")}));
        }

        QJsonObject applicationMutationSchema() {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("changed"),        JsonSchema::boolean()},
                    {QStringLiteral("validated_only"), JsonSchema::boolean()},
            },
                {QStringLiteral("changed"), QStringLiteral("validated_only")}));
        }

        QJsonObject nullableSchema(const QJsonObject &schema) {
            return JsonSchema::oneOf(QJsonArray{schema, JsonSchema::null()});
        }

        QJsonObject l3DocumentInput(QJsonObject properties, QStringList required,
                                    const bool expectedRevision = false,
                                    const int minimumProperties = -1) {
            properties.insert(QStringLiteral("window_id"), nonEmptyStringSchema());
            properties.insert(QStringLiteral("document_id"), uuidSchema());
            required.prepend(QStringLiteral("document_id"));
            required.prepend(QStringLiteral("window_id"));
            if (expectedRevision) {
                properties.insert(QStringLiteral("expected_revision"), revisionSchema());
                required.append(QStringLiteral("expected_revision"));
            }
            auto root = JsonSchema::object(properties, required);
            if (minimumProperties >= 0)
                root.insert(QStringLiteral("minProperties"), minimumProperties);
            return JsonSchema::document(root);
        }

        QJsonObject l3ApplicationUpdateInput(QJsonObject properties,
                                             const int minimumProperties = 1) {
            auto updateOnly = JsonSchema::object(properties);
            updateOnly.insert(QStringLiteral("minProperties"), minimumProperties);
            return JsonSchema::document(updateOnly);
        }

        QJsonObject l3ApplicationValidatableUpdateInput(QJsonObject properties,
                                                        const int minimumProperties = 1) {
            auto updateOnly = JsonSchema::object(properties);
            updateOnly.insert(QStringLiteral("minProperties"), minimumProperties);
            properties.insert(QStringLiteral("validate_only"), JsonSchema::boolean());
            auto root = JsonSchema::object(properties);
            root.insert(QStringLiteral("minProperties"), minimumProperties);
            auto preview = JsonSchema::object(properties, {QStringLiteral("validate_only")});
            preview.insert(QStringLiteral("minProperties"), minimumProperties + 1);
            root.insert(QStringLiteral("oneOf"), QJsonArray{updateOnly, preview});
            return JsonSchema::document(root);
        }

        QJsonObject defaultLyricsInputSchema() {
            return JsonSchema::array(
                JsonSchema::object(
                    {
                        {QStringLiteral("language_id"), nonEmptyStringSchema()},
                        {QStringLiteral("lyric"),       JsonSchema::string()  },
            },
                    {QStringLiteral("language_id"), QStringLiteral("lyric")}),
                0, MaximumCommandCollectionItems);
        }

        QJsonObject taggerEntrySchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("type"),
                     JsonSchema::string({QStringLiteral("regex"), QStringLiteral("array"),
                                         QStringLiteral("dict")})},
                    {QStringLiteral("value"),
                     JsonSchema::array(nonEmptyStringSchema(), 1, MaximumCommandCollectionItems)},
                    {QStringLiteral("tag"), nonEmptyStringSchema()},
                    {QStringLiteral("discard"), JsonSchema::boolean()},
            },
                {QStringLiteral("type"), QStringLiteral("value"), QStringLiteral("tag"),
                 QStringLiteral("discard")});
        }

        QJsonObject lyricRuleDraftSchema(const QString &kind) {
            if (kind == QStringLiteral("splitter")) {
                return JsonSchema::object(
                    {
                        {QStringLiteral("kind"), JsonSchema::constant(QStringLiteral("splitter"))},
                        {QStringLiteral("name"), nonEmptyStringSchema()},
                        {QStringLiteral("regexes"),
                         JsonSchema::array(nonEmptyStringSchema(), 1,
                         MaximumCommandCollectionItems)},
                        {QStringLiteral("enabled"), JsonSchema::boolean()},
                        {QStringLiteral("position"), JsonSchema::integer(0.0)},
                        {QStringLiteral("validate_only"), JsonSchema::boolean()},
                },
                    {QStringLiteral("kind"), QStringLiteral("name"), QStringLiteral("regexes")});
            }
            return JsonSchema::object(
                {
                    {QStringLiteral("kind"), JsonSchema::constant(QStringLiteral("tagger"))},
                    {QStringLiteral("name"), nonEmptyStringSchema()},
                    {QStringLiteral("language"), nonEmptyStringSchema()},
                    {QStringLiteral("entries"),
                     JsonSchema::array(taggerEntrySchema(), 1, MaximumCommandCollectionItems)},
                    {QStringLiteral("enabled"), JsonSchema::boolean()},
                    {QStringLiteral("position"), JsonSchema::integer(0.0)},
                    {QStringLiteral("validate_only"), JsonSchema::boolean()},
            },
                {QStringLiteral("kind"), QStringLiteral("name"), QStringLiteral("language"),
                 QStringLiteral("entries")});
        }

        QJsonObject l3InputSchema(const QString &id) {
            if (id == PublicToolNames::workspace_get) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("window_id"), nonEmptyStringSchema()}
                },
                    {QStringLiteral("window_id")}));
            }
            if (id == PublicToolNames::workspace_set_panel_visibility) {
                auto root = JsonSchema::object(
                    {
                        {QStringLiteral("window_id"),           nonEmptyStringSchema()},
                        {QStringLiteral("track_panel_visible"), JsonSchema::boolean() },
                        {QStringLiteral("clip_editor_visible"), JsonSchema::boolean() },
                },
                    {QStringLiteral("window_id")});
                root.insert(QStringLiteral("minProperties"), 2);
                return JsonSchema::document(root);
            }

            if (id == PublicToolNames::track_panel_get)
                return l3DocumentInput({}, {});
            if (id == PublicToolNames::track_panel_set_viewport) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("center_tick"),
                         JsonSchema::number(0.0, static_cast<double>(MaximumSafeJsonInteger))},
                        {QStringLiteral("center_track_index"), JsonSchema::number(0.0)},
                        {QStringLiteral("horizontal_scale"), JsonSchema::number(0.000001)},
                        {QStringLiteral("vertical_scale"), JsonSchema::number(0.000001)},
                },
                    {}, false, 3);
            }
            if (id == PublicToolNames::track_panel_reveal_clips) {
                QJsonObject common{
                    {QStringLiteral("window_id"),         nonEmptyStringSchema()},
                    {QStringLiteral("document_id"),       uuidSchema()          },
                    {QStringLiteral("expected_revision"), revisionSchema()      },
                };
                auto track = common;
                track.insert(QStringLiteral("track_id"), identifierSchema());
                auto clips = common;
                clips.insert(
                    QStringLiteral("clip_ids"),
                    JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems));
                auto root = JsonSchema::oneOf(QJsonArray{
                    JsonSchema::object(
                        track, {QStringLiteral("window_id"), QStringLiteral("document_id"),
                                QStringLiteral("expected_revision"), QStringLiteral("track_id")}),
                    JsonSchema::object(
                        clips, {QStringLiteral("window_id"), QStringLiteral("document_id"),
                                QStringLiteral("expected_revision"), QStringLiteral("clip_ids")}),
                });
                root.insert(QStringLiteral("type"), QStringLiteral("object"));
                return JsonSchema::document(root);
            }
            if (id == PublicToolNames::track_panel_set_auto_page_turn) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("enabled"), JsonSchema::boolean()}
                },
                    {QStringLiteral("enabled")});
            }
            if (id == PublicToolNames::track_panel_select_track) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("track_id"), nullableSchema(identifierSchema())}
                },
                    {QStringLiteral("track_id")}, true);
            }
            if (id == PublicToolNames::track_panel_select_clips) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("clip_ids"),
                         JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems)},
                        {QStringLiteral("primary_clip_id"), nullableSchema(identifierSchema())},
                },
                    {QStringLiteral("clip_ids")}, true);
            }
            if (id == PublicToolNames::track_panel_clear_selection) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("target"),
                         JsonSchema::string({QStringLiteral("track"), QStringLiteral("clips"),
                                             QStringLiteral("all")})}
                },
                    {QStringLiteral("target")}, true);
            }

            if (id == PublicToolNames::clip_editor_get)
                return l3DocumentInput({}, {});
            if (id == PublicToolNames::clip_editor_set_active_clip) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("clip_id"), nullableSchema(identifierSchema())}
                },
                    {QStringLiteral("clip_id")}, true);
            }
            if (id == PublicToolNames::clip_editor_set_time_viewport) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("center_tick"),
                         JsonSchema::number(0.0, static_cast<double>(MaximumSafeJsonInteger))},
                        {QStringLiteral("horizontal_scale"), JsonSchema::number(0.000001)},
                },
                    {}, false, 3);
            }
            if (id == PublicToolNames::clip_editor_set_auto_page_turn) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("enabled"), JsonSchema::boolean()}
                },
                    {QStringLiteral("enabled")});
            }
            if (id == PublicToolNames::clip_editor_show_region) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("region"),
                         JsonSchema::string(
                             {QStringLiteral("piano"), QStringLiteral("parameters")})}
                },
                    {QStringLiteral("region")});
            }
            if (id == PublicToolNames::clip_editor_piano_set_pitch_viewport) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("center_key_index"),
                         JsonSchema::number(MinimumMidiKeyIndex, MaximumMidiKeyIndex)},
                        {QStringLiteral("vertical_scale"), JsonSchema::number(0.000001)},
                },
                    {}, false, 3);
            }
            if (id == PublicToolNames::clip_editor_piano_reveal_notes) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("note_ids"),
                         JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems)}
                },
                    {QStringLiteral("note_ids")}, true);
            }
            if (id == PublicToolNames::clip_editor_piano_set_edit_mode) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("mode"),
                         JsonSchema::string(
                             {QStringLiteral("select"), QStringLiteral("interval_select"),
                              QStringLiteral("draw_note"), QStringLiteral("erase_note"),
                              QStringLiteral("split_note"), QStringLiteral("draw_pitch"),
                              QStringLiteral("edit_pitch_anchor"), QStringLiteral("erase_pitch"),
                              QStringLiteral("bake_pitch")})}
                },
                    {QStringLiteral("mode")});
            }
            if (id == PublicToolNames::clip_editor_piano_set_quantize) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("quantize"),
                         valueDomainSchema(PublicValueDomain::Quantize)   },
                        {QStringLiteral("enabled"),  JsonSchema::boolean()},
                },
                    {}, false, 3);
            }
            if (id == PublicToolNames::clip_editor_piano_select_notes) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("note_ids"),
                         JsonSchema::array(identifierSchema(), 1, MaximumCommandCollectionItems)},
                        {QStringLiteral("primary_note_id"), nullableSchema(identifierSchema())},
                },
                    {QStringLiteral("note_ids")}, true);
            }
            if (id == PublicToolNames::clip_editor_piano_clear_selection)
                return l3DocumentInput({}, {}, true);
            if (id == PublicToolNames::clip_editor_parameters_set_foreground) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("parameter"), parameterForegroundNameSchema()}
                },
                    {QStringLiteral("parameter")}, true);
            }
            if (id == PublicToolNames::clip_editor_parameters_set_background) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("parameter"),
                         nullableSchema(parameterBackgroundNameSchema())}
                },
                    {QStringLiteral("parameter")}, true);
            }
            if (id == PublicToolNames::clip_editor_parameters_swap)
                return l3DocumentInput({}, {}, true);
            if (id == PublicToolNames::clip_editor_parameters_set_tool) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("tool"),
                         JsonSchema::string({QStringLiteral("draw"), QStringLiteral("erase"),
                                             QStringLiteral("bake"), QStringLiteral("anchor")})}
                },
                    {QStringLiteral("tool")}, true);
            }
            if (id == PublicToolNames::clip_editor_parameters_set_value_viewport) {
                return l3DocumentInput(
                    {
                        {QStringLiteral("center_ratio"), JsonSchema::number(0.0, 1.0)},
                        {QStringLiteral("vertical_scale"), JsonSchema::number(1.0)},
                },
                    {}, true, 4);
            }

            if (id == PublicToolNames::settings_query) {
                auto domains = JsonSchema::array(
                    JsonSchema::string({QStringLiteral("ui_language"), QStringLiteral("singing"),
                                        QStringLiteral("theme"), QStringLiteral("audio_device"),
                                        QStringLiteral("playback_behavior"),
                                        QStringLiteral("compute_device"), QStringLiteral("render"),
                                        QStringLiteral("singer_session_retention"),
                                        QStringLiteral("package_search_paths")}),
                    0, 9);
                domains.insert(QStringLiteral("uniqueItems"), true);
                return JsonSchema::document(JsonSchema::object({
                    {QStringLiteral("domains"), domains}
                }));
            }
            if (id == PublicToolNames::settings_ui_language_update) {
                return l3ApplicationUpdateInput({
                    {QStringLiteral("ui_language"), nonEmptyStringSchema()}
                });
            }
            if (id == PublicToolNames::settings_singing_update) {
                return l3ApplicationUpdateInput({
                    {QStringLiteral("default_language"), nonEmptyStringSchema()    },
                    {QStringLiteral("default_lyrics"),   defaultLyricsInputSchema()},
                });
            }
            if (id == PublicToolNames::settings_theme_update) {
                return l3ApplicationUpdateInput({
                    {QStringLiteral("theme_id"), nonEmptyStringSchema()}
                });
            }
            if (id == PublicToolNames::settings_audio_device_update) {
                return l3ApplicationValidatableUpdateInput({
                    {QStringLiteral("driver_name"), nonEmptyStringSchema()},
                    {QStringLiteral("device_name"), JsonSchema::string()},
                    {QStringLiteral("buffer_size"), JsonSchema::integer(0.0)},
                    {QStringLiteral("sample_rate"), JsonSchema::integer(0.0)},
                    {QStringLiteral("hot_plug_notification_mode"), JsonSchema::integer(0.0, 2.0)},
                    {QStringLiteral("gain"), JsonSchema::number(0.0, MaximumAudioDeviceGain)},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
                });
            }
            if (id == PublicToolNames::settings_playback_behavior_update) {
                return l3ApplicationUpdateInput({
                    {QStringLiteral("behavior"), JsonSchema::integer(0.0, 2.0)}
                });
            }
            if (id == PublicToolNames::settings_compute_device_update) {
                return l3ApplicationValidatableUpdateInput({
                    {QStringLiteral("execution_provider"),
                     JsonSchema::string({QStringLiteral("CPU"), QStringLiteral("DirectML"),
                                         QStringLiteral("CUDA")})                                },
                    {QStringLiteral("gpu_index"),          JsonSchema::integer(-1.0)             },
                    {QStringLiteral("gpu_id"),             nullableSchema(nonEmptyStringSchema())},
                });
            }
            if (id == PublicToolNames::settings_render_update) {
                return l3ApplicationUpdateInput({
                    {QStringLiteral("sampling_steps"), JsonSchema::integer(1.0, 1000.0)},
                    {QStringLiteral("depth"), JsonSchema::number(0.0, 1.0)},
                    {QStringLiteral("run_vocoder_on_cpu"), JsonSchema::boolean()},
                    {QStringLiteral("auto_start_inference"), JsonSchema::boolean()},
                    {QStringLiteral("playback_lookahead_seconds"), JsonSchema::number(0.000001)},
                    {QStringLiteral("pitch_smooth_kernel_size"), JsonSchema::integer(0.0)},
                });
            }
            if (id == PublicToolNames::settings_singer_session_retention_update) {
                auto idleTimeout = JsonSchema::integer(0.0, 300.0);
                idleTimeout.insert(QStringLiteral("multipleOf"), 60);
                return l3ApplicationUpdateInput({
                    {QStringLiteral("capacity"), JsonSchema::integer(0.0, 8.0)},
                    {QStringLiteral("idle_timeout_seconds"), idleTimeout},
                });
            }
            if (id == PublicToolNames::settings_package_search_paths_update) {
                return l3ApplicationValidatableUpdateInput({
                    {QStringLiteral("paths"),
                     JsonSchema::array(nonEmptyStringSchema(), 0, MaximumCommandCollectionItems)}
                });
            }

            if (id == PublicToolNames::packages_list) {
                return JsonSchema::document(JsonSchema::object({
                    {QStringLiteral("query"), JsonSchema::string()},
                    {QStringLiteral("cursor"), omissionEquivalentStringSchema()},
                    {QStringLiteral("limit"),
                     JsonSchema::integer(MinimumPageSize, MaximumPageSize)},
                }));
            }
            if (id == PublicToolNames::packages_describe) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("package_id"), nonEmptyStringSchema()          },
                        {QStringLiteral("version"),    omissionEquivalentStringSchema()}
                },
                    {QStringLiteral("package_id")}));
            }
            if (id == PublicToolNames::packages_refresh) {
                return JsonSchema::document(JsonSchema::object({}));
            }

            if (id == PublicToolNames::lyric_rules_list) {
                return JsonSchema::document(JsonSchema::object({
                    {QStringLiteral("kind"),
                     omissionEquivalentStringSchema(
                         {QStringLiteral("splitter"), QStringLiteral("tagger")})},
                    {QStringLiteral("include_disabled"), JsonSchema::boolean()  },
                }));
            }
            if (id == PublicToolNames::lyric_rules_create) {
                auto root = JsonSchema::oneOf(QJsonArray{
                    lyricRuleDraftSchema(QStringLiteral("splitter")),
                    lyricRuleDraftSchema(QStringLiteral("tagger")),
                });
                root.insert(QStringLiteral("type"), QStringLiteral("object"));
                return JsonSchema::document(root);
            }
            if (id == PublicToolNames::lyric_rules_update) {
                auto root = JsonSchema::object(
                    {
                        {QStringLiteral("rule_id"), nonEmptyStringSchema()},
                        {QStringLiteral("name"), nonEmptyStringSchema()},
                        {QStringLiteral("language"), nonEmptyStringSchema()},
                        {QStringLiteral("regexes"),
                         JsonSchema::array(nonEmptyStringSchema(), 1,
                         MaximumCommandCollectionItems)},
                        {QStringLiteral("entries"),
                         JsonSchema::array(taggerEntrySchema(), 1, MaximumCommandCollectionItems)},
                        {QStringLiteral("validate_only"), JsonSchema::boolean()},
                },
                    {QStringLiteral("rule_id")});
                root.insert(QStringLiteral("minProperties"), 2);
                return JsonSchema::document(root);
            }
            if (id == PublicToolNames::lyric_rules_delete) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("rule_id"), nonEmptyStringSchema()},
                },
                    {QStringLiteral("rule_id")}));
            }
            if (id == PublicToolNames::lyric_rules_set_enabled) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("rule_id"), nonEmptyStringSchema()},
                        {QStringLiteral("enabled"), JsonSchema::boolean() },
                },
                    {QStringLiteral("rule_id"), QStringLiteral("enabled")}));
            }
            if (id == PublicToolNames::lyric_rules_move) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("rule_id"),  nonEmptyStringSchema()  },
                        {QStringLiteral("position"), JsonSchema::integer(0.0)},
                },
                    {QStringLiteral("rule_id"), QStringLiteral("position")}));
            }
            if (id == PublicToolNames::lyric_rules_test) {
                return JsonSchema::document(JsonSchema::object(
                    {
                        {QStringLiteral("text"), JsonSchema::string()}
                },
                    {QStringLiteral("text")}));
            }

            qFatal("No explicit L3 input schema for operation '%s'", qPrintable(id));
            return {};
        }

        QJsonObject pianoEditModeSchema() {
            return JsonSchema::string({
                QStringLiteral("select"),
                QStringLiteral("interval_select"),
                QStringLiteral("draw_note"),
                QStringLiteral("erase_note"),
                QStringLiteral("split_note"),
                QStringLiteral("draw_pitch"),
                QStringLiteral("edit_pitch_anchor"),
                QStringLiteral("erase_pitch"),
                QStringLiteral("bake_pitch"),
            });
        }

        QJsonObject parameterEditToolSchema() {
            return JsonSchema::string({QStringLiteral("draw"), QStringLiteral("erase"),
                                       QStringLiteral("bake"), QStringLiteral("anchor")});
        }

        QJsonObject trackViewportSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("center_tick"),        JsonSchema::number(0.0)     },
                    {QStringLiteral("center_track_index"), JsonSchema::number(0.0)     },
                    {QStringLiteral("horizontal_scale"),   JsonSchema::number(0.000001)},
                    {QStringLiteral("vertical_scale"),     JsonSchema::number(0.000001)},
            },
                {QStringLiteral("center_tick"), QStringLiteral("center_track_index"),
                 QStringLiteral("horizontal_scale"), QStringLiteral("vertical_scale")});
        }

        QJsonObject timeViewportSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("center_tick"),      JsonSchema::number(0.0)     },
                    {QStringLiteral("horizontal_scale"), JsonSchema::number(0.000001)},
            },
                {QStringLiteral("center_tick"), QStringLiteral("horizontal_scale")});
        }

        QJsonObject pitchViewportSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("center_key_index"),
                     JsonSchema::number(MinimumMidiKeyIndex, MaximumMidiKeyIndex)},
                    {QStringLiteral("vertical_scale"), JsonSchema::number(0.000001)},
            },
                {QStringLiteral("center_key_index"), QStringLiteral("vertical_scale")});
        }

        QJsonObject parameterValueViewportSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("center_ratio"), JsonSchema::number(0.0, 1.0)},
                    {QStringLiteral("vertical_scale"), JsonSchema::number(1.0)},
            },
                {QStringLiteral("center_ratio"), QStringLiteral("vertical_scale")});
        }

        QJsonObject workspaceSnapshotObjectSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("window_id"),           nonEmptyStringSchema()              },
                    {QStringLiteral("track_panel_visible"), JsonSchema::boolean()               },
                    {QStringLiteral("clip_editor_visible"), JsonSchema::boolean()               },
                    {QStringLiteral("active_panel"),
                     JsonSchema::string({QStringLiteral("track_panel"),
                                         QStringLiteral("clip_editor"), QStringLiteral("none")})},
                    {QStringLiteral("focused_region"),
                     JsonSchema::string({QStringLiteral("track_panel"), QStringLiteral("piano"),
                                         QStringLiteral("parameters"), QStringLiteral("none")}) },
            },
                {QStringLiteral("window_id"), QStringLiteral("track_panel_visible"),
                 QStringLiteral("clip_editor_visible"), QStringLiteral("active_panel"),
                 QStringLiteral("focused_region")});
        }

        QJsonObject trackPanelSnapshotObjectSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("window_id"), nonEmptyStringSchema()},
                    {QStringLiteral("document_id"), uuidSchema()},
                    {QStringLiteral("viewport"), trackViewportSchema()},
                    {QStringLiteral("auto_page_turn"), JsonSchema::boolean()},
                    {QStringLiteral("selected_track_id"), nullableSchema(identifierSchema())},
                    {QStringLiteral("selected_clip_ids"),
                     JsonSchema::array(identifierSchema(), 0, MaximumCommandCollectionItems)},
                    {QStringLiteral("primary_clip_id"), nullableSchema(identifierSchema())},
                    {QStringLiteral("focused"), JsonSchema::boolean()},
            },
                {QStringLiteral("window_id"), QStringLiteral("document_id"),
                 QStringLiteral("viewport"), QStringLiteral("auto_page_turn"),
                 QStringLiteral("selected_track_id"), QStringLiteral("selected_clip_ids"),
                 QStringLiteral("primary_clip_id"), QStringLiteral("focused")});
        }

        QJsonObject clipEditorSnapshotObjectSchema() {
            const auto piano = JsonSchema::object(
                {
                    {QStringLiteral("visible"), JsonSchema::boolean()},
                    {QStringLiteral("focused"), JsonSchema::boolean()},
                    {QStringLiteral("pitch_viewport"), pitchViewportSchema()},
                    {QStringLiteral("edit_mode"), pianoEditModeSchema()},
                    {QStringLiteral("quantize"), valueDomainSchema(PublicValueDomain::Quantize)},
                    {QStringLiteral("quantize_enabled"), JsonSchema::boolean()},
                    {QStringLiteral("selected_note_ids"),
                     JsonSchema::array(identifierSchema(), 0, MaximumCommandCollectionItems)},
                    {QStringLiteral("primary_note_id"), nullableSchema(identifierSchema())},
            },
                {QStringLiteral("visible"), QStringLiteral("focused"),
                 QStringLiteral("pitch_viewport"), QStringLiteral("edit_mode"),
                 QStringLiteral("quantize"), QStringLiteral("quantize_enabled"),
                 QStringLiteral("selected_note_ids"), QStringLiteral("primary_note_id")});
            const auto parameters = JsonSchema::object(
                {
                    {QStringLiteral("visible"),        JsonSchema::boolean()                },
                    {QStringLiteral("focused"),        JsonSchema::boolean()                },
                    {QStringLiteral("foreground"),     parameterNameSchema()                },
                    {QStringLiteral("background"),     nullableSchema(parameterNameSchema())},
                    {QStringLiteral("tool"),           parameterEditToolSchema()            },
                    {QStringLiteral("value_viewport"), parameterValueViewportSchema()       },
            },
                {QStringLiteral("visible"), QStringLiteral("focused"), QStringLiteral("foreground"),
                 QStringLiteral("background"), QStringLiteral("tool"),
                 QStringLiteral("value_viewport")});
            return JsonSchema::object(
                {
                    {QStringLiteral("window_id"),      nonEmptyStringSchema()            },
                    {QStringLiteral("document_id"),    uuidSchema()                      },
                    {QStringLiteral("visible"),        JsonSchema::boolean()             },
                    {QStringLiteral("active_clip_id"), nullableSchema(identifierSchema())},
                    {QStringLiteral("active_region"),
                     JsonSchema::string({QStringLiteral("piano"), QStringLiteral("parameters"),
                                         QStringLiteral("none")})                        },
                    {QStringLiteral("focused_region"),
                     JsonSchema::string({QStringLiteral("piano"), QStringLiteral("parameters"),
                                         QStringLiteral("none")})                        },
                    {QStringLiteral("time_viewport"),  timeViewportSchema()              },
                    {QStringLiteral("auto_page_turn"), JsonSchema::boolean()             },
                    {QStringLiteral("piano"),          piano                             },
                    {QStringLiteral("parameters"),     parameters                        },
            },
                {QStringLiteral("window_id"), QStringLiteral("document_id"),
                 QStringLiteral("visible"), QStringLiteral("active_clip_id"),
                 QStringLiteral("active_region"), QStringLiteral("focused_region"),
                 QStringLiteral("time_viewport"), QStringLiteral("auto_page_turn"),
                 QStringLiteral("piano"), QStringLiteral("parameters")});
        }

        QJsonObject guiMutationOutputSchema() {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("window_id"),      nonEmptyStringSchema()},
                    {QStringLiteral("changed"),        JsonSchema::boolean() },
                    {QStringLiteral("validated_only"), JsonSchema::boolean() },
            },
                {QStringLiteral("window_id"), QStringLiteral("changed"),
                 QStringLiteral("validated_only")}));
        }

        QJsonObject settingsDomainStateSchema(const QJsonObject &valueSchema,
                                              const QJsonObject &candidatesSchema = {}) {
            QJsonObject properties{
                {QStringLiteral("configured"),         valueSchema          },
                {QStringLiteral("effective"),          valueSchema          },
                {QStringLiteral("restart_required"),   JsonSchema::boolean()},
                {QStringLiteral("available"),          JsonSchema::boolean()},
                {QStringLiteral("unavailable_reason"), JsonSchema::string() },
            };
            QStringList required{
                QStringLiteral("configured"),         QStringLiteral("effective"),
                QStringLiteral("restart_required"),   QStringLiteral("available"),
                QStringLiteral("unavailable_reason"),
            };
            if (!candidatesSchema.isEmpty()) {
                properties.insert(QStringLiteral("candidates"), candidatesSchema);
                required.append(QStringLiteral("candidates"));
            }
            return JsonSchema::object(properties, required);
        }

        QJsonObject singingSettingsValueSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("default_language"), nonEmptyStringSchema()    },
                    {QStringLiteral("default_lyrics"),   defaultLyricsInputSchema()},
            },
                {QStringLiteral("default_language"), QStringLiteral("default_lyrics")});
        }

        QJsonObject audioDeviceSettingsValueSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("driver_name"), JsonSchema::string()},
                    {QStringLiteral("device_name"), JsonSchema::string()},
                    {QStringLiteral("buffer_size"), JsonSchema::integer(0.0)},
                    {QStringLiteral("sample_rate"), JsonSchema::integer(0.0)},
                    {QStringLiteral("hot_plug_notification_mode"), JsonSchema::integer(0.0, 2.0)},
                    {QStringLiteral("gain"), JsonSchema::number(0.0, MaximumAudioDeviceGain)},
                    {QStringLiteral("pan"), JsonSchema::number(MinimumPan, MaximumPan)},
            },
                {QStringLiteral("driver_name"), QStringLiteral("device_name"),
                 QStringLiteral("buffer_size"), QStringLiteral("sample_rate"),
                 QStringLiteral("hot_plug_notification_mode"), QStringLiteral("gain"),
                 QStringLiteral("pan")});
        }

        QJsonObject computeDeviceSettingsValueSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("execution_provider"), nonEmptyStringSchema()                },
                    {QStringLiteral("gpu_index"),          JsonSchema::integer(-1.0)             },
                    {QStringLiteral("gpu_id"),             nullableSchema(nonEmptyStringSchema())},
            },
                {QStringLiteral("execution_provider"), QStringLiteral("gpu_index"),
                 QStringLiteral("gpu_id")});
        }

        QJsonObject renderSettingsValueSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("sampling_steps"), JsonSchema::integer(1.0, 1000.0)},
                    {QStringLiteral("depth"), JsonSchema::number(0.0, 1.0)},
                    {QStringLiteral("run_vocoder_on_cpu"), JsonSchema::boolean()},
                    {QStringLiteral("auto_start_inference"), JsonSchema::boolean()},
                    {QStringLiteral("playback_lookahead_seconds"), JsonSchema::number(0.0)},
                    {QStringLiteral("pitch_smooth_kernel_size"), JsonSchema::integer(0.0)},
            },
                {QStringLiteral("sampling_steps"), QStringLiteral("depth"),
                 QStringLiteral("run_vocoder_on_cpu"), QStringLiteral("auto_start_inference"),
                 QStringLiteral("playback_lookahead_seconds"),
                 QStringLiteral("pitch_smooth_kernel_size")});
        }

        QJsonObject retentionSettingsValueSchema() {
            auto idleTimeout = JsonSchema::integer(0.0, 300.0);
            idleTimeout.insert(QStringLiteral("multipleOf"), 60);
            return JsonSchema::object(
                {
                    {QStringLiteral("capacity"), JsonSchema::integer(0.0, 8.0)},
                    {QStringLiteral("idle_timeout_seconds"), idleTimeout},
            },
                {QStringLiteral("capacity"), QStringLiteral("idle_timeout_seconds")});
        }

        QJsonObject packagePathsValueSchema() {
            return JsonSchema::object(
                {
                    {QStringLiteral("paths"),
                     JsonSchema::array(nonEmptyStringSchema(), 0, MaximumCommandCollectionItems)}
            },
                {QStringLiteral("paths")});
        }

        QJsonObject settingsQueryOutputSchema() {
            const auto stringCandidates = JsonSchema::array(nonEmptyStringSchema());
            const auto languageCandidates = JsonSchema::object(
                {
                    {QStringLiteral("languages"), stringCandidates}
            },
                {QStringLiteral("languages")});
            const auto audioDeviceCandidate = JsonSchema::object(
                {
                    {QStringLiteral("device_name"),  JsonSchema::string()                       },
                    {QStringLiteral("name"),         JsonSchema::string()                       },
                    {QStringLiteral("buffer_sizes"), JsonSchema::array(JsonSchema::integer(0.0))},
                    {QStringLiteral("sample_rates"), JsonSchema::array(JsonSchema::number(0.0)) },
            },
                {QStringLiteral("device_name"), QStringLiteral("name"),
                 QStringLiteral("buffer_sizes"), QStringLiteral("sample_rates")});
            const auto audioDriverCandidate = JsonSchema::object(
                {
                    {QStringLiteral("driver_name"), nonEmptyStringSchema()                 },
                    {QStringLiteral("name"),        JsonSchema::string()                   },
                    {QStringLiteral("devices"),     JsonSchema::array(audioDeviceCandidate)},
            },
                {QStringLiteral("driver_name"), QStringLiteral("name"), QStringLiteral("devices")});
            const auto audioCandidates = JsonSchema::object(
                {
                    {QStringLiteral("drivers"), JsonSchema::array(audioDriverCandidate)},
                    {QStringLiteral("hot_plug_notification_modes"),
                     JsonSchema::array(JsonSchema::integer(0.0, 2.0))},
                    {QStringLiteral("gain_range"),
                     JsonSchema::object({{QStringLiteral("minimum"), JsonSchema::number(0.0)},
                                         {QStringLiteral("maximum"), JsonSchema::number(0.0)}},
                     {QStringLiteral("minimum"), QStringLiteral("maximum")})},
                    {QStringLiteral("pan_range"),
                     JsonSchema::object(
                         {{QStringLiteral("minimum"), JsonSchema::number(MinimumPan, MaximumPan)},
                          {QStringLiteral("maximum"), JsonSchema::number(MinimumPan, MaximumPan)}},
                     {QStringLiteral("minimum"), QStringLiteral("maximum")})},
            },
                {QStringLiteral("drivers"), QStringLiteral("hot_plug_notification_modes"),
                 QStringLiteral("gain_range"), QStringLiteral("pan_range")});
            const auto computeCandidates = JsonSchema::object(
                {
                    {QStringLiteral("execution_providers"), stringCandidates},
                    {QStringLiteral("gpus"),
                     JsonSchema::array(JsonSchema::object(
                         {
                             {QStringLiteral("index"), JsonSchema::integer(0.0)},
                             {QStringLiteral("id"), nonEmptyStringSchema()},
                             {QStringLiteral("name"), JsonSchema::string()},
                         }, {QStringLiteral("index"), QStringLiteral("id"), QStringLiteral("name")}))},
            },
                {QStringLiteral("execution_providers"), QStringLiteral("gpus")});
            const auto renderRanges = JsonSchema::object(
                {
                    {QStringLiteral("sampling_steps"),
                     JsonSchema::object({{QStringLiteral("minimum"), JsonSchema::integer(1.0)},
                                         {QStringLiteral("maximum"), JsonSchema::integer(1.0)}},
                     {QStringLiteral("minimum"), QStringLiteral("maximum")})},
                    {QStringLiteral("depth"),
                     JsonSchema::object({{QStringLiteral("minimum"), JsonSchema::number(0.0, 1.0)},
                                         {QStringLiteral("maximum"), JsonSchema::number(0.0, 1.0)}},
                     {QStringLiteral("minimum"), QStringLiteral("maximum")})},
                    {QStringLiteral("playback_lookahead_seconds"),
                     JsonSchema::object({{QStringLiteral("minimum"), JsonSchema::number(0.0)},
                                         {QStringLiteral("maximum"), JsonSchema::number(0.0)}},
                     {QStringLiteral("minimum"), QStringLiteral("maximum")})},
                    {QStringLiteral("pitch_smooth_kernel_size"),
                     JsonSchema::object({{QStringLiteral("minimum"), JsonSchema::integer(0.0)},
                                         {QStringLiteral("maximum"), JsonSchema::integer(0.0)}},
                     {QStringLiteral("minimum"), QStringLiteral("maximum")})},
            },
                {QStringLiteral("sampling_steps"), QStringLiteral("depth"),
                 QStringLiteral("playback_lookahead_seconds"),
                 QStringLiteral("pitch_smooth_kernel_size")});
            const auto retentionRanges = JsonSchema::object(
                {
                    {QStringLiteral("capacity"),
                     JsonSchema::object({{QStringLiteral("minimum"), JsonSchema::integer(0.0)},
                                         {QStringLiteral("maximum"), JsonSchema::integer(0.0)}},
                     {QStringLiteral("minimum"), QStringLiteral("maximum")})},
                    {QStringLiteral("idle_timeout_seconds"),
                     JsonSchema::object({{QStringLiteral("minimum"), JsonSchema::integer(0.0)},
                                         {QStringLiteral("maximum"), JsonSchema::integer(0.0)},
                                         {QStringLiteral("step"), JsonSchema::integer(1.0)}},
                     {QStringLiteral("minimum"), QStringLiteral("maximum"),
                                         QStringLiteral("step")})           },
            },
                {QStringLiteral("capacity"), QStringLiteral("idle_timeout_seconds")});

            const auto domains = JsonSchema::object({
                {QStringLiteral("ui_language"),
                 settingsDomainStateSchema(nonEmptyStringSchema(), stringCandidates)},
                {QStringLiteral("singing"),
                 settingsDomainStateSchema(singingSettingsValueSchema(), languageCandidates)},
                {QStringLiteral("theme"),
                 settingsDomainStateSchema(nonEmptyStringSchema(), stringCandidates)},
                {QStringLiteral("audio_device"),
                 settingsDomainStateSchema(audioDeviceSettingsValueSchema(), audioCandidates)},
                {QStringLiteral("playback_behavior"),
                 settingsDomainStateSchema(JsonSchema::integer(0.0, 2.0),
                 JsonSchema::array(JsonSchema::integer(0.0, 2.0)))},
                {QStringLiteral("compute_device"),
                 settingsDomainStateSchema(computeDeviceSettingsValueSchema(), computeCandidates)},
                {QStringLiteral("render"),
                 settingsDomainStateSchema(renderSettingsValueSchema(), renderRanges)},
                {QStringLiteral("singer_session_retention"),
                 settingsDomainStateSchema(retentionSettingsValueSchema(), retentionRanges)},
                {QStringLiteral("package_search_paths"),
                 settingsDomainStateSchema(packagePathsValueSchema())},
            });
            auto nonEmptyDomains = domains;
            nonEmptyDomains.insert(QStringLiteral("minProperties"), 1);
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("domains"), nonEmptyDomains}
            },
                {QStringLiteral("domains")}));
        }

        QJsonObject settingsMutationOutputSchema(const QJsonObject &valueSchema) {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("changed"),                 JsonSchema::boolean()                  },
                    {QStringLiteral("validated_only"),          JsonSchema::boolean()                  },
                    {QStringLiteral("configured"),              valueSchema                            },
                    {QStringLiteral("effective"),               valueSchema                            },
                    {QStringLiteral("restart_required"),        JsonSchema::boolean()                  },
                    {QStringLiteral("restart_required_fields"),
                     JsonSchema::array(nonEmptyStringSchema())                                         },
                    {QStringLiteral("warnings"),                JsonSchema::array(JsonSchema::string())},
            },
                {QStringLiteral("changed"), QStringLiteral("validated_only"),
                 QStringLiteral("configured"), QStringLiteral("effective"),
                 QStringLiteral("restart_required"), QStringLiteral("restart_required_fields"),
                 QStringLiteral("warnings")}));
        }

        QJsonObject packageVoiceSummarySchema(const bool detailed = false) {
            QJsonObject properties{
                {QStringLiteral("singer_id"), nonEmptyStringSchema()                   },
                {QStringLiteral("name"),      JsonSchema::string()                     },
                {QStringLiteral("languages"), JsonSchema::array(nonEmptyStringSchema())},
                {QStringLiteral("speakers"),  JsonSchema::array(nonEmptyStringSchema())},
            };
            QStringList required{
                QStringLiteral("singer_id"),
                QStringLiteral("name"),
                QStringLiteral("languages"),
                QStringLiteral("speakers"),
            };
            if (detailed) {
                properties.insert(QStringLiteral("description"), JsonSchema::string());
                properties.insert(QStringLiteral("avatar_path"), JsonSchema::string());
                required.append(QStringLiteral("description"));
                required.append(QStringLiteral("avatar_path"));
            }
            return JsonSchema::object(properties, required);
        }

        QJsonObject packageSummarySchema(const bool detailed = false) {
            QJsonObject properties{
                {QStringLiteral("package_id"),     nonEmptyStringSchema()                                },
                {QStringLiteral("name"),           JsonSchema::string()                                  },
                {QStringLiteral("version"),        JsonSchema::string()                                  },
                {QStringLiteral("vendor"),         JsonSchema::string()                                  },
                {QStringLiteral("canonical_path"), nullableSchema(nonEmptyStringSchema())                },
                {QStringLiteral("voices"),         JsonSchema::array(packageVoiceSummarySchema(detailed))},
            };
            QStringList required{
                QStringLiteral("package_id"),     QStringLiteral("name"),
                QStringLiteral("version"),        QStringLiteral("vendor"),
                QStringLiteral("canonical_path"), QStringLiteral("voices"),
            };
            if (detailed) {
                properties.insert(QStringLiteral("description"), JsonSchema::string());
                properties.insert(QStringLiteral("license"), JsonSchema::string());
                properties.insert(QStringLiteral("homepage"), JsonSchema::string());
                required.append(QStringLiteral("description"));
                required.append(QStringLiteral("license"));
                required.append(QStringLiteral("homepage"));
            }
            return JsonSchema::object(properties, required);
        }

        QJsonObject packagesListOutputSchema() {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("packages"),    JsonSchema::array(packageSummarySchema(false))},
                    {QStringLiteral("next_cursor"), JsonSchema::string()                          },
            },
                {QStringLiteral("packages")}));
        }

        QJsonObject packageDescribeOutputSchema() {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("package"), packageSummarySchema(true)}
            },
                {QStringLiteral("package")}));
        }

        QJsonObject packageRefreshResultSchema() {
            const auto failure = JsonSchema::object(
                {
                    {QStringLiteral("path"),   nullableSchema(nonEmptyStringSchema())},
                    {QStringLiteral("reason"), JsonSchema::string()                  },
            },
                {QStringLiteral("path"), QStringLiteral("reason")});
            return JsonSchema::object(
                {
                    {QStringLiteral("packages"), JsonSchema::integer(0.0)                 },
                    {QStringLiteral("added"),    JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("updated"),  JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("removed"),  JsonSchema::array(nonEmptyStringSchema())},
                    {QStringLiteral("failures"), JsonSchema::array(failure)               },
            },
                {QStringLiteral("packages"), QStringLiteral("added"), QStringLiteral("updated"),
                 QStringLiteral("removed"), QStringLiteral("failures")});
        }

        QJsonObject lyricRuleSchema() {
            const auto common = QJsonObject{
                {QStringLiteral("rule_id"), nonEmptyStringSchema()  },
                {QStringLiteral("builtin"), JsonSchema::boolean()   },
                {QStringLiteral("enabled"), JsonSchema::boolean()   },
                {QStringLiteral("order"),   JsonSchema::integer(0.0)},
            };
            auto splitter = common;
            splitter.insert(QStringLiteral("kind"),
                            JsonSchema::constant(QStringLiteral("splitter")));
            splitter.insert(QStringLiteral("name"), nonEmptyStringSchema());
            splitter.insert(
                QStringLiteral("regexes"),
                JsonSchema::array(nonEmptyStringSchema(), 1, MaximumCommandCollectionItems));
            auto tagger = common;
            tagger.insert(QStringLiteral("kind"), JsonSchema::constant(QStringLiteral("tagger")));
            tagger.insert(QStringLiteral("name"), nonEmptyStringSchema());
            tagger.insert(QStringLiteral("language"), nonEmptyStringSchema());
            tagger.insert(QStringLiteral("entries"),
                          JsonSchema::array(taggerEntrySchema(), 1, MaximumCommandCollectionItems));
            return JsonSchema::oneOf(QJsonArray{
                JsonSchema::object(splitter, {QStringLiteral("rule_id"), QStringLiteral("kind"),
                                              QStringLiteral("builtin"), QStringLiteral("name"),
                                              QStringLiteral("regexes"), QStringLiteral("enabled"),
                                              QStringLiteral("order")}),
                JsonSchema::object(tagger, {QStringLiteral("rule_id"), QStringLiteral("kind"),
                                            QStringLiteral("builtin"), QStringLiteral("name"),
                                            QStringLiteral("language"), QStringLiteral("entries"),
                                            QStringLiteral("enabled"), QStringLiteral("order")}),
            });
        }

        QJsonObject lyricRulesListOutputSchema() {
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("rules"), JsonSchema::array(lyricRuleSchema())}
            },
                {QStringLiteral("rules")}));
        }

        QJsonObject lyricRuleMutationOutputSchema(const bool deletion = false) {
            QJsonObject properties{
                {QStringLiteral("changed"),        JsonSchema::boolean()                  },
                {QStringLiteral("validated_only"), JsonSchema::boolean()                  },
                {QStringLiteral("warnings"),       JsonSchema::array(JsonSchema::string())},
            };
            QStringList required{
                QStringLiteral("changed"),
                QStringLiteral("validated_only"),
                QStringLiteral("warnings"),
            };
            if (deletion) {
                properties.insert(QStringLiteral("rule_id"), nonEmptyStringSchema());
                required.append(QStringLiteral("rule_id"));
            } else {
                properties.insert(QStringLiteral("rule"), lyricRuleSchema());
                required.append(QStringLiteral("rule"));
            }
            return JsonSchema::document(JsonSchema::object(properties, required));
        }

        QJsonObject lyricRulesTestOutputSchema() {
            const auto tagged = JsonSchema::object(
                {
                    {QStringLiteral("lyric"),    JsonSchema::string()  },
                    {QStringLiteral("language"), nonEmptyStringSchema()},
                    {QStringLiteral("tag"),      nonEmptyStringSchema()},
                    {QStringLiteral("discard"),  JsonSchema::boolean() },
            },
                {QStringLiteral("lyric"), QStringLiteral("language"), QStringLiteral("tag"),
                 QStringLiteral("discard")});
            return JsonSchema::document(JsonSchema::object(
                {
                    {QStringLiteral("split_tokens"),  JsonSchema::array(JsonSchema::string())},
                    {QStringLiteral("tagged_tokens"), JsonSchema::array(tagged)              },
            },
                {QStringLiteral("split_tokens"), QStringLiteral("tagged_tokens")}));
        }

        QJsonObject l3OutputSchema(const QString &id) {
            if (id == PublicToolNames::workspace_get)
                return JsonSchema::document(workspaceSnapshotObjectSchema());
            if (id == PublicToolNames::track_panel_get)
                return JsonSchema::document(trackPanelSnapshotObjectSchema());
            if (id == PublicToolNames::clip_editor_get)
                return JsonSchema::document(clipEditorSnapshotObjectSchema());
            if (isL3GuiOperation(id))
                return guiMutationOutputSchema();

            if (id == PublicToolNames::settings_query)
                return settingsQueryOutputSchema();
            if (id == PublicToolNames::settings_ui_language_update)
                return settingsMutationOutputSchema(nonEmptyStringSchema());
            if (id == PublicToolNames::settings_singing_update)
                return settingsMutationOutputSchema(singingSettingsValueSchema());
            if (id == PublicToolNames::settings_theme_update)
                return settingsMutationOutputSchema(nonEmptyStringSchema());
            if (id == PublicToolNames::settings_audio_device_update)
                return settingsMutationOutputSchema(audioDeviceSettingsValueSchema());
            if (id == PublicToolNames::settings_playback_behavior_update)
                return settingsMutationOutputSchema(JsonSchema::integer(0.0, 2.0));
            if (id == PublicToolNames::settings_compute_device_update)
                return settingsMutationOutputSchema(computeDeviceSettingsValueSchema());
            if (id == PublicToolNames::settings_render_update)
                return settingsMutationOutputSchema(renderSettingsValueSchema());
            if (id == PublicToolNames::settings_singer_session_retention_update)
                return settingsMutationOutputSchema(retentionSettingsValueSchema());
            if (id == PublicToolNames::settings_package_search_paths_update)
                return settingsMutationOutputSchema(packagePathsValueSchema());

            if (id == PublicToolNames::packages_list)
                return packagesListOutputSchema();
            if (id == PublicToolNames::packages_describe)
                return packageDescribeOutputSchema();
            if (id == PublicToolNames::packages_refresh)
                return taskAcceptedSchema(false, true);

            if (id == PublicToolNames::lyric_rules_list)
                return lyricRulesListOutputSchema();
            if (id == PublicToolNames::lyric_rules_delete)
                return lyricRuleMutationOutputSchema(true);
            if (id == PublicToolNames::lyric_rules_create ||
                id == PublicToolNames::lyric_rules_update ||
                id == PublicToolNames::lyric_rules_set_enabled ||
                id == PublicToolNames::lyric_rules_move) {
                return lyricRuleMutationOutputSchema();
            }
            if (id == PublicToolNames::lyric_rules_test)
                return lyricRulesTestOutputSchema();

            qFatal("No explicit L3 output schema for operation '%s'", qPrintable(id));
            return {};
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
            if (id == PublicToolNames::application_get_status)
                return statusOutputSchema();
            if (id == PublicToolNames::application_get_file_access)
                return fileAccessOutputSchema();
            if (id == PublicToolNames::documents_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), documentSnapshotSchema());
            if (id == PublicToolNames::documents_list_recent)
                return recentDocumentsOutputSchema();
            if (id == PublicToolNames::formats_list)
                return formatsOutputSchema();
            if (id == PublicToolNames::formats_inspect)
                return formatInspectionOutputSchema();
            if (id == PublicToolNames::tracks_list) {
                return queryEnvelopeSchema(QStringLiteral("tracks"),
                                           JsonSchema::array(trackSnapshotSchema()), true);
            }
            if (id == PublicToolNames::tracks_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), trackSnapshotSchema(true));
            if (id == PublicToolNames::master_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), masterSnapshotSchema());
            if (id == PublicToolNames::clips_list) {
                return queryEnvelopeSchema(QStringLiteral("clips"),
                                           JsonSchema::array(clipSnapshotSchema()), true);
            }
            if (id == PublicToolNames::clips_get)
                return queryEnvelopeSchema(QStringLiteral("snapshot"), clipSnapshotSchema(true));
            if (id == PublicToolNames::audio_clips_get) {
                return queryEnvelopeSchema(QStringLiteral("snapshot"), audioClipSnapshotSchema());
            }
            if (id == PublicToolNames::speaker_mix_get) {
                return queryEnvelopeSchema(QStringLiteral("snapshot"), speakerMixSnapshotSchema());
            }
            if (id == PublicToolNames::speaker_mix_presets_list)
                return speakerMixPresetsOutputSchema();
            if (id == PublicToolNames::notes_list) {
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
                return tasksListOutputSchema();
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
            if (isL3Operation(id))
                return l3OutputSchema(id);
            if (syncMode == SyncMode::Asynchronous)
                return taskAcceptedSchema(id == PublicToolNames::documents_open);
            if (kind == OperationKind::Command) {
                if (id == PublicToolNames::documents_new)
                    return documentLifecycleResultSchema();
                if (isPersistentPlaybackOperation(id))
                    return persistentPlaybackMutationSchema();
                if (id.startsWith(QStringLiteral("playback.")))
                    return playbackMutationSchema();
                if (id == PublicToolNames::speaker_mix_presets_save)
                    return speakerMixPresetSaveOutputSchema();
                if (id == PublicToolNames::speaker_mix_presets_delete)
                    return applicationMutationSchema();
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
            if (operationId == PublicToolNames::application_get_status) {
                return QStringLiteral(
                    "Read the active editor instance, host, access profile, toolset version, and "
                    "stable document/window identities.");
            }
            if (operationId == PublicToolNames::application_get_file_access) {
                return QStringLiteral(
                    "Read the canonical automation read/write roots and temporary file "
                    "grants enforced by the editor File Guard.");
            }

            if (isL3GuiOperation(operationId)) {
                if (kind == OperationKind::Query) {
                    return QStringLiteral(
                               "Read the authoritative %1 GUI state for the explicit window and "
                               "document context. This is presentation-only and never changes the "
                               "document revision or History.")
                        .arg(category);
                }
                return QStringLiteral(
                           "Apply the GUI-equivalent %1 presentation action to the explicit "
                           "window. "
                           "Object-bearing requests validate stable document identities, while the "
                           "action itself never changes document revision or History.")
                    .arg(humanTitle(operationId).toLower());
            }
            if (operationId == PublicToolNames::settings_query) {
                return QStringLiteral(
                    "Read the configured and effective values, candidates or ranges, availability, "
                    "and restart requirements for the requested public settings domains.");
            }
            if (operationId.startsWith(QStringLiteral("settings."))) {
                return QStringLiteral(
                           "Sparsely update the public %1 settings domain without modal dialogs. "
                           "The response distinguishes configured and effective values and reports "
                           "fields that require an application restart.")
                    .arg(humanTitle(operationId).toLower());
            }
            if (operationId == PublicToolNames::packages_list ||
                operationId == PublicToolNames::packages_describe) {
                return QStringLiteral(
                           "Read %1 from the current package index. Reported canonical paths are "
                           "included only when permitted by the configured automation read roots.")
                    .arg(humanTitle(operationId).toLower());
            }
            if (operationId == PublicToolNames::packages_refresh) {
                return QStringLiteral(
                    "Rescan the current effective package search paths as an application-scoped "
                    "task, then atomically replace the package index and report added, updated, "
                    "removed, and failed entries.");
            }
            if (operationId.startsWith(QStringLiteral("lyric_rules."))) {
                if (kind == OperationKind::Query) {
                    return QStringLiteral(
                               "Read %1 from the application lyric splitter and tagger pipeline "
                               "without changing its configuration.")
                        .arg(humanTitle(operationId).toLower());
                }
                return QStringLiteral(
                           "Apply %1 to the application lyric-rule store using a stable rule ID. "
                           "Built-in rules retain their protected content and the operation never "
                           "changes a document revision or History.")
                    .arg(humanTitle(operationId).toLower());
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
            if (operationId == PublicToolNames::speaker_mix_presets_save ||
                operationId == PublicToolNames::speaker_mix_presets_delete) {
                return QStringLiteral(
                           "Apply the %1 operation to the editor's Speaker Mix preset store. "
                           "This changes application configuration without changing document "
                           "revision or History.")
                    .arg(action);
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
                sourceOperation.startsWith(QStringLiteral("settings.")) ||
                        sourceOperation.startsWith(QStringLiteral("packages.")) ||
                        sourceOperation.startsWith(QStringLiteral("lyric_rules."))
                    ? AutomationProfile::L3
                : sourceOperation == PublicToolNames::voices_list ||
                        sourceOperation == PublicToolNames::voices_describe ||
                        sourceOperation == PublicToolNames::parameters_get_capabilities ||
                        sourceOperation == PublicToolNames::tracks_list ||
                        sourceOperation == PublicToolNames::clips_list ||
                        sourceOperation == PublicToolNames::notes_list
                    ? AutomationProfile::L1
                    : AutomationProfile::L2;
            const auto availability = sourceProfile == AutomationProfile::L3
                                          ? QStringLiteral("both")
                                          : QStringLiteral("gui");
            return {
                {QStringLiteral("field_path"),        fieldPath                           },
                {QStringLiteral("operation_id"),      sourceOperation                     },
                {QStringLiteral("context_fields"),    context                             },
                {QStringLiteral("minimum_profile"),   automationProfileName(sourceProfile)},
                {QStringLiteral("host_availability"), availability                        },
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
                if (id == PublicToolNames::parameters_insert_anchors ||
                    id == PublicToolNames::parameters_create_anchor_curve) {
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
            if (id == PublicToolNames::speaker_mix_presets_list) {
                add(QStringLiteral("/singer"), PublicToolNames::voices_list);
            }
            if (id == PublicToolNames::speaker_mix_presets_save) {
                add(QStringLiteral("/preset/singer"), PublicToolNames::voices_list);
                add(QStringLiteral("/preset/sources/*/speaker"), PublicToolNames::voices_describe,
                    {QStringLiteral("/preset/singer")});
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
            if (id == PublicToolNames::packages_describe) {
                add(QStringLiteral("/package_id"), PublicToolNames::packages_list);
                add(QStringLiteral("/version"), PublicToolNames::packages_list,
                    {QStringLiteral("/package_id")});
            }
            if (id.startsWith(QStringLiteral("settings.")) &&
                id != PublicToolNames::settings_query) {
                const auto properties = inputSchema(id, OperationKind::Command)
                                            .value(QStringLiteral("properties"))
                                            .toObject();
                static const QStringList queryableFields{
                    QStringLiteral("ui_language"),
                    QStringLiteral("default_language"),
                    QStringLiteral("theme_id"),
                    QStringLiteral("driver_name"),
                    QStringLiteral("device_name"),
                    QStringLiteral("buffer_size"),
                    QStringLiteral("sample_rate"),
                    QStringLiteral("hot_plug_notification_mode"),
                    QStringLiteral("gain"),
                    QStringLiteral("pan"),
                    QStringLiteral("behavior"),
                    QStringLiteral("execution_provider"),
                    QStringLiteral("gpu_index"),
                    QStringLiteral("gpu_id"),
                    QStringLiteral("sampling_steps"),
                    QStringLiteral("depth"),
                    QStringLiteral("playback_lookahead_seconds"),
                    QStringLiteral("pitch_smooth_kernel_size"),
                    QStringLiteral("capacity"),
                    QStringLiteral("idle_timeout_seconds"),
                };
                for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
                    if (!queryableFields.contains(it.key()))
                        continue;
                    QStringList contextFields;
                    if (id == PublicToolNames::settings_audio_device_update) {
                        if (it.key() == QStringLiteral("device_name")) {
                            contextFields.append(QStringLiteral("/driver_name"));
                        } else if (it.key() == QStringLiteral("buffer_size") ||
                                   it.key() == QStringLiteral("sample_rate")) {
                            contextFields.append(QStringLiteral("/driver_name"));
                            contextFields.append(QStringLiteral("/device_name"));
                        }
                    } else if (id == PublicToolNames::settings_compute_device_update &&
                               (it.key() == QStringLiteral("gpu_index") ||
                                it.key() == QStringLiteral("gpu_id"))) {
                        contextFields.append(QStringLiteral("/execution_provider"));
                    }
                    add(u'/' + it.key(), PublicToolNames::settings_query, contextFields);
                }
            }
            if (id.startsWith(QStringLiteral("lyric_rules.")) &&
                id != PublicToolNames::lyric_rules_list &&
                id != PublicToolNames::lyric_rules_create &&
                id != PublicToolNames::lyric_rules_test) {
                add(QStringLiteral("/rule_id"), PublicToolNames::lyric_rules_list);
            }
            if (id == PublicToolNames::track_panel_select_track)
                add(QStringLiteral("/track_id"), PublicToolNames::tracks_list,
                    {QStringLiteral("/document_id")});
            if (id == PublicToolNames::track_panel_reveal_clips)
                add(QStringLiteral("/track_id"), PublicToolNames::tracks_list,
                    {QStringLiteral("/document_id")});
            if (id == PublicToolNames::track_panel_reveal_clips ||
                id == PublicToolNames::track_panel_select_clips) {
                add(QStringLiteral("/clip_ids/*"), PublicToolNames::clips_list,
                    {QStringLiteral("/document_id")});
            }
            if (id == PublicToolNames::clip_editor_set_active_clip)
                add(QStringLiteral("/clip_id"), PublicToolNames::clips_list,
                    {QStringLiteral("/document_id")});
            if (id == PublicToolNames::clip_editor_piano_reveal_notes ||
                id == PublicToolNames::clip_editor_piano_select_notes) {
                add(QStringLiteral("/note_ids/*"), PublicToolNames::notes_list,
                    {QStringLiteral("/window_id"), QStringLiteral("/document_id")});
            }
            if (id.startsWith(QStringLiteral("clip_editor.parameters.")) &&
                id != PublicToolNames::clip_editor_parameters_swap &&
                id != PublicToolNames::clip_editor_parameters_set_tool &&
                id != PublicToolNames::clip_editor_parameters_set_value_viewport) {
                add(QStringLiteral("/parameter"), PublicToolNames::parameters_get_capabilities,
                    {QStringLiteral("/window_id"), QStringLiteral("/document_id")});
            }
            return result;
        }

        FileAccess fileAccess(const QString &id) {
            if (id == PublicToolNames::settings_package_search_paths_update ||
                id.startsWith(QStringLiteral("packages."))) {
                return FileAccess::Read;
            }
            if (id == PublicToolNames::documents_open || id == PublicToolNames::documents_import ||
                id == PublicToolNames::documents_import_batch ||
                id == PublicToolNames::formats_inspect ||
                id == PublicToolNames::audio_clips_import ||
                id == PublicToolNames::audio_clips_import_batch ||
                id == PublicToolNames::audio_clips_relocate ||
                id == PublicToolNames::audio_clips_confirm_path ||
                id.startsWith(QStringLiteral("extract."))) {
                return FileAccess::Read;
            }
            if (id == PublicToolNames::documents_save || id == PublicToolNames::documents_save_as ||
                id == PublicToolNames::exports_midi_preview ||
                id == PublicToolNames::exports_midi_start ||
                id == PublicToolNames::exports_audio_preview ||
                id == PublicToolNames::exports_audio_start) {
                return FileAccess::Write;
            }
            return FileAccess::None;
        }

        QJsonObject toolAnnotations(const QString &id, const ToolEffect effect,
                                    const ToolRepeatability repeatability,
                                    const ToolWorldAccess worldAccess) {
            return {
                {QStringLiteral("title"),           humanTitle(id)                                },
                {QStringLiteral("readOnlyHint"),    effect == ToolEffect::ReadOnly                },
                {QStringLiteral("destructiveHint"), effect == ToolEffect::Destructive             },
                {QStringLiteral("idempotentHint"),  repeatability == ToolRepeatability::Idempotent},
                {QStringLiteral("openWorldHint"),   worldAccess == ToolWorldAccess::OpenWorld     },
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

    QString fileAccessName(const FileAccess access) {
        return publicStringValueDomainValues(PublicValueDomain::FileAccess)
            .value(static_cast<qsizetype>(access));
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
                      {QStringLiteral("minimum_toolset_version"),
                       static_cast<qint64>(minimumToolsetVersion)},
                      {QStringLiteral("category"), category},
                      {QStringLiteral("minimum_profile"), automationProfileName(minimumProfile)},
                      {QStringLiteral("kind"), operationKindName(kind)},
                      {QStringLiteral("sync_mode"), syncModeName(syncMode)},
                      {QStringLiteral("file_access"), fileAccessName(fileAccess)},
                      {QStringLiteral("host_availability"), hostAvailability},
                      {QStringLiteral("value_sources"), valueSources},
                  }},
             }                                           },
        };
    }

    const QList<ToolContract> &publicToolContracts() {
        static const QList<ToolContract> tools = [] {
            QList<ToolContract> result;
            result.reserve(175);
#define AUTOMATION_WIRE_PUBLIC_TOOL(symbol, name, categoryValue, profile, kindValue, syncValue,    \
                                    minimumValue, effectValue, repeatabilityValue,                 \
                                    worldAccessValue)                                              \
    do {                                                                                           \
        const QString operationId(PublicToolNames::symbol);                                        \
        const auto operationKind = OperationKind::kindValue;                                       \
        const auto operationSync = SyncMode::syncValue;                                            \
        const auto title = humanTitle(operationId);                                                \
        result.append({                                                                            \
            .operationId = operationId,                                                            \
            .minimumToolsetVersion = minimumValue,                                                 \
            .title = title,                                                                        \
            .description = toolDescription(operationId, QStringLiteral(categoryValue),             \
                                           operationKind, operationSync),                          \
            .category = QStringLiteral(categoryValue),                                             \
            .minimumProfile = AutomationProfile::profile,                                          \
            .kind = operationKind,                                                                 \
            .syncMode = operationSync,                                                             \
            .fileAccess = fileAccess(operationId),                                                 \
            .hostAvailability = isL3Operation(operationId) && !isL3GuiOperation(operationId)       \
                                    ? QStringLiteral("both")                                       \
                                    : QStringLiteral("gui"),                                       \
            .inputSchema = inputSchema(operationId, operationKind),                                \
            .outputSchema = outputSchema(operationId, operationKind, operationSync),               \
            .valueSources = valueSources(operationId),                                             \
            .annotations = toolAnnotations(operationId, ToolEffect::effectValue,                   \
                                           ToolRepeatability::repeatabilityValue,                  \
                                           ToolWorldAccess::worldAccessValue),                     \
        });                                                                                        \
    } while (false);
#include "PublicToolDefinitions.inc"
#undef AUTOMATION_WIRE_PUBLIC_TOOL
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
            const auto enabled = tool.minimumProfile == AutomationProfile::L0 ||
                                 (profile == AutomationProfile::Custom
                                      ? customEnabled.contains(tool.operationId)
                                      : presetIncludes(profile, tool.minimumProfile));
            if (enabled)
                result.append(tool);
        }
        return result;
    }

}
