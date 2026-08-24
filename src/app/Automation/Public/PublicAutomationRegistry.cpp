#include "PublicAutomationRegistry.h"
#include "PublicAutomationCodecs.h"

#include "../CoreRuntime.h"
#include "../OperationIds.h"
#include "Modules/FillLyric/Utils/SplitLyric.h"

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/CanonicalJson.h>
#include <lite/AutomationWire/PublicValueDomains.h>

#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QJsonDocument>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QVersionNumber>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace Automation {
    namespace {
        AutomationError error(const AutomationErrorCode code, QString message,
                              QString fieldPath = {}) {
            AutomationError result;
            result.code = code;
            result.message = std::move(message);
            result.fieldPath = std::move(fieldPath);
            return result;
        }

        AutomationError unavailable(const QString &message) {
            return error(AutomationErrorCode::HostCapabilityUnavailable, message);
        }

        QString admissionDomain(const AutomationWire::ToolContract &contract,
                                const QJsonObject &arguments,
                                const DocumentVersion &currentDocument) {
            const auto scope =
                contract.toManifestJson().value(QStringLiteral("concurrency_scope")).toString();
            if (scope == QStringLiteral("application"))
                return QStringLiteral("application");
            if (scope == QStringLiteral("playback"))
                return QStringLiteral("playback");

            auto document = arguments.value(QStringLiteral("document_id")).toString();
            if (document.isEmpty() && !currentDocument.documentId.isNull())
                document = currentDocument.documentId.toString();
            if (scope == QStringLiteral("task")) {
                const auto task = arguments.value(QStringLiteral("task_id")).toString();
                return task.isEmpty() ? QStringLiteral("tasks:") + document
                                      : QStringLiteral("task:") + task;
            }
            if (scope == QStringLiteral("export"))
                return QStringLiteral("export:") + document;
            if (scope == QStringLiteral("document"))
                return QStringLiteral("document:") + document;
            return QStringLiteral("scope:") + scope;
        }

        QString unescapePointerToken(QString token) {
            token.replace(QStringLiteral("~1"), QStringLiteral("/"));
            token.replace(QStringLiteral("~0"), QStringLiteral("~"));
            return token;
        }

        QStringList pointerSegments(const QString &pointer) {
            auto normalized = pointer;
            if (normalized.startsWith(u'/'))
                normalized.remove(0, 1);
            if (normalized.isEmpty())
                return {};
            auto result = normalized.split(u'/');
            for (auto &segment : result)
                segment = unescapePointerToken(segment);
            return result;
        }

        QList<QJsonValue> valuesAtPointer(const QJsonValue &root, const QString &pointer) {
            QList<QJsonValue> current{root};
            for (const auto &segment : pointerSegments(pointer)) {
                QList<QJsonValue> next;
                for (const auto &value : std::as_const(current)) {
                    if (value.isObject()) {
                        const auto object = value.toObject();
                        if (object.contains(segment))
                            next.append(object.value(segment));
                    } else if (value.isArray()) {
                        const auto array = value.toArray();
                        if (segment == QStringLiteral("*")) {
                            for (const auto &item : array)
                                next.append(item);
                        } else {
                            bool ok = false;
                            const auto index = segment.toLongLong(&ok);
                            if (ok && index >= 0 && index < array.size())
                                next.append(array.at(index));
                        }
                    }
                }
                current = std::move(next);
            }
            return current;
        }

        void collectConcretePointerValues(const QJsonValue &value, const QStringList &segments,
                                          const qsizetype offset, const QString &path,
                                          QList<QPair<QString, QJsonValue>> &result) {
            if (offset == segments.size()) {
                result.append({path.isEmpty() ? QStringLiteral("/") : path, value});
                return;
            }
            const auto &segment = segments.at(offset);
            if (value.isObject()) {
                const auto object = value.toObject();
                if (object.contains(segment)) {
                    collectConcretePointerValues(object.value(segment), segments, offset + 1,
                                                 path + u'/' + segment, result);
                }
                return;
            }
            if (!value.isArray())
                return;
            const auto array = value.toArray();
            if (segment == QStringLiteral("*")) {
                for (qsizetype index = 0; index < array.size(); ++index) {
                    collectConcretePointerValues(array.at(index), segments, offset + 1,
                                                 path + u'/' + QString::number(index), result);
                }
                return;
            }
            bool ok = false;
            const auto index = segment.toLongLong(&ok);
            if (ok && index >= 0 && index < array.size()) {
                collectConcretePointerValues(array.at(index), segments, offset + 1,
                                             path + u'/' + segment, result);
            }
        }

        QList<QPair<QString, QJsonValue>> concretePointerValues(const QJsonValue &root,
                                                                const QString &pointer) {
            QList<QPair<QString, QJsonValue>> result;
            const auto segments = pointerSegments(pointer);
            collectConcretePointerValues(root, segments, 0, {}, result);
            return result;
        }

        QJsonObject schemaAtPointer(QJsonObject schema, const QString &pointer) {
            for (const auto &segment : pointerSegments(pointer)) {
                if (schema.contains(QStringLiteral("oneOf"))) {
                    QJsonObject selected;
                    for (const auto &branch : schema.value(QStringLiteral("oneOf")).toArray()) {
                        const auto candidate = branch.toObject();
                        if (candidate.value(QStringLiteral("properties"))
                                .toObject()
                                .contains(segment) ||
                            candidate.value(QStringLiteral("type")).toString() ==
                                QStringLiteral("array")) {
                            selected = candidate;
                            break;
                        }
                    }
                    schema = selected;
                }
                const auto type = schema.value(QStringLiteral("type")).toString();
                if (type == QStringLiteral("object")) {
                    schema = schema.value(QStringLiteral("properties"))
                                 .toObject()
                                 .value(segment)
                                 .toObject();
                } else if (type == QStringLiteral("array")) {
                    schema = schema.value(QStringLiteral("items")).toObject();
                } else {
                    return {};
                }
                if (schema.isEmpty())
                    return {};
            }
            return schema;
        }

        QString normalizedPointerPattern(const QString &pointer) {
            QStringList result;
            for (const auto &segment : pointerSegments(pointer)) {
                bool numeric = false;
                segment.toLongLong(&numeric);
                result.append(numeric ? QStringLiteral("*") : segment);
            }
            return u'/' + result.join(u'/');
        }

        QJsonObject optionEntry(const QJsonValue &value, QString label, const bool available = true,
                                QString unavailableReason = {}, QJsonObject metadata = {}) {
            QJsonObject result{
                {QStringLiteral("value"),     value           },
                {QStringLiteral("label"),     std::move(label)},
                {QStringLiteral("available"), available       },
            };
            if (!unavailableReason.isEmpty()) {
                result.insert(QStringLiteral("unavailable_reason"), std::move(unavailableReason));
            }
            if (!metadata.isEmpty())
                result.insert(QStringLiteral("metadata"), std::move(metadata));
            return result;
        }

        bool optionMatches(const QJsonValue &actual, const QJsonObject &option) {
            const auto expected = option.value(QStringLiteral("value"));
            if (actual.isObject() || expected.isObject()) {
                if (!actual.isObject() || !expected.isObject())
                    return false;
                const auto actualRef = actual.toObject();
                const auto expectedRef = expected.toObject();
                bool comparedIdentity = false;
                for (const auto &key : {QStringLiteral("package_id"), QStringLiteral("singer_id"),
                                        QStringLiteral("speaker_id")}) {
                    if (!actualRef.contains(key))
                        continue;
                    comparedIdentity = true;
                    if (!expectedRef.contains(key) ||
                        actualRef.value(key) != expectedRef.value(key)) {
                        return false;
                    }
                }
                return comparedIdentity;
            }
            return actual == expected;
        }

        QJsonObject encodeDocumentVersion(const DocumentVersion &version) {
            return {
                {QStringLiteral("document_id"), version.documentId.toString()        },
                {QStringLiteral("revision"),    static_cast<qint64>(version.revision)},
            };
        }

        QJsonValue encodeNullableDocumentVersion(const DocumentVersion &version) {
            return version.documentId.isNull() ? QJsonValue(QJsonValue::Null)
                                               : QJsonValue(encodeDocumentVersion(version));
        }

        QJsonObject encodeObjectRef(const ObjectRef &ref) {
            return {
                {QStringLiteral("kind"), encodePublicObjectKind(ref.kind)},
                {QStringLiteral("id"),   ref.value                       },
            };
        }

        QJsonObject encodeMutation(const MutationResult &mutation) {
            QJsonArray affected;
            for (const auto &ref : mutation.affectedObjects)
                affected.append(encodeObjectRef(ref));
            QJsonArray created;
            for (const auto &ref : mutation.createdObjects) {
                created.append(QJsonObject{
                    {QStringLiteral("client_ref"), ref.clientRef              },
                    {QStringLiteral("object"),     encodeObjectRef(ref.object)},
                });
            }
            QJsonArray warnings;
            for (const auto &warning : mutation.warnings)
                warnings.append(warning);
            return {
                {QStringLiteral("previous"),             encodeNullableDocumentVersion(mutation.previous)},
                {QStringLiteral("current"),              encodeNullableDocumentVersion(mutation.current) },
                {QStringLiteral("changed"),              mutation.changed                                },
                {QStringLiteral("validated_only"),       mutation.validatedOnly                          },
                {QStringLiteral("affected_objects"),     affected                                        },
                {QStringLiteral("created_objects"),      created                                         },
                {QStringLiteral("resolved_values"),      QJsonArray{}                                    },
                {QStringLiteral("presentation_effects"), QJsonArray{}                                    },
                {QStringLiteral("warnings"),             warnings                                        },
            };
        }

        QJsonObject encodeDocumentLifecycleResult(const MutationResult &mutation,
                                                  const QString &path = {},
                                                  const bool dirty = false) {
            QJsonArray warnings;
            for (const auto &warning : mutation.warnings)
                warnings.append(warning);
            return {
                {QStringLiteral("previous"),             encodeNullableDocumentVersion(mutation.previous)},
                {QStringLiteral("current"),              encodeNullableDocumentVersion(mutation.current) },
                {QStringLiteral("path"),                 path                                            },
                {QStringLiteral("dirty"),                dirty                                           },
                {QStringLiteral("changed"),              mutation.changed                                },
                {QStringLiteral("validated_only"),       mutation.validatedOnly                          },
                {QStringLiteral("presentation_effects"), QJsonArray{}                                    },
                {QStringLiteral("warnings"),             warnings                                        },
            };
        }

        QJsonObject encodeTaskAccepted(const TaskAcceptedResult &accepted) {
            QJsonObject result{
                {QStringLiteral("document"),
                 accepted.document.documentId.isNull()
                     ? QJsonValue(QJsonValue::Null)
                     : QJsonValue(encodeDocumentVersion(accepted.document))},
                {QStringLiteral("validated_only"), accepted.validatedOnly  },
            };
            if (!accepted.validatedOnly)
                result.insert(QStringLiteral("task_id"), accepted.taskId.toString());
            return result;
        }

        QString taskStateName(const AutomationTaskState state) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::TaskState);
            const auto index = static_cast<qsizetype>(state);
            return index >= 0 && index < values.size() ? values.at(index) : QString();
        }

        QJsonObject encodeAutomationError(const AutomationError &value) {
            QJsonObject result{
                {QStringLiteral("code"),    errorCodeName(value.code)},
                {QStringLiteral("message"), value.message            },
            };
            if (!value.fieldPath.isEmpty())
                result.insert(QStringLiteral("field_path"), value.fieldPath);
            return result;
        }

        QJsonObject encodeTaskSnapshot(const AutomationTaskSnapshot &snapshot) {
            QJsonObject progress{
                {QStringLiteral("minimum"),       snapshot.progress.minimum      },
                {QStringLiteral("maximum"),       snapshot.progress.maximum      },
                {QStringLiteral("value"),         snapshot.progress.value        },
                {QStringLiteral("indeterminate"), snapshot.progress.indeterminate},
            };
            QJsonObject result{
                {QStringLiteral("task_id"),      snapshot.taskId.toString()                          },
                {QStringLiteral("operation_id"), snapshot.operationId                                },
                {QStringLiteral("document"),     encodeNullableDocumentVersion(snapshot.baseDocument)},
                {QStringLiteral("state"),        taskStateName(snapshot.state)                       },
                {QStringLiteral("progress"),     progress                                            },
            };
            if (snapshot.mutation)
                result.insert(QStringLiteral("result"), encodeMutation(*snapshot.mutation));
            if (snapshot.error)
                result.insert(QStringLiteral("error"), encodeAutomationError(*snapshot.error));
            if (!snapshot.createdByClientId.isEmpty()) {
                result.insert(QStringLiteral("created_by_client_id"), snapshot.createdByClientId);
            }
            return result;
        }

        DocumentId documentId(const QJsonObject &arguments) {
            return DocumentId::fromString(
                arguments.value(QStringLiteral("document_id")).toString());
        }

        CommandContext commandContext(const QJsonObject &arguments,
                                      const PublicInvocationContext &invocation) {
            return {
                .expected =
                    {
                               documentId(arguments),
                               static_cast<Revision>(
                            arguments.value(QStringLiteral("expected_revision")).toInteger()),
                               },
                .validateOnly = arguments.value(QStringLiteral("validate_only")).toBool(false),
                .idempotencyKey = arguments.value(QStringLiteral("idempotency_key")).toString(),
                .source = InvocationSource::PublicMcp,
                .clientId = invocation.clientId,
            };
        }

        CommandContext replacementCommandContext(CoreRuntime &runtime, const QJsonObject &arguments,
                                                 const PublicInvocationContext &invocation) {
            auto context = commandContext(arguments, invocation);
            if (!arguments.contains(QStringLiteral("document_id")))
                context.expected = runtime.documentVersion();
            return context;
        }

        CommandContext playbackCommandContext(CoreRuntime &runtime, const QJsonObject &arguments,
                                              const PublicInvocationContext &invocation) {
            auto context = commandContext(arguments, invocation);
            context.expected = runtime.documentVersion();
            return context;
        }

        template <typename Id>
        QList<Id> objectIds(const QJsonArray &values) {
            QList<Id> result;
            result.reserve(values.size());
            for (const auto &value : values)
                result.append(Id(value.toInt()));
            return result;
        }

        ParamInfo::Name parameterName(const QString &value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterName);
            const auto index = values.indexOf(value);
            return index < 0 || index >= static_cast<qsizetype>(ParamInfo::Unknown)
                       ? ParamInfo::Unknown
                       : static_cast<ParamInfo::Name>(index);
        }

        QString parameterName(const ParamInfo::Name value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterName);
            const auto index = static_cast<qsizetype>(value);
            return index >= 0 && index < values.size() ? values.at(index) : QString();
        }

        Param::Type parameterType(const QString &value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterLayer);
            const auto index = values.indexOf(value);
            return index < 0 || index >= static_cast<qsizetype>(Param::Unknown)
                       ? Param::Unknown
                       : static_cast<Param::Type>(index);
        }

        QString parameterType(const Param::Type value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterLayer);
            const auto index = static_cast<qsizetype>(value);
            return index >= 0 && index < values.size() ? values.at(index) : QString();
        }

        AnchorNode::InterpMode interpolation(const QString &value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::Interpolation);
            if (value == values.value(1))
                return AnchorNode::Linear;
            if (value == values.value(2))
                return AnchorNode::None;
            return AnchorNode::Hermite;
        }

        QString interpolation(const AnchorNode::InterpMode value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::Interpolation);
            switch (value) {
                case AnchorNode::Linear:
                    return values.value(1);
                case AnchorNode::Hermite:
                case AnchorNode::Cubic:
                    return values.value(0);
                case AnchorNode::None:
                    return values.value(2);
            }
            return {};
        }

        CurveDraftDto decodeCurve(const QJsonObject &object) {
            CurveDraftDto result;
            if (object.contains(QStringLiteral("curve_id")))
                result.id = CurveId(object.value(QStringLiteral("curve_id")).toInt());
            if (object.value(QStringLiteral("type")).toString() == QStringLiteral("anchor")) {
                result.type = CurveDraftDto::Type::Anchor;
                for (const auto &value : object.value(QStringLiteral("nodes")).toArray()) {
                    const auto node = value.toObject();
                    AnchorNodeDraftDto draft{
                        node.value(QStringLiteral("position")).toInt(),
                        node.value(QStringLiteral("value")).toInt(),
                        interpolation(node.value(QStringLiteral("interpolation")).toString()),
                    };
                    if (node.contains(QStringLiteral("anchor_id")))
                        draft.id = AnchorId(node.value(QStringLiteral("anchor_id")).toInt());
                    result.nodes.append(std::move(draft));
                }
            } else {
                result.type = CurveDraftDto::Type::Draw;
                result.localStart = object.value(QStringLiteral("local_start")).toInt();
                result.step = object.value(QStringLiteral("step")).toInt(5);
                for (const auto &value : object.value(QStringLiteral("values")).toArray())
                    result.values.append(value.toInt());
            }
            return result;
        }

        QJsonObject encodeCurve(const CurveDraftDto &curve) {
            if (curve.type == CurveDraftDto::Type::Anchor) {
                QJsonArray nodes;
                for (const auto &node : curve.nodes) {
                    QJsonObject encoded{
                        {QStringLiteral("anchor_id"),     node.id.value()                  },
                        {QStringLiteral("position"),      node.position                    },
                        {QStringLiteral("value"),         node.value                       },
                        {QStringLiteral("interpolation"), interpolation(node.interpolation)},
                    };
                    nodes.append(encoded);
                }
                QJsonObject result{
                    {QStringLiteral("type"),     QStringLiteral("anchor")},
                    {QStringLiteral("curve_id"), curve.id.value()        },
                    {QStringLiteral("nodes"),    nodes                   },
                };
                return result;
            }
            QJsonArray values;
            for (const auto value : curve.values)
                values.append(value);
            QJsonObject result{
                {QStringLiteral("type"),        QStringLiteral("draw")},
                {QStringLiteral("curve_id"),    curve.id.value()      },
                {QStringLiteral("local_start"), curve.localStart      },
                {QStringLiteral("step"),        curve.step            },
                {QStringLiteral("values"),      values                },
            };
            return result;
        }

        Pronunciation decodePronunciation(const QJsonValue &value) {
            Pronunciation result;
            if (value.isString()) {
                result.edited = value.toString();
            } else {
                const auto object = value.toObject();
                const auto text = object.value(QStringLiteral("value")).toString();
                if (object.value(QStringLiteral("source")).toString() ==
                    QStringLiteral("original")) {
                    result.original = text;
                } else {
                    result.edited = text;
                }
            }
            return result;
        }

        Phonemes decodePhonemes(const QJsonArray &array) {
            Phonemes result;
            bool allOffsetsPresent = !array.isEmpty();
            for (const auto &value : array) {
                const auto object = value.toObject();
                PhonemeName name;
                name.language = object.value(QStringLiteral("language")).toString();
                name.name = object.value(QStringLiteral("symbol")).toString();
                result.nameSeq.edited.append(std::move(name));
                if (object.contains(QStringLiteral("offset"))) {
                    result.offsetSeq.edited.append(object.value(QStringLiteral("offset")).toInt());
                } else {
                    allOffsetsPresent = false;
                }
            }
            if (!allOffsetsPresent)
                result.offsetSeq.edited.clear();
            return result;
        }

        QJsonArray encodePhonemes(const Phonemes &phonemes) {
            QJsonArray result;
            const auto &names = phonemes.nameSeq.result();
            const auto &offsets = phonemes.offsetSeq.result();
            for (qsizetype index = 0; index < names.size(); ++index) {
                const auto &name = names.at(index);
                QJsonObject encoded{
                    {QStringLiteral("symbol"),   name.name    },
                    {QStringLiteral("language"), name.language},
                };
                if (index < offsets.size())
                    encoded.insert(QStringLiteral("offset"), offsets.at(index));
                result.append(encoded);
            }
            return result;
        }

        QJsonObject encodePronunciation(const Pronunciation &pronunciation) {
            const bool edited = !pronunciation.edited.isEmpty();
            return {
                {QStringLiteral("value"),  edited ? pronunciation.edited : pronunciation.original},
                {QStringLiteral("source"),
                 edited ? QStringLiteral("edited") : QStringLiteral("original")                  },
            };
        }

        QString decodeLanguageSelection(const QJsonValue &value, const QString &fallback) {
            if (value.isString())
                return value.toString();
            const auto selection = value.toObject();
            const auto mode = selection.value(QStringLiteral("mode")).toString();
            if (mode == QStringLiteral("explicit"))
                return selection.value(QStringLiteral("language_id")).toString();
            if (mode == QStringLiteral("unknown"))
                return QStringLiteral("unknown");
            return fallback;
        }

        QJsonObject encodeLanguageSelection(const QString &language,
                                            const QString &singerDefault = {}) {
            if (language.isEmpty() || (!singerDefault.isEmpty() && language == singerDefault))
                return {
                    {QStringLiteral("mode"), QStringLiteral("follow_singer")}
                };
            if (language == QStringLiteral("unknown"))
                return {
                    {QStringLiteral("mode"), QStringLiteral("unknown")}
                };
            return {
                {QStringLiteral("mode"),        QStringLiteral("explicit")},
                {QStringLiteral("language_id"), language                  },
            };
        }

        NoteDraftDto decodeNote(const QJsonObject &object, const QString &defaultLanguage = {},
                                const QString &defaultLyric = QStringLiteral("la")) {
            NoteDraftDto result;
            result.clientRef = object.value(QStringLiteral("client_ref")).toString();
            result.localStart = object.value(QStringLiteral("local_start")).toInt();
            result.length = object.value(QStringLiteral("length")).toInt();
            result.keyIndex = object.value(QStringLiteral("key_index")).toInt(60);
            result.centShift = object.value(QStringLiteral("cent_shift")).toInt();
            result.lyric = object.contains(QStringLiteral("lyric"))
                               ? object.value(QStringLiteral("lyric")).toString()
                               : defaultLyric;
            result.language = object.contains(QStringLiteral("language"))
                                  ? decodeLanguageSelection(
                                        object.value(QStringLiteral("language")), defaultLanguage)
                                  : defaultLanguage;
            result.pronunciation =
                decodePronunciation(object.value(QStringLiteral("pronunciation")));
            for (const auto &candidate :
                 object.value(QStringLiteral("pronunciation_candidates")).toArray()) {
                result.pronunciationCandidates.append(candidate.toString());
            }
            result.phonemes = decodePhonemes(object.value(QStringLiteral("phonemes")).toArray());
            result.lineFeed = object.value(QStringLiteral("line_feed")).toBool();
            return result;
        }

        QJsonObject encodeNoteDraft(const NoteDraftDto &note) {
            QJsonArray pronunciationCandidates;
            for (const auto &candidate : note.pronunciationCandidates)
                pronunciationCandidates.append(candidate);
            return {
                {QStringLiteral("client_ref"),               note.clientRef                         },
                {QStringLiteral("local_start"),              note.localStart                        },
                {QStringLiteral("length"),                   note.length                            },
                {QStringLiteral("key_index"),                note.keyIndex                          },
                {QStringLiteral("cent_shift"),               note.centShift                         },
                {QStringLiteral("lyric"),                    note.lyric                             },
                {QStringLiteral("language"),                 note.language                          },
                {QStringLiteral("pronunciation"),            encodePronunciation(note.pronunciation)},
                {QStringLiteral("pronunciation_candidates"), pronunciationCandidates                },
                {QStringLiteral("phonemes"),                 encodePhonemes(note.phonemes)          },
                {QStringLiteral("line_feed"),                note.lineFeed                          },
            };
        }

        ClipPropertiesDto decodeClipProperties(const QJsonObject &object) {
            ClipPropertiesDto result;
            result.id = ClipId(object.value(QStringLiteral("clip_id")).toInt(-1));
            result.name = object.value(QStringLiteral("name")).toString();
            result.start = object.value(QStringLiteral("start")).toInt();
            result.length = object.value(QStringLiteral("length")).toInt(1);
            result.clipStart = object.value(QStringLiteral("clip_start")).toInt();
            result.clipLen = object.value(QStringLiteral("clip_length")).toInt(result.length);
            result.gain = object.value(QStringLiteral("gain")).toDouble();
            result.mute = object.value(QStringLiteral("mute")).toBool();
            return result;
        }

        PublicAudioClipProperties decodeAudioImportProperties(const QJsonObject &object) {
            PublicAudioClipProperties result;
            if (object.contains(QStringLiteral("name")))
                result.name = object.value(QStringLiteral("name")).toString();
            if (object.contains(QStringLiteral("start")))
                result.start = object.value(QStringLiteral("start")).toInt();
            if (object.contains(QStringLiteral("length")))
                result.length = object.value(QStringLiteral("length")).toInt();
            if (object.contains(QStringLiteral("clip_start")))
                result.clipStart = object.value(QStringLiteral("clip_start")).toInt();
            if (object.contains(QStringLiteral("clip_length")))
                result.clipLength = object.value(QStringLiteral("clip_length")).toInt();
            if (object.contains(QStringLiteral("gain")))
                result.gain = object.value(QStringLiteral("gain")).toDouble();
            if (object.contains(QStringLiteral("mute")))
                result.mute = object.value(QStringLiteral("mute")).toBool();
            return result;
        }

        TrackPropertiesDto decodeTrackProperties(const QJsonObject &object) {
            TrackPropertiesDto result;
            result.id = TrackId(object.value(QStringLiteral("track_id")).toInt(-1));
            result.name = object.value(QStringLiteral("name")).toString();
            result.gain = object.value(QStringLiteral("gain")).toDouble();
            result.pan = object.value(QStringLiteral("pan")).toDouble();
            result.mute = object.value(QStringLiteral("mute")).toBool();
            result.solo = object.value(QStringLiteral("solo")).toBool();
            return result;
        }

        SingerInfo decodeSinger(const QJsonObject &object) {
            SingerIdentifier identifier{
                object.value(QStringLiteral("singer_id")).toString(),
                object.value(QStringLiteral("package_id")).toString(),
                {},
            };
            return SingerInfo(identifier);
        }

        SpeakerInfo decodeSpeaker(const QJsonObject &object) {
            return SpeakerInfo(object.value(QStringLiteral("speaker_id")).toString());
        }

        QJsonObject encodeSingerRef(const SingerInfo &singer) {
            return {
                {QStringLiteral("package_id"), singer.packageId()},
                {QStringLiteral("singer_id"),  singer.singerId() },
            };
        }

        QJsonObject encodeVoiceSelection(const SingerInfo &singer, const SpeakerInfo &speaker) {
            return {
                {QStringLiteral("singer"),  encodeSingerRef(singer)       },
                {QStringLiteral("speaker"),
                 QJsonObject{{QStringLiteral("speaker_id"), speaker.id()}}},
            };
        }

        AutomationResult<QPair<SingerInfo, SpeakerInfo>>
            resolveVoiceSelection(CoreRuntime &runtime, const QJsonObject &selection,
                                  const QString &fieldPath) {
            const auto singerRef = selection.value(QStringLiteral("singer")).toObject();
            const auto speakerRef = selection.value(QStringLiteral("speaker")).toObject();
            const auto packageId = singerRef.value(QStringLiteral("package_id")).toString();
            const auto singerId = singerRef.value(QStringLiteral("singer_id")).toString();
            const auto speakerId = speakerRef.value(QStringLiteral("speaker_id")).toString();
            auto packages = runtime.packages().getInstalledPackages();
            if (!packages)
                return packages.getError();
            for (const auto &package : packages.get()) {
                for (const auto &singer : package.singers) {
                    if (singer.packageId != packageId || singer.singerId != singerId)
                        continue;
                    for (const auto &speaker : singer.info.speakers()) {
                        if (speaker.id() == speakerId)
                            return QPair<SingerInfo, SpeakerInfo>{singer.info, speaker};
                    }
                    return AutomationError::invalidArgument(
                        fieldPath + QStringLiteral(".speaker"),
                        QStringLiteral("The selected speaker does not belong to the singer"));
                }
            }
            return AutomationError::invalidArgument(
                fieldPath + QStringLiteral(".singer"),
                QStringLiteral("The selected singer is not installed"));
        }

        AutomationResult<QPair<SingerInfo, SpeakerMixModel::SpeakerMixData>>
            resolveSpeakerMix(CoreRuntime &runtime, const QJsonObject &object,
                              const QString &fieldPath) {
            const auto singerRef = object.value(QStringLiteral("singer")).toObject();
            const auto packageId = singerRef.value(QStringLiteral("package_id")).toString();
            const auto singerId = singerRef.value(QStringLiteral("singer_id")).toString();
            auto packages = runtime.packages().getInstalledPackages();
            if (!packages)
                return packages.getError();
            for (const auto &package : packages.get()) {
                for (const auto &singer : package.singers) {
                    if (singer.packageId != packageId || singer.singerId != singerId)
                        continue;
                    SpeakerMixModel::SpeakerMixData mix;
                    QVector<double> fullWeights;
                    for (const auto &value : object.value(QStringLiteral("sources")).toArray()) {
                        const auto source = value.toObject();
                        const auto speakerId = source.value(QStringLiteral("speaker"))
                                                   .toObject()
                                                   .value(QStringLiteral("speaker_id"))
                                                   .toString();
                        const auto speakers = singer.info.speakers();
                        const auto found = std::find_if(
                            speakers.cbegin(), speakers.cend(),
                            [&](const SpeakerInfo &speaker) { return speaker.id() == speakerId; });
                        if (found == speakers.cend()) {
                            return AutomationError::invalidArgument(
                                fieldPath + QStringLiteral(".sources"),
                                QStringLiteral("A speaker does not belong to the singer"));
                        }
                        mix.sources.append({*found});
                        fullWeights.append(source.value(QStringLiteral("weight")).toDouble());
                    }
                    if (mix.sources.size() == 1) {
                        mix.mode = SpeakerMixModel::SingerSourceMode::Single;
                    } else {
                        mix.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
                        mix.fixedWeights =
                            SpeakerMixModel::explicitWeightsFromFullWeights(fullWeights);
                    }
                    return QPair<SingerInfo, SpeakerMixModel::SpeakerMixData>{
                        singer.info, SpeakerMixModel::normalizeSpeakerMixData(mix)};
                }
            }
            return AutomationError::invalidArgument(
                fieldPath + QStringLiteral(".singer"),
                QStringLiteral("The selected singer is not installed"));
        }

        SpeakerMixModel::SpeakerMixData decodeSpeakerMix(const QJsonObject &object) {
            SpeakerMixModel::SpeakerMixData result;
            if (object.contains(QStringLiteral("sources"))) {
                QVector<double> fullWeights;
                for (const auto &value : object.value(QStringLiteral("sources")).toArray()) {
                    const auto source = value.toObject();
                    result.sources.append(
                        {decodeSpeaker(source.value(QStringLiteral("speaker")).toObject())});
                    fullWeights.append(source.value(QStringLiteral("weight")).toDouble());
                }
                if (result.sources.size() > 1) {
                    result.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
                    result.fixedWeights =
                        SpeakerMixModel::explicitWeightsFromFullWeights(fullWeights);
                }
                return SpeakerMixModel::normalizeSpeakerMixData(result);
            }
            const auto mode = object.value(QStringLiteral("mode")).toString();
            result.mode =
                mode == QStringLiteral("dynamic") ? SpeakerMixModel::SingerSourceMode::DynamicMix
                : mode == QStringLiteral("fixed") ? SpeakerMixModel::SingerSourceMode::FixedMix
                                                  : SpeakerMixModel::SingerSourceMode::Single;
            for (const auto &value : object.value(QStringLiteral("speakers")).toArray())
                result.sources.append({decodeSpeaker(value.toObject())});
            for (const auto &value : object.value(QStringLiteral("fixed_weights")).toArray())
                result.fixedWeights.append(value.toDouble());
            for (const auto &value : object.value(QStringLiteral("dynamic_keyframes")).toArray()) {
                const auto keyframe = value.toObject();
                SpeakerMixModel::SpeakerMixKeyframe decoded;
                decoded.tick = keyframe.value(QStringLiteral("position")).toInt();
                for (const auto &weight : keyframe.value(QStringLiteral("weights")).toArray())
                    decoded.weights.append(weight.toDouble());
                result.dynamicKeyframes.append(std::move(decoded));
            }
            return result;
        }

        TrackDraftDto decodeTrack(const QJsonObject &object) {
            TrackDraftDto result;
            result.clientRef = object.value(QStringLiteral("client_ref")).toString();
            result.name = object.value(QStringLiteral("name")).toString();
            result.colorIndex = object.value(QStringLiteral("color_index")).toInt();
            result.gain = object.value(QStringLiteral("gain")).toDouble();
            result.pan = object.value(QStringLiteral("pan")).toDouble();
            result.mute = object.value(QStringLiteral("mute")).toBool();
            result.solo = object.value(QStringLiteral("solo")).toBool();
            result.defaultLanguage = object.value(QStringLiteral("default_language")).toString();
            if (object.contains(QStringLiteral("voice")))
                result.singerInfo = decodeSinger(object.value(QStringLiteral("voice")).toObject());
            return result;
        }

        ClipDraftDto decodeClip(const QJsonObject &object) {
            ClipDraftDto result;
            result.clientRef = object.value(QStringLiteral("client_ref")).toString();
            result.type = ClipDraftDto::Type::Singing;
            if (object.contains(QStringLiteral("properties"))) {
                result.properties =
                    decodeClipProperties(object.value(QStringLiteral("properties")).toObject());
            } else {
                result.properties.start = object.value(QStringLiteral("start")).toInt();
                result.properties.length = object.value(QStringLiteral("length")).toInt(1);
                result.properties.clipLen = result.properties.length;
                result.properties.name = object.value(QStringLiteral("name")).toString();
            }
            result.defaultLanguage = object.value(QStringLiteral("default_language")).toString();
            for (const auto &value : object.value(QStringLiteral("notes")).toArray())
                result.notes.append(decodeNote(value.toObject()));
            for (const auto &value : object.value(QStringLiteral("parameters")).toArray()) {
                const auto parameter = value.toObject();
                ParamCurvesDraftDto draft;
                draft.name = parameterName(parameter.value(QStringLiteral("name")).toString());
                draft.type = parameterType(parameter.value(QStringLiteral("type")).toString());
                for (const auto &curve : parameter.value(QStringLiteral("curves")).toArray())
                    draft.curves.append(decodeCurve(curve.toObject()));
                result.params.append(std::move(draft));
            }
            if (object.contains(QStringLiteral("speaker_mix"))) {
                const auto mix = object.value(QStringLiteral("speaker_mix")).toObject();
                result.usesTrackVoiceContext = false;
                result.ownSingerInfo = decodeSinger(mix.value(QStringLiteral("singer")).toObject());
                result.ownSpeakerMixData = decodeSpeakerMix(mix);
            }
            return result;
        }

        TrackControl decodeTrackControl(const QJsonObject &object) {
            TrackControl result;
            result.setGain(object.value(QStringLiteral("gain")).toDouble());
            result.setPan(object.value(QStringLiteral("pan")).toDouble());
            result.setMute(object.value(QStringLiteral("mute")).toBool());
            result.setSolo(object.value(QStringLiteral("solo")).toBool());
            return result;
        }

        AudioExportConfigDto decodeAudioExportConfig(const QJsonObject &object,
                                                     const QString &path = {}) {
            AudioExportConfigDto result;
            const QFileInfo fileInfo(path);
            result.fileName = fileInfo.fileName();
            result.fileDirectory = fileInfo.absolutePath();
            const auto format = object.value(QStringLiteral("format")).toString();
            if (format == QStringLiteral("wav"))
                result.fileType = 0;
            else if (format == QStringLiteral("flac"))
                result.fileType = 1;
            else if (format == QStringLiteral("ogg"))
                result.fileType = 2;
            else if (format == QStringLiteral("mp3"))
                result.fileType = 3;
            result.mono =
                object.value(QStringLiteral("channel_mode")).toString() == QStringLiteral("mono");
            result.sampleRate = object.value(QStringLiteral("sample_rate")).toDouble(44100.0);
            const auto mixing = object.value(QStringLiteral("mixing_mode")).toString();
            if (mixing == QStringLiteral("separated"))
                result.mixingOption = 1;
            else if (mixing == QStringLiteral("separated_through_master"))
                result.mixingOption = 2;
            const auto source = object.value(QStringLiteral("source")).toString();
            if (source == QStringLiteral("selected"))
                result.sourceOption = 1;
            else if (source == QStringLiteral("custom"))
                result.sourceOption = 2;
            result.muteSoloEnabled = object.value(QStringLiteral("mute_solo_enabled")).toBool(true);
            for (const auto &value : object.value(QStringLiteral("source_ids")).toArray())
                result.sources.append(value.toInt());
            return result;
        }

        AutomationResult<AudioExportConfigDto>
            resolveAudioExportSources(CoreRuntime &runtime, const DocumentId &documentId,
                                      AudioExportConfigDto config) {
            if (config.sourceOption != 2)
                return config;
            auto project = runtime.project().getProject(documentId);
            if (!project)
                return project.getError();
            QHash<int, int> trackIndexes;
            for (qsizetype index = 0; index < project.get().tracks.size(); ++index)
                trackIndexes.insert(project.get().tracks.at(index).id.value(), index);
            for (auto &source : config.sources) {
                const auto found = trackIndexes.constFind(source);
                if (found == trackIndexes.cend()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("options.source_ids"),
                        QStringLiteral("Audio export source TrackId does not exist"));
                }
                source = *found;
            }
            return config;
        }

        AutomationResult<QJsonObject>
            encodeAudioExportPreview(const AudioExportPreviewDto &preview) {
            constexpr quint32 knownFlags = AudioExportNoFile | AudioExportDuplicatedFile |
                                           AudioExportWillOverwrite |
                                           AudioExportUnrecognizedTemplate | AudioExportLossyFormat;
            if ((preview.warningFlags & ~knownFlags) != 0) {
                return error(AutomationErrorCode::InternalError,
                             QStringLiteral("Audio export preview returned an unknown diagnostic"));
            }
            QJsonArray targets;
            for (const auto &path : preview.filePaths)
                targets.append(QJsonObject{
                    {QStringLiteral("path"), path}
                });
            QJsonArray diagnostics;
            const auto append = [&](const quint32 flag, const QString &code,
                                    const QString &message) {
                if ((preview.warningFlags & flag) == 0)
                    return;
                diagnostics.append(QJsonObject{
                    {QStringLiteral("code"),     code                     },
                    {QStringLiteral("severity"), QStringLiteral("warning")},
                    {QStringLiteral("message"),  message                  },
                    {QStringLiteral("blocking"), true                     },
                });
            };
            append(AudioExportNoFile, QStringLiteral("no_files"),
                   QStringLiteral("The export plan contains no output files"));
            append(AudioExportDuplicatedFile, QStringLiteral("duplicate_paths"),
                   QStringLiteral("The export plan contains duplicate target paths"));
            append(AudioExportWillOverwrite, QStringLiteral("will_overwrite"),
                   QStringLiteral("The export plan would overwrite existing files"));
            append(AudioExportUnrecognizedTemplate, QStringLiteral("unrecognized_template"),
                   QStringLiteral("The export file-name template is not recognized"));
            append(AudioExportLossyFormat, QStringLiteral("lossy_format"),
                   QStringLiteral("The selected audio format is lossy"));
            return QJsonObject{
                {QStringLiteral("plan"),
                 QJsonObject{
                     {QStringLiteral("base_directory"), preview.baseDirectory},
                     {QStringLiteral("targets"), targets},
                 }                                         },
                {QStringLiteral("diagnostics"), diagnostics},
            };
        }

        QJsonObject queryResult(const DocumentVersion &document, const QString &name,
                                const QJsonValue &value) {
            return {
                {QStringLiteral("document"), encodeDocumentVersion(document)},
                {name,                       value                          },
            };
        }

        AutomationResult<QJsonObject> mutationResult(AutomationResult<MutationResult> result) {
            if (!result)
                return result.getError();
            return encodeMutation(result.get());
        }

        AutomationResult<QJsonObject>
            taskAcceptedResult(AutomationResult<TaskAcceptedResult> result) {
            if (!result)
                return result.getError();
            return encodeTaskAccepted(result.get());
        }

        AutomationResult<QJsonObject>
            completedTaskResult(CoreRuntime &runtime, const OperationId &operationId,
                                const QString &clientId,
                                AutomationResult<MutationResult> mutation) {
            if (!mutation)
                return mutation.getError();
            if (mutation.get().validatedOnly) {
                return encodeTaskAccepted({{}, mutation.get().previous, true});
            }
            const auto task = runtime.automationTasks().createTask(
                operationId, mutation.get().previous, std::nullopt, {}, clientId);
            runtime.automationTasks().markRunning(task.taskId);
            runtime.automationTasks().beginCommitting(task.taskId);
            runtime.automationTasks().succeed(task.taskId, mutation.get());
            return encodeTaskAccepted({task.taskId, mutation.get().previous, false});
        }

        QJsonObject encodeDocument(const DocumentSnapshotDto &snapshot) {
            return {
                {QStringLiteral("path"),          snapshot.path                                    },
                {QStringLiteral("project_name"),  snapshot.projectName                             },
                {QStringLiteral("busy"),          snapshot.busy                                    },
                {QStringLiteral("saved"),         snapshot.saved                                   },
                {QStringLiteral("dirty"),         !snapshot.saved                                  },
                {QStringLiteral("on_save_point"), snapshot.saved                                   },
                {QStringLiteral("lifecycle"),     encodePublicDocumentLifecycle(snapshot.lifecycle)},
            };
        }

        QJsonObject encodeTrackProperties(const TrackPropertiesDto &properties) {
            return {
                {QStringLiteral("name"), properties.name},
                {QStringLiteral("gain"), properties.gain},
                {QStringLiteral("pan"),  properties.pan },
                {QStringLiteral("mute"), properties.mute},
                {QStringLiteral("solo"), properties.solo},
            };
        }

        QJsonObject encodeClipProperties(const ClipPropertiesDto &properties) {
            return {
                {QStringLiteral("name"),        properties.name     },
                {QStringLiteral("start"),       properties.start    },
                {QStringLiteral("length"),      properties.length   },
                {QStringLiteral("clip_start"),  properties.clipStart},
                {QStringLiteral("clip_length"), properties.clipLen  },
                {QStringLiteral("gain"),        properties.gain     },
                {QStringLiteral("mute"),        properties.mute     },
            };
        }

        QJsonObject encodeProject(const ProjectSnapshotDto &snapshot) {
            QJsonArray tracks;
            for (const auto &track : snapshot.tracks) {
                QJsonArray clips;
                for (const auto &clip : track.clips) {
                    clips.append(QJsonObject{
                        {QStringLiteral("clip_id"),          clip.id.value()                                                  },
                        {QStringLiteral("track_id"),         clip.trackId.value()                                             },
                        {QStringLiteral("type"),             clip.data.type == ClipDraftDto::Type::Singing
                                                     ? QStringLiteral("singing")
                                                     : QStringLiteral("audio")},
                        {QStringLiteral("properties"),       encodeClipProperties(clip.data.properties)                       },
                        {QStringLiteral("default_language"), clip.data.defaultLanguage                                        },
                    });
                }
                tracks.append(QJsonObject{
                    {QStringLiteral("track_id"),         track.id.value()                     },
                    {QStringLiteral("properties"),
                     encodeTrackProperties({track.id, track.data.name, track.data.gain,
                                            track.data.pan, track.data.mute, track.data.solo})},
                    {QStringLiteral("color_index"),      track.data.colorIndex                },
                    {QStringLiteral("default_language"), track.data.defaultLanguage           },
                    {QStringLiteral("clips"),            clips                                },
                });
            }
            return {
                {QStringLiteral("tracks"), tracks}
            };
        }

        QJsonObject encodeTrackSnapshot(const TrackSnapshotDto &track, const qsizetype index) {
            return {
                {QStringLiteral("track_id"),            track.id.value()          },
                {QStringLiteral("index"),               static_cast<qint64>(index)},
                {QStringLiteral("name"),                track.data.name           },
                {QStringLiteral("color_index"),         track.data.colorIndex     },
                {QStringLiteral("gain"),                track.data.gain           },
                {QStringLiteral("pan"),                 track.data.pan            },
                {QStringLiteral("mute"),                track.data.mute           },
                {QStringLiteral("solo"),                track.data.solo           },
                {QStringLiteral("default_language_id"), track.data.defaultLanguage},
                {QStringLiteral("clip_count"),          track.clips.size()        },
            };
        }

        struct JsonPage {
            QJsonArray items;
            QString nextCursor;
        };

        AutomationResult<JsonPage> paginateJson(AutomationWire::OpaqueCursorCodec &codec,
                                                const QJsonArray &all, const QJsonObject &arguments,
                                                const QString &scope, const QJsonValue &identity) {
            const auto digest = AutomationWire::sha256Digest(identity);
            qint64 offset = 0;
            const auto cursor = arguments.value(QStringLiteral("cursor")).toString();
            if (!cursor.isEmpty()) {
                const auto parsed = codec.parse(cursor, scope, digest);
                if (!parsed.valid()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("cursor"), QStringLiteral("Collection cursor is invalid"));
                }
                offset = *parsed.offset;
            }
            if (offset < 0 || offset > all.size()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("cursor"), QStringLiteral("Collection cursor is invalid"));
            }
            const auto requested = arguments.value(QStringLiteral("limit")).toInt(all.size());
            const auto end = std::min<qint64>(all.size(), offset + requested);
            JsonPage result;
            for (auto index = offset; index < end; ++index)
                result.items.append(all.at(static_cast<qsizetype>(index)));
            if (end < all.size())
                result.nextCursor = codec.issue(scope, digest, end);
            return result;
        }

        QJsonObject encodeClipSnapshot(const ClipSnapshotDto &clip) {
            return {
                {QStringLiteral("clip_id"),             clip.id.value()                                       },
                {QStringLiteral("track_id"),            clip.trackId.value()                                  },
                {QStringLiteral("type"),                clip.data.type == ClipDraftDto::Type::Singing
                                             ? QStringLiteral("singing")
                                             : QStringLiteral("audio")},
                {QStringLiteral("name"),                clip.data.properties.name                             },
                {QStringLiteral("start"),               clip.data.properties.start                            },
                {QStringLiteral("length"),              clip.data.properties.length                           },
                {QStringLiteral("gain"),                clip.data.properties.gain                             },
                {QStringLiteral("mute"),                clip.data.properties.mute                             },
                {QStringLiteral("default_language_id"), clip.data.defaultLanguage                             },
            };
        }

        QJsonObject encodeVoiceContext(const SingerInfo &ownSinger, const SpeakerInfo &ownSpeaker,
                                       const SingerInfo &effectiveSinger,
                                       const SpeakerInfo &effectiveSpeaker,
                                       const bool inheritsTrack, const QString &defaultLanguage) {
            const bool ownAvailable = !ownSinger.isEmpty() && !ownSpeaker.isEmpty();
            const bool effectiveAvailable =
                !effectiveSinger.isEmpty() && !effectiveSpeaker.isEmpty();
            return {
                {QStringLiteral("own_voice"),
                 ownAvailable ? QJsonValue(encodeVoiceSelection(ownSinger, ownSpeaker))
                              : QJsonValue(QJsonValue::Null)},
                {QStringLiteral("effective_voice"),
                 effectiveAvailable
                     ? QJsonValue(encodeVoiceSelection(effectiveSinger, effectiveSpeaker))
                     : QJsonValue(QJsonValue::Null)},
                {QStringLiteral("inherits_track"), inheritsTrack},
                {QStringLiteral("default_language"),
                 defaultLanguage.isEmpty()
                     ? QJsonValue(QJsonValue::Null)
                     : QJsonValue(encodeLanguageSelection(defaultLanguage,
                 effectiveSinger.defaultLanguage()))},
                {QStringLiteral("available"), effectiveAvailable},
                {QStringLiteral("unavailable_reason"),
                 effectiveAvailable ? QString() : QStringLiteral("No voice is assigned")},
            };
        }

        QJsonObject encodeAudioClipSnapshot(const ClipSnapshotDto &clip) {
            const auto &audio = clip.data.audioInfo;
            QJsonArray candidates;
            if (!clip.data.audioPathInfo.relativeDir.isEmpty())
                candidates.append(clip.data.audioPathInfo.relativeDir);
            QString status = QStringLiteral("resolved");
            if (clip.data.audioPathStatus == AudioClip::PathStatus::Missing)
                status = QStringLiteral("missing");
            else if (clip.data.audioPathStatus == AudioClip::PathStatus::Unconfirmed)
                status = QStringLiteral("candidate");
            const auto duration =
                audio.sampleRate > 0 ? static_cast<double>(audio.frames) / audio.sampleRate : 0.0;
            return {
                {QStringLiteral("clip_id"), clip.id.value()},
                {QStringLiteral("path"), clip.data.audioPath},
                {QStringLiteral("path_status"), status},
                {QStringLiteral("candidate_paths"), candidates},
                {QStringLiteral("hash_exists"), !clip.data.audioPathInfo.sha512.isEmpty()},
                {QStringLiteral("duration_seconds"), duration},
                {QStringLiteral("sample_rate"), std::max(1, audio.sampleRate)},
                {QStringLiteral("channels"), std::max(1, audio.channels)},
            };
        }

        QJsonObject encodeSpeakerMixTarget(const SpeakerMixTargetDto &target) {
            return {
                {QStringLiteral("type"), target.kind == SpeakerMixTargetKind::Track
                                             ? QStringLiteral("track")
                                             : QStringLiteral("clip")},
                {QStringLiteral("id"),   target.id                                                           },
            };
        }

        SpeakerMixTargetDto decodeSpeakerMixTarget(const QJsonObject &target) {
            return {
                target.value(QStringLiteral("type")).toString() == QStringLiteral("clip")
                    ? SpeakerMixTargetKind::Clip
                    : SpeakerMixTargetKind::Track,
                target.value(QStringLiteral("id")).toInt(-1),
            };
        }

        QJsonObject encodeSpeakerMix(const SpeakerMixSnapshotDto &snapshot) {
            const auto mix = SpeakerMixModel::normalizeSpeakerMixData(snapshot.mix);
            QList<SpeakerInfo> speakers;
            if (mix.sources.isEmpty()) {
                if (!snapshot.speaker.isEmpty())
                    speakers.append(snapshot.speaker);
            } else {
                for (const auto &source : mix.sources)
                    speakers.append(source.speaker);
            }
            QVector<double> weights;
            if (speakers.size() == 1) {
                weights.append(1.0);
            } else if (!mix.fixedWeights.isEmpty()) {
                weights = SpeakerMixModel::fullWeightsFromExplicitWeights(mix.fixedWeights);
            } else if (!mix.dynamicKeyframes.isEmpty()) {
                weights = SpeakerMixModel::fullWeightsFromExplicitWeights(
                    mix.dynamicKeyframes.constFirst().weights);
            }
            QJsonArray sources;
            for (qsizetype index = 0; index < speakers.size(); ++index) {
                sources.append(QJsonObject{
                    {QStringLiteral("speaker"),
                     QJsonObject{{QStringLiteral("speaker_id"), speakers.at(index).id()}}},
                    {QStringLiteral("weight"), weights.value(index, index == 0 ? 1.0 : 0.0)},
                });
            }
            QJsonArray keyframes;
            for (const auto &keyframe : mix.dynamicKeyframes) {
                QJsonArray encodedWeights;
                for (const auto weight :
                     SpeakerMixModel::fullWeightsFromExplicitWeights(keyframe.weights)) {
                    encodedWeights.append(weight);
                }
                keyframes.append(QJsonObject{
                    {QStringLiteral("keyframe_id"), keyframe.id   },
                    {QStringLiteral("position"),    keyframe.tick },
                    {QStringLiteral("weights"),     encodedWeights},
                });
            }
            return {
                {QStringLiteral("target"),          encodeSpeakerMixTarget(snapshot.target)},
                {QStringLiteral("mix"),
                 QJsonObject{{QStringLiteral("singer"), encodeSingerRef(snapshot.singer)},
                             {QStringLiteral("sources"), sources}}                         },
                {QStringLiteral("dynamic_enabled"),
                 mix.mode == SpeakerMixModel::SingerSourceMode::DynamicMix                 },
                {QStringLiteral("bypassed"),        mix.dynamicBypassed                    },
                {QStringLiteral("keyframes"),       keyframes                              },
            };
        }

        QString effectiveDefaultLanguage(const ProjectSnapshotDto &project, const ClipId clipId) {
            for (const auto &track : project.tracks) {
                for (const auto &clip : track.clips) {
                    if (clip.id != clipId)
                        continue;
                    if (!clip.data.defaultLanguage.isEmpty())
                        return clip.data.defaultLanguage;
                    const auto singer = clip.data.usesTrackVoiceContext ? track.data.singerInfo
                                                                        : clip.data.ownSingerInfo;
                    if (clip.data.usesTrackVoiceContext && !track.data.defaultLanguage.isEmpty()) {
                        return track.data.defaultLanguage;
                    }
                    return singer.defaultLanguage();
                }
            }
            return {};
        }

        QJsonObject encodeParameter(const ParameterSnapshotDto &snapshot) {
            QJsonArray curves;
            for (const auto &curve : snapshot.curves)
                curves.append(encodeCurve(curve));
            return {
                {QStringLiteral("clip_id"), snapshot.clipId.value()     },
                {QStringLiteral("name"),    parameterName(snapshot.name)},
                {QStringLiteral("layer"),   parameterType(snapshot.type)},
                {QStringLiteral("curves"),  curves                      },
            };
        }

        QJsonObject encodePlayback(const PlaybackSnapshotDto &snapshot) {
            const auto states = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::PlaybackState);
            const auto state = states.value(static_cast<qsizetype>(snapshot.state));
            return {
                {QStringLiteral("state_version"), static_cast<qint64>(snapshot.stateVersion)},
                {QStringLiteral("state"),         state                                     },
                {QStringLiteral("playable"),      snapshot.playable                         },
                {QStringLiteral("position"),      snapshot.position                         },
                {QStringLiteral("last_position"), snapshot.lastPosition                     },
                {QStringLiteral("loop"),
                 QJsonObject{
                     {QStringLiteral("enabled"), snapshot.loop.enabled},
                     {QStringLiteral("start"), snapshot.loop.start},
                     {QStringLiteral("end"), snapshot.loop.end()},
                 }                                                                          },
            };
        }

        AutomationResult<QJsonObject>
            playbackMutationResult(CoreRuntime &runtime,
                                   AutomationResult<MutationResult> mutation) {
            if (!mutation)
                return mutation.getError();
            auto playback = runtime.playback().getPlayback(runtime.documentVersion().documentId);
            if (!playback)
                return playback.getError();
            QJsonArray warnings;
            for (const auto &warning : mutation.get().warnings)
                warnings.append(warning);
            return QJsonObject{
                {QStringLiteral("state_version"), static_cast<qint64>(playback.get().stateVersion)},
                {QStringLiteral("changed"),       mutation.get().changed                          },
                {QStringLiteral("playback"),      encodePlayback(playback.get())                  },
                {QStringLiteral("warnings"),      warnings                                        },
            };
        }

        AutomationResult<QJsonObject>
            validateFileArguments(AutomationFileGuard &guard,
                                  const AutomationWire::ToolContract &contract,
                                  const QJsonObject &arguments) {
            auto result = arguments;
            const auto access =
                contract.toManifestJson().value(QStringLiteral("file_access")).toString();
            if (access == QStringLiteral("none"))
                return result;
            const auto purpose = access == QStringLiteral("write") ? FileAccessPurpose::Write
                                                                   : FileAccessPurpose::Read;
            if (arguments.contains(QStringLiteral("path"))) {
                auto authorized =
                    guard.authorize(arguments.value(QStringLiteral("path")).toString(), purpose);
                if (!authorized)
                    return authorized.getError();
                result.insert(QStringLiteral("path"), authorized.get().canonicalPath);
            }
            if (arguments.contains(QStringLiteral("items"))) {
                QJsonArray items;
                for (const auto &value : arguments.value(QStringLiteral("items")).toArray()) {
                    auto item = value.toObject();
                    auto authorized =
                        guard.authorize(item.value(QStringLiteral("path")).toString(), purpose);
                    if (!authorized)
                        return authorized.getError();
                    item.insert(QStringLiteral("path"), authorized.get().canonicalPath);
                    items.append(item);
                }
                result.insert(QStringLiteral("items"), items);
            }
            return result;
        }

        AutomationResult<PublicPreparedAudioPath>
            prepareAuthorizedAudioPath(PublicAutomationHostServices &services,
                                       AutomationFileGuard &guard, const QString &canonicalPath) {
            if (!services.prepareAudioPath) {
                return unavailable(QStringLiteral("Audio path preparation service is unavailable"));
            }
            const AuthorizedPath authorizedPath{canonicalPath, FileAccessPurpose::Read};
            auto authorized = guard.reauthorize(authorizedPath);
            if (!authorized)
                return authorized.getError();
            auto prepared = services.prepareAudioPath(canonicalPath);
            if (!prepared)
                return prepared.getError();
            if (prepared.get().sha512.trimmed().isEmpty()) {
                return error(AutomationErrorCode::IoError,
                             QStringLiteral("Audio path preparation did not produce a hash"),
                             QStringLiteral("path"));
            }
            authorized = guard.reauthorize(authorizedPath);
            if (!authorized)
                return authorized.getError();
            return prepared;
        }

        std::function<AutomationResult<AutomationUnit>(const QString &)>
            sourcePathAuthorizer(AutomationFileGuard &guard) {
            auto frozen = std::make_shared<std::optional<AuthorizedPath>>();
            return [&guard, frozen](const QString &path) -> AutomationResult<AutomationUnit> {
                if (!*frozen) {
                    auto authorized = guard.authorize(path, FileAccessPurpose::Read);
                    if (!authorized)
                        return authorized.getError();
                    *frozen = authorized.get();
                    return AutomationUnit{};
                }
                auto authorized = guard.reauthorize(**frozen);
                if (!authorized)
                    return authorized.getError();
                return AutomationUnit{};
            };
        }
    }

    PublicAutomationRegistry::PublicAutomationRegistry(CoreRuntime &runtime,
                                                       AutomationAccessPolicy &accessPolicy,
                                                       AutomationFileGuard &fileGuard,
                                                       AdmissionController &admissionController,
                                                       PublicAutomationHostServices hostServices)
        : m_runtime(runtime), m_accessPolicy(accessPolicy), m_fileGuard(fileGuard),
          m_admissionController(admissionController), m_hostServices(std::move(hostServices)) {
        if (m_hostServices.editorInstanceId.isNull())
            m_hostServices.editorInstanceId = QUuid::createUuid();
        registerBindings();
    }

    const QList<AutomationWire::ToolContract> &PublicAutomationRegistry::contracts() const {
        return AutomationWire::publicToolContracts();
    }

    QStringList PublicAutomationRegistry::bindingIds() const {
        auto result = m_handlers.keys();
        std::sort(result.begin(), result.end());
        return result;
    }

    QList<AutomationWire::ToolContract> PublicAutomationRegistry::enabledContracts() const {
        return m_accessPolicy.enabledContracts();
    }

    bool PublicAutomationRegistry::isComplete() const {
        auto expected = AutomationWire::publicToolIds();
        auto actual = bindingIds();
        std::sort(expected.begin(), expected.end());
        return actual == expected;
    }

    const AutomationWire::ToolContract *
        PublicAutomationRegistry::contractForTracking(const QString &trackingId) const {
        const auto &all = contracts();
        const auto it = std::find_if(all.cbegin(), all.cend(), [&](const auto &contract) {
            return contract.trackingId == trackingId;
        });
        return it == all.cend() ? nullptr : &*it;
    }

    AutomationResult<QJsonArray>
        PublicAutomationRegistry::resolveValueOptions(const AutomationWire::ToolContract &target,
                                                      const QString &fieldPath,
                                                      const QJsonObject &partialArguments) {
        const auto fieldSchema = schemaAtPointer(target.inputSchema, fieldPath);
        if (fieldSchema.isEmpty()) {
            return AutomationError::invalidArgument(QStringLiteral("field_path"),
                                                    QStringLiteral("Target field does not exist"));
        }

        const auto normalizedField = normalizedPointerPattern(fieldPath);
        QJsonObject source;
        for (const auto &value : target.valueSources) {
            const auto candidate = value.toObject();
            if (candidate.value(QStringLiteral("field_path")).toString() == normalizedField) {
                source = candidate;
                break;
            }
        }
        if (source.isEmpty()) {
            return AutomationError::invalidArgument(
                QStringLiteral("field_path"),
                QStringLiteral(
                    "Target field has no dynamic value source; inspect its input schema"));
        }

        const auto sourceOperation = source.value(QStringLiteral("operation_id")).toString();
        const auto *sourceContract = AutomationWire::findPublicTool(sourceOperation);
        if (!sourceContract || !m_accessPolicy.isAllowed(*sourceContract)) {
            return error(AutomationErrorCode::PermissionDenied,
                         QStringLiteral("Value provider is unavailable under the active profile"),
                         QStringLiteral("field_path"));
        }

        if (sourceOperation == QStringLiteral("voices.list")) {
            auto packages = m_runtime.packages().getInstalledPackages();
            if (!packages)
                return packages.getError();
            QJsonArray result;
            for (const auto &package : packages.get()) {
                for (const auto &singer : package.singers) {
                    QJsonObject reference{
                        {QStringLiteral("singer_id"), singer.singerId},
                    };
                    if (!singer.packageId.isEmpty())
                        reference.insert(QStringLiteral("package_id"), singer.packageId);
                    result.append(optionEntry(reference, singer.name));
                }
            }
            return result;
        }

        auto singerReference = [&]() -> QJsonObject {
            for (const auto &pointer : {QStringLiteral("/singer"), QStringLiteral("/voice/singer"),
                                        QStringLiteral("/mix/singer")}) {
                const auto values = valuesAtPointer(partialArguments, pointer);
                if (!values.isEmpty() && values.constFirst().isObject())
                    return values.constFirst().toObject();
            }
            const auto document = DocumentId::fromString(
                partialArguments.value(QStringLiteral("document_id")).toString());
            if (document.isNull())
                return {};
            auto project = m_runtime.project().getProject(document);
            if (!project)
                return {};
            const auto requestedTrack =
                TrackId(partialArguments.value(QStringLiteral("track_id")).toInt(-1));
            const auto requestedClip =
                ClipId(partialArguments.value(QStringLiteral("clip_id")).toInt(-1));
            for (const auto &track : project.get().tracks) {
                const bool trackMatches = requestedTrack.isValid() && track.id == requestedTrack;
                if (trackMatches) {
                    return QJsonObject{
                        {QStringLiteral("singer_id"),  track.data.singerInfo.singerId() },
                        {QStringLiteral("package_id"), track.data.singerInfo.packageId()},
                    };
                }
                for (const auto &clip : track.clips) {
                    if (!requestedClip.isValid() || clip.id != requestedClip)
                        continue;
                    const auto singer = clip.data.usesTrackVoiceContext ? track.data.singerInfo
                                                                        : clip.data.ownSingerInfo;
                    return QJsonObject{
                        {QStringLiteral("singer_id"),  singer.singerId() },
                        {QStringLiteral("package_id"), singer.packageId()},
                    };
                }
            }
            return {};
        };

        if (sourceOperation == QStringLiteral("voices.describe")) {
            const auto reference = singerReference();
            const auto singerId = reference.value(QStringLiteral("singer_id")).toString();
            const auto packageId = reference.value(QStringLiteral("package_id")).toString();
            if (singerId.isEmpty()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("partial_arguments"),
                    QStringLiteral("A resolved voice context is required for this field"));
            }
            auto packages = m_runtime.packages().getInstalledPackages();
            if (!packages)
                return packages.getError();
            for (const auto &package : packages.get()) {
                for (const auto &singer : package.singers) {
                    if (singer.singerId != singerId ||
                        (!packageId.isEmpty() && singer.packageId != packageId)) {
                        continue;
                    }
                    QJsonArray result;
                    if (normalizedField.contains(QStringLiteral("language"))) {
                        for (const auto &language : singer.info.languages()) {
                            result.append(optionEntry(language.id(), language.name()));
                        }
                    } else {
                        const bool requiresMixable =
                            normalizedField.contains(QStringLiteral("/mix/speakers")) ||
                            normalizedField.contains(QStringLiteral("/mix/sources/"));
                        const bool capabilityKnown = singer.info.capability().has_value();
                        for (const auto &speaker : singer.info.speakers()) {
                            const bool available =
                                !requiresMixable || !capabilityKnown || speaker.mixable();
                            QJsonObject reference{
                                {QStringLiteral("speaker_id"), speaker.id()},
                            };
                            result.append(optionEntry(
                                reference, speaker.name(), available,
                                available
                                    ? QString()
                                    : QStringLiteral("Speaker is not mixable for this voice")));
                        }
                    }
                    return result;
                }
            }
            return error(AutomationErrorCode::NotFound, QStringLiteral("Singer was not found"),
                         QStringLiteral("partial_arguments"));
        }

        if (sourceOperation == QStringLiteral("parameters.get_capabilities")) {
            const auto document = DocumentId::fromString(
                partialArguments.value(QStringLiteral("document_id")).toString());
            const auto clipId = ClipId(partialArguments.value(QStringLiteral("clip_id")).toInt(-1));
            auto capabilities = m_runtime.parameters().getCapabilities(document, clipId);
            if (!capabilities)
                return capabilities.getError();
            QJsonArray result;
            for (const auto &capability : capabilities.get().parameters) {
                const auto selectedName = parameterName(
                    parameterName(partialArguments.value(QStringLiteral("name")).toString()));
                const bool numericValue = normalizedField.endsWith(QStringLiteral("/value")) ||
                                          normalizedField.contains(QStringLiteral("/values/"));
                if (numericValue) {
                    if (!selectedName.isEmpty() && selectedName != parameterName(capability.name)) {
                        continue;
                    }
                    result.append(optionEntry(
                        QJsonValue(QJsonValue::Null), QStringLiteral("Editable range"), true,
                        {
                    },
                        QJsonObject{
                            {QStringLiteral("minimum"), capability.valueSpec.minimum},
                            {QStringLiteral("maximum"), capability.valueSpec.maximum},
                            {QStringLiteral("step"), capability.valueSpec.step},
                            {QStringLiteral("unit"), capability.valueSpec.unit},
                        }));
                    break;
                } else if (normalizedField.contains(QStringLiteral("curves"))) {
                    if (normalizedField.contains(QStringLiteral("interpolation"))) {
                        for (const auto mode : capability.interpolations) {
                            const auto name = interpolation(mode);
                            result.append(optionEntry(name, name));
                        }
                    } else {
                        if (capability.supportsDraw)
                            result.append(
                                optionEntry(QStringLiteral("draw"), QStringLiteral("draw")));
                        if (capability.supportsAnchor)
                            result.append(
                                optionEntry(QStringLiteral("anchor"), QStringLiteral("anchor")));
                    }
                    break;
                } else if (normalizedField.endsWith(QStringLiteral("/name"))) {
                    const auto name = parameterName(capability.name);
                    result.append(optionEntry(name, name));
                } else if (normalizedField.endsWith(QStringLiteral("/layer"))) {
                    if (!selectedName.isEmpty() && selectedName != parameterName(capability.name))
                        continue;
                    for (const auto type : capability.types) {
                        const auto name = parameterType(type);
                        result.append(optionEntry(name, name));
                    }
                } else if (normalizedField.contains(QStringLiteral("interpolation"))) {
                    for (const auto mode : capability.interpolations) {
                        const auto name = interpolation(mode);
                        result.append(optionEntry(name, name));
                    }
                    break;
                }
            }
            return result;
        }

        if (sourceOperation == QStringLiteral("exports.audio.get_capabilities")) {
            if (!m_hostServices.audioExportCapabilities)
                return unavailable(
                    QStringLiteral("Audio export capability service is unavailable"));
            const auto document = DocumentId::fromString(
                partialArguments.value(QStringLiteral("document_id")).toString());
            auto capabilities = m_hostServices.audioExportCapabilities(document);
            if (!capabilities)
                return capabilities.getError();
            const auto object = capabilities.get().toObject();
            QJsonArray result;
            const auto appendScalarArray = [&](const QString &name) {
                for (const auto &value : object.value(name).toArray())
                    result.append(optionEntry(value, value.toVariant().toString()));
            };
            if (normalizedField.endsWith(QStringLiteral("/format")))
                appendScalarArray(QStringLiteral("formats"));
            else if (normalizedField.endsWith(QStringLiteral("/sample_rate")))
                appendScalarArray(QStringLiteral("sample_rates"));
            else if (normalizedField.endsWith(QStringLiteral("/channel_mode")))
                appendScalarArray(QStringLiteral("channel_modes"));
            else if (normalizedField.endsWith(QStringLiteral("/mixing_mode")))
                appendScalarArray(QStringLiteral("mixing_modes"));
            else if (normalizedField.endsWith(QStringLiteral("/source")))
                appendScalarArray(QStringLiteral("source_modes"));
            else if (normalizedField.endsWith(QStringLiteral("/source_ids"))) {
                for (const auto &value : object.value(QStringLiteral("sources")).toArray()) {
                    const auto source = value.toObject();
                    result.append(optionEntry(source.value(QStringLiteral("id")),
                                              source.value(QStringLiteral("name")).toString(),
                                              source.value(QStringLiteral("available")).toBool()));
                }
            }
            return result;
        }

        if (sourceOperation == QStringLiteral("extract.get_capabilities")) {
            if (!m_hostServices.extractionCapabilities)
                return unavailable(QStringLiteral("Extraction capability service is unavailable"));
            const auto document = DocumentId::fromString(
                partialArguments.value(QStringLiteral("document_id")).toString());
            const auto clipId = ClipId(partialArguments.value(QStringLiteral("clip_id")).toInt(-1));
            auto capabilities = m_hostServices.extractionCapabilities(document, clipId);
            if (!capabilities)
                return capabilities.getError();
            const auto object = capabilities.get().toObject();
            if (normalizedField.endsWith(QStringLiteral("/default_language"))) {
                QJsonArray result;
                for (const auto &language : object.value(QStringLiteral("languages")).toArray())
                    result.append(optionEntry(language, language.toString()));
                return result;
            }
            const auto capability =
                object
                    .value(target.operationId == OperationIds::extract::pitch::start
                               ? QStringLiteral("pitch")
                               : QStringLiteral("midi"))
                    .toObject();
            const auto models = capability.value(QStringLiteral("models")).toArray();
            QJsonArray result;
            for (const auto &value : models) {
                const auto model = value.toObject();
                result.append(
                    optionEntry(model.value(QStringLiteral("model_id")),
                                model.value(QStringLiteral("display_name")).toString(),
                                model.value(QStringLiteral("available")).toBool(),
                                model.value(QStringLiteral("unavailable_reason")).toString()));
            }
            return result;
        }

        if (sourceOperation == QStringLiteral("inference.get_capabilities")) {
            if (!m_hostServices.inferenceCapabilities)
                return unavailable(QStringLiteral("Inference capability service is unavailable"));
            const auto document = DocumentId::fromString(
                partialArguments.value(QStringLiteral("document_id")).toString());
            auto capabilities = m_hostServices.inferenceCapabilities(
                document, partialArguments.value(QStringLiteral("scope")).toObject());
            if (!capabilities)
                return capabilities.getError();
            const auto object = capabilities.get().toObject();
            QJsonArray result;
            if (normalizedField.contains(QStringLiteral("stage"))) {
                for (const auto &value : object.value(QStringLiteral("stages")).toArray())
                    result.append(optionEntry(value, value.toString()));
                return result;
            }
            QString collection;
            if (normalizedField.endsWith(QStringLiteral("/provider_id")))
                collection = QStringLiteral("providers");
            else if (normalizedField.endsWith(QStringLiteral("/device_id")))
                collection = QStringLiteral("devices");
            else
                collection = QStringLiteral("models");
            for (const auto &value : object.value(collection).toArray()) {
                const auto item = value.toObject();
                const auto idKey = collection == QStringLiteral("models")
                                       ? QStringLiteral("model_id")
                                       : QStringLiteral("id");
                result.append(optionEntry(
                    item.value(idKey), item.value(QStringLiteral("display_name")).toString(),
                    item.value(QStringLiteral("available")).toBool(),
                    item.value(QStringLiteral("unavailable_reason")).toString()));
            }
            return result;
        }

        return error(AutomationErrorCode::InternalError,
                     QStringLiteral("Dynamic value provider declaration is invalid"),
                     QStringLiteral("field_path"));
    }

    AutomationResult<AutomationUnit> PublicAutomationRegistry::validateDynamicArguments(
        const AutomationWire::ToolContract &target, const QJsonObject &arguments) {
        for (const auto &value : target.valueSources) {
            const auto source = value.toObject();
            const auto fieldPath = source.value(QStringLiteral("field_path")).toString();
            const auto actualValues = concretePointerValues(arguments, fieldPath);
            if (actualValues.isEmpty())
                continue;
            for (const auto &[concretePath, actual] : actualValues) {
                auto options = resolveValueOptions(target, concretePath, arguments);
                if (!options)
                    return options.getError();
                if (options.get().isEmpty()) {
                    return AutomationError::invalidArgument(
                        concretePath, QStringLiteral("No values are available for this field"));
                }
                QList<QJsonValue> leaves{actual};
                if (actual.isArray()) {
                    leaves.clear();
                    for (const auto &item : actual.toArray())
                        leaves.append(item);
                }
                for (const auto &leaf : std::as_const(leaves)) {
                    const auto rangeMetadata = options.get().size() == 1
                                                   ? options.get()
                                                         .first()
                                                         .toObject()
                                                         .value(QStringLiteral("metadata"))
                                                         .toObject()
                                                   : QJsonObject{};
                    if (rangeMetadata.contains(QStringLiteral("minimum"))) {
                        const auto number = leaf.toDouble();
                        const auto minimum =
                            rangeMetadata.value(QStringLiteral("minimum")).toInteger();
                        const auto maximum =
                            rangeMetadata.value(QStringLiteral("maximum")).toInteger();
                        const auto step = rangeMetadata.value(QStringLiteral("step")).toInteger();
                        if (!leaf.isDouble() || number != std::floor(number) || number < minimum ||
                            number > maximum || step <= 0 ||
                            (static_cast<qint64>(number) - minimum) % step != 0) {
                            return AutomationError::invalidArgument(
                                concretePath,
                                QStringLiteral("Value is outside the capability-declared range"));
                        }
                        continue;
                    }
                    const auto match = std::find_if(
                        options.get().constBegin(), options.get().constEnd(),
                        [&](const auto &candidate) {
                            const auto option = candidate.toObject();
                            return option.value(QStringLiteral("available")).toBool() &&
                                   optionMatches(leaf, option);
                        });
                    if (match == options.get().constEnd()) {
                        return AutomationError::invalidArgument(
                            concretePath,
                            QStringLiteral("Value is not available from the declared provider"));
                    }
                }
            }
        }
        return AutomationUnit{};
    }

    void PublicAutomationRegistry::addBinding(const QString &trackingId, Handler handler) {
        const auto *contract = contractForTracking(trackingId);
        Q_ASSERT(contract);
        Q_ASSERT(!m_handlers.contains(contract->operationId));
        m_handlers.insert(contract->operationId, std::move(handler));
    }

    void PublicAutomationRegistry::addOperationBinding(const QString &operationId,
                                                       Handler handler) {
        const auto *contract = AutomationWire::findPublicTool(operationId);
        Q_ASSERT(contract);
        Q_ASSERT(!m_handlers.contains(operationId));
        m_handlers.insert(operationId, std::move(handler));
    }

    AutomationResult<QJsonObject>
        PublicAutomationRegistry::invoke(const QString &operationId, const QJsonObject &arguments,
                                         const PublicInvocationContext &context) {
        const auto *contract = AutomationWire::findPublicTool(operationId);
        const auto handler = m_handlers.constFind(operationId);
        if (!contract || handler == m_handlers.cend()) {
            auto result = error(AutomationErrorCode::OperationUnavailable,
                                QStringLiteral("Public operation is not registered"),
                                QStringLiteral("operation_id"));
            result.operationId = operationId;
            return result;
        }
        if (!m_accessPolicy.isAllowed(*contract)) {
            auto result =
                error(AutomationErrorCode::PermissionDenied,
                      QStringLiteral("Public operation is disabled by the active profile"));
            result.operationId = operationId;
            return result;
        }
        const auto inputValidation =
            AutomationWire::validateJsonValue(arguments, contract->inputSchema);
        if (!inputValidation.valid()) {
            const auto &issue = inputValidation.issues.constFirst();
            auto result =
                error(AutomationErrorCode::InvalidArgument, issue.message, issue.instancePath);
            result.operationId = operationId;
            return result;
        }
        auto effectiveArguments = arguments;
        const auto saveCurrent = operationId == OperationIds::documents::save &&
                                 !effectiveArguments.contains(QStringLiteral("path"));
        if (saveCurrent) {
            auto document = m_runtime.documents().getDocument(documentId(effectiveArguments));
            if (!document) {
                auto result = document.getError();
                result.operationId = operationId;
                return result;
            }
            if (document.get().path.trimmed().isEmpty()) {
                auto result = AutomationError::invalidArgument(
                    QStringLiteral("path"),
                    QStringLiteral("The current document does not have a save path"));
                result.operationId = operationId;
                return result;
            }
            effectiveArguments.insert(QStringLiteral("path"), document.get().path);
            effectiveArguments.insert(QStringLiteral("allow_overwrite"), true);
        }
        auto dynamicValidation = validateDynamicArguments(*contract, effectiveArguments);
        if (!dynamicValidation) {
            auto result = dynamicValidation.getError();
            result.operationId = operationId;
            return result;
        }
        auto authorized = validateFileArguments(m_fileGuard, *contract, effectiveArguments);
        if (!authorized) {
            auto result = authorized.getError();
            result.operationId = operationId;
            return result;
        }
        const auto domain =
            admissionDomain(*contract, effectiveArguments, m_runtime.documentVersion());
        auto lease = m_admissionController.tryAcquire(
            context.clientId, domain, contract->syncMode == AutomationWire::SyncMode::Asynchronous);
        if (!lease) {
            auto result = lease.getError();
            result.operationId = operationId;
            return result;
        }
        auto result = handler.value()(authorized.get(), context);
        if (!result) {
            auto decorated = result.getError();
            decorated.operationId = operationId;
            return decorated;
        }
        const auto outputValidation =
            AutomationWire::validateJsonValue(result.get(), contract->outputSchema);
        if (!outputValidation.valid()) {
            const auto &issue = outputValidation.issues.constFirst();
            auto failure = error(AutomationErrorCode::InternalError,
                                 QStringLiteral("Public operation produced an invalid result: %1")
                                     .arg(issue.message),
                                 issue.instancePath);
            failure.operationId = operationId;
            return failure;
        }
        if (contract->syncMode == AutomationWire::SyncMode::Asynchronous) {
            const auto taskId =
                TaskId::fromString(result.get().value(QStringLiteral("task_id")).toString());
            if (!taskId.isNull()) {
                auto retainedLease = std::make_shared<AdmissionLease>(lease.get());
                m_runtime.automationTasks().setTerminalCallback(
                    taskId, [retainedLease](const AutomationTaskSnapshot &) {});
            }
        }
        return result;
    }

    void PublicAutomationRegistry::registerBindings() {
        addBinding(QStringLiteral("P2-TOOL-001"),
                   [this](const QJsonObject &, const PublicInvocationContext &) {
                       auto result = m_runtime.application().getInfo();
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("name"),     result.get().name    },
                           {QStringLiteral("version"),  result.get().version },
                           {QStringLiteral("platform"), result.get().platform},
                           {QStringLiteral("build_id"), result.get().version },
                       });
                   });
        addBinding(QStringLiteral("P2-TOOL-002"), [this](const QJsonObject &,
                                                         const PublicInvocationContext &) {
            const auto policy = m_accessPolicy.snapshot();
            const auto manifest = AutomationWire::buildPublicManifest(
                policy.profile, policy.customEnabled, m_hostServices.hostMode);
            QJsonArray documents;
            if (m_hostServices.documentStatus) {
                documents = m_hostServices.documentStatus();
            } else {
                const auto version = m_runtime.documentVersion();
                if (!version.documentId.isNull())
                    documents.append(encodeDocumentVersion(version));
            }
            while (documents.size() > 1)
                documents.removeLast();
            QJsonArray windows;
            if (m_hostServices.windowStatus) {
                windows = m_hostServices.windowStatus();
            } else {
                const auto version = m_runtime.documentVersion();
                if (!m_runtime.windowId().isNull() && !version.documentId.isNull()) {
                    windows.append(QJsonObject{
                        {QStringLiteral("window_id"),   m_runtime.windowId().toString()},
                        {QStringLiteral("document_id"), version.documentId.toString()  },
                    });
                }
            }
            while (windows.size() > 1)
                windows.removeLast();
            return AutomationResult<QJsonObject>(QJsonObject{
                {QStringLiteral("editor_instance_id"),
                 m_hostServices.editorInstanceId.toString(QUuid::WithoutBraces)                             },
                {QStringLiteral("host_mode"),          m_hostServices.hostMode                              },
                {QStringLiteral("profile"),            AutomationWire::automationProfileName(policy.profile)},
                {QStringLiteral("manifest"),
                 QJsonObject{
                     {QStringLiteral("toolset_version"),
                      static_cast<qint64>(AutomationWire::PublicToolsetVersion)},
                     {QStringLiteral("digest"), manifest.digest},
                 }                                                                                          },
                {QStringLiteral("documents"),          documents                                            },
                {QStringLiteral("windows"),            windows                                              },
            });
        });
        addBinding(QStringLiteral("P2-TOOL-003"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            const auto cursorText = arguments.value(QStringLiteral("cursor")).toString();
            const auto policy = m_accessPolicy.snapshot();
            const auto fullManifest = AutomationWire::buildPublicManifest(
                policy.profile, policy.customEnabled, m_hostServices.hostMode);
            qint64 offset = 0;
            if (!cursorText.isEmpty()) {
                const auto parsed = m_manifestCursorCodec.parse(
                    cursorText, QStringLiteral("editor-public-manifest/v1"), fullManifest.digest);
                if (!parsed.valid()) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("cursor"), QStringLiteral("Manifest cursor is invalid")));
                }
                offset = *parsed.offset;
            }
            if (offset < 0 || offset > fullManifest.operations.size()) {
                return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                    QStringLiteral("cursor"), QStringLiteral("Manifest cursor is invalid")));
            }
            const auto limit = arguments.value(QStringLiteral("limit")).toInt();
            auto manifest = AutomationWire::buildPublicManifest(
                policy.profile, policy.customEnabled, m_hostServices.hostMode,
                static_cast<qsizetype>(offset), limit);
            QJsonArray operations;
            for (const auto &operation : manifest.operations)
                operations.append(operation.toManifestJson());
            QJsonObject result{
                {QStringLiteral("toolset_version"),
                 static_cast<qint64>(AutomationWire::PublicToolsetVersion)                               },
                {QStringLiteral("digest"),          manifest.digest                                      },
                {QStringLiteral("profile"),         AutomationWire::automationProfileName(policy.profile)},
                {QStringLiteral("host_mode"),       m_hostServices.hostMode                              },
                {QStringLiteral("operations"),      operations                                           },
                {QStringLiteral("extensions"),      manifest.extensions                                  },
            };
            if (!manifest.nextCursor.isEmpty()) {
                const auto nextOffset = offset + manifest.operations.size();
                const auto nextCursor = m_manifestCursorCodec.issue(
                    QStringLiteral("editor-public-manifest/v1"), fullManifest.digest, nextOffset);
                if (nextCursor.isEmpty()) {
                    return AutomationResult<QJsonObject>(
                        error(AutomationErrorCode::InternalError,
                              QStringLiteral("Manifest cursor could not be issued"),
                              QStringLiteral("next_cursor")));
                }
                result.insert(QStringLiteral("next_cursor"), nextCursor);
            }
            return AutomationResult<QJsonObject>(std::move(result));
        });
        addBinding(QStringLiteral("P2-TOOL-004"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            const auto targetId = arguments.value(QStringLiteral("operation_id")).toString();
            const auto *target = AutomationWire::findPublicTool(targetId);
            if (!target || !m_accessPolicy.isAllowed(*target)) {
                return AutomationResult<QJsonObject>(error(
                    AutomationErrorCode::PermissionDenied,
                    QStringLiteral("Target operation is unavailable under the active profile"),
                    QStringLiteral("operation_id")));
            }
            const auto partial = arguments.value(QStringLiteral("partial_arguments")).toObject();
            const auto fieldPath = arguments.value(QStringLiteral("field_path")).toString();
            auto options = resolveValueOptions(*target, fieldPath, partial);
            if (!options)
                return AutomationResult<QJsonObject>(options.getError());
            QJsonArray dependencies;
            for (const auto &value : target->valueSources) {
                const auto source = value.toObject();
                if (source.value(QStringLiteral("field_path")).toString() !=
                    normalizedPointerPattern(fieldPath)) {
                    continue;
                }
                for (const auto &dependency :
                     source.value(QStringLiteral("context_fields")).toArray()) {
                    dependencies.append(dependency);
                }
            }
            QJsonObject result{
                {QStringLiteral("operation_id"), targetId     },
                {QStringLiteral("field_path"),   fieldPath    },
                {QStringLiteral("options"),      options.get()},
                {QStringLiteral("dependencies"), dependencies },
            };
            result.insert(QStringLiteral("context_digest"), AutomationWire::sha256Digest(partial));
            return AutomationResult<QJsonObject>(std::move(result));
        });
        addBinding(QStringLiteral("P2-TOOL-006"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.documents().getDocument(documentId(arguments));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            return AutomationResult<QJsonObject>(queryResult(
                result.get().document, QStringLiteral("snapshot"), encodeDocument(result.get())));
        });
        addBinding(QStringLiteral("P2-TOOL-007"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.project().getProject(documentId(arguments));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            return AutomationResult<QJsonObject>(queryResult(
                result.get().document, QStringLiteral("snapshot"), encodeProject(result.get())));
        });
        addBinding(QStringLiteral("P2-TOOL-068"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.notes().getNotes(
                documentId(arguments), ClipId(arguments.value(QStringLiteral("clip_id")).toInt()));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            QJsonArray notes;
            for (const auto &note : result.get()) {
                auto encoded = encodeNoteDraft(note.data);
                encoded.insert(QStringLiteral("note_id"), note.id.value());
                encoded.insert(QStringLiteral("clip_id"), note.clipId.value());
                notes.append(encoded);
            }
            auto page = paginateJson(
                m_collectionCursorCodec, notes, arguments, QStringLiteral("editor-public-notes/v1"),
                QJsonObject{
                    {QStringLiteral("document"),
                     encodeDocumentVersion(m_runtime.documentVersion())                    },
                    {QStringLiteral("clip_id"),  arguments.value(QStringLiteral("clip_id"))},
                    {QStringLiteral("notes"),    notes                                     }
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            auto encoded =
                queryResult(m_runtime.documentVersion(), QStringLiteral("notes"), page.get().items);
            if (!page.get().nextCursor.isEmpty())
                encoded.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            return AutomationResult<QJsonObject>(std::move(encoded));
        });
        addBinding(QStringLiteral("P2-TOOL-087"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.parameters().getParameter(
                documentId(arguments), ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                parameterName(arguments.value(QStringLiteral("name")).toString()),
                parameterType(arguments.value(QStringLiteral("layer")).toString()));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            return AutomationResult<QJsonObject>(queryResult(
                result.get().document, QStringLiteral("snapshot"), encodeParameter(result.get())));
        });
        addBinding(QStringLiteral("P2-TOOL-086"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
            auto result = m_runtime.parameters().getCapabilities(documentId(arguments), clipId);
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            QJsonArray parameters;
            for (const auto &capability : result.get().parameters) {
                QJsonArray layers;
                for (const auto type : capability.types)
                    layers.append(parameterType(type));
                QJsonArray curveTypes;
                if (capability.supportsDraw)
                    curveTypes.append(QStringLiteral("draw"));
                if (capability.supportsAnchor)
                    curveTypes.append(QStringLiteral("anchor"));
                QJsonArray interpolations;
                for (const auto mode : capability.interpolations)
                    interpolations.append(interpolation(mode));
                parameters.append(QJsonObject{
                    {QStringLiteral("name"),           parameterName(capability.name)},
                    {QStringLiteral("layers"),         layers                        },
                    {QStringLiteral("curve_types"),    curveTypes                    },
                    {QStringLiteral("interpolations"), interpolations                },
                    {QStringLiteral("editable"),       capability.editable           },
                    {QStringLiteral("range"),
                     QJsonObject{
                         {QStringLiteral("minimum"), capability.valueSpec.minimum},
                         {QStringLiteral("maximum"), capability.valueSpec.maximum},
                         {QStringLiteral("step"), capability.valueSpec.step},
                         {QStringLiteral("unit"), capability.valueSpec.unit},
                     }                                                               },
                });
            }
            return AutomationResult<QJsonObject>(
                queryResult(result.get().document, QStringLiteral("capabilities"),
                            QJsonObject{
                                {QStringLiteral("clip_id"),    clipId.value()},
                                {QStringLiteral("parameters"), parameters    }
            }));
        });
        addBinding(QStringLiteral("P2-TOOL-096"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.timeline().getTimeline(documentId(arguments));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            QJsonArray tempos;
            for (const auto &tempo : result.get().tempos) {
                tempos.append(QJsonObject{
                    {QStringLiteral("tick"),  tempo.pos  },
                    {QStringLiteral("tempo"), tempo.value}
                });
            }
            QJsonArray signatures;
            for (const auto &signature : result.get().timeSignatures) {
                signatures.append(QJsonObject{
                    {QStringLiteral("bar_index"),   signature.barIndex   },
                    {QStringLiteral("numerator"),   signature.numerator  },
                    {QStringLiteral("denominator"), signature.denominator},
                });
            }
            return AutomationResult<QJsonObject>(
                queryResult(result.get().document, QStringLiteral("snapshot"),
                            QJsonObject{
                                {QStringLiteral("tempos"),          tempos    },
                                {QStringLiteral("time_signatures"), signatures}
            }));
        });
        addBinding(QStringLiteral("P2-TOOL-101"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto result = m_runtime.history().getState(documentId(arguments));
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(queryResult(
                           result.get().document, QStringLiteral("snapshot"),
                           QJsonObject{
                               {QStringLiteral("can_undo"),      result.get().canUndo    },
                               {QStringLiteral("can_redo"),      result.get().canRedo    },
                               {QStringLiteral("on_save_point"), result.get().onSavePoint},
                               {QStringLiteral("undo_name"),     result.get().undoName   },
                               {QStringLiteral("redo_name"),     result.get().redoName   },
                       }));
                   });
        addBinding(QStringLiteral("P2-TOOL-057"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.packages().getInstalledPackages();
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            const auto query = arguments.value(QStringLiteral("query")).toString();
            const auto packageFilter = arguments.value(QStringLiteral("package_id")).toString();
            QJsonArray voices;
            for (const auto &package : result.get()) {
                if (!packageFilter.isEmpty() && package.id != packageFilter)
                    continue;
                for (const auto &singer : package.singers) {
                    if (!query.isEmpty() && !singer.name.contains(query, Qt::CaseInsensitive) &&
                        !singer.singerId.contains(query, Qt::CaseInsensitive)) {
                        continue;
                    }
                    voices.append(QJsonObject{
                        {QStringLiteral("package_id"), singer.packageId                },
                        {QStringLiteral("singer_id"),  singer.singerId                 },
                        {QStringLiteral("name"),       singer.name                     },
                        {QStringLiteral("version"),    singer.packageVersion.toString()},
                    });
                }
            }
            auto page = paginateJson(m_collectionCursorCodec, voices, arguments,
                                     QStringLiteral("editor-public-voices/v1"),
                                     QJsonObject{
                                         {QStringLiteral("query"),      query        },
                                         {QStringLiteral("package_id"), packageFilter},
                                         {QStringLiteral("singers"),    voices       }
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            QJsonObject encoded{
                {QStringLiteral("singers"), page.get().items}
            };
            if (!page.get().nextCursor.isEmpty())
                encoded.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            return AutomationResult<QJsonObject>(std::move(encoded));
        });
        addBinding(QStringLiteral("P2-TOOL-058"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            const auto voice = arguments.value(QStringLiteral("singer")).toObject();
            const auto singerId = voice.value(QStringLiteral("singer_id")).toString();
            const auto packageId = voice.value(QStringLiteral("package_id")).toString();
            auto result = m_runtime.packages().getInstalledPackages();
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            for (const auto &package : result.get()) {
                for (const auto &singer : package.singers) {
                    if (singer.singerId == singerId &&
                        (packageId.isEmpty() || singer.packageId == packageId)) {
                        const auto resolutionStates = AutomationWire::publicStringValueDomainValues(
                            AutomationWire::PublicValueDomain::VoiceResolutionState);
                        const auto resolutionState = singer.info.resolutionState();
                        const bool resolved = resolutionState == ResolutionState::Resolved;
                        const auto defaultLanguage = singer.info.defaultLanguage();
                        QJsonArray languageIds;
                        QJsonArray languages;
                        bool defaultG2pReady = false;
                        for (const auto &language : singer.info.languages()) {
                            const auto languageId = language.id();
                            const auto g2pId = language.g2p();
                            const bool g2pReady =
                                resolved && !g2pId.isEmpty() && g2pId != QStringLiteral("unknown");
                            languageIds.append(languageId);
                            languages.append(QJsonObject{
                                {QStringLiteral("language_id"), languageId                   },
                                {QStringLiteral("name"),        language.name()              },
                                {QStringLiteral("g2p_id"),      g2pId                        },
                                {QStringLiteral("g2p_ready"),   g2pReady                     },
                                {QStringLiteral("default"),     languageId == defaultLanguage},
                            });
                            if (languageId == defaultLanguage)
                                defaultG2pReady = g2pReady;
                        }
                        QJsonArray speakers;
                        const auto speakerInfos = singer.info.speakers();
                        const QString defaultSpeaker =
                            speakerInfos.isEmpty() ? QString() : speakerInfos.constFirst().id();
                        for (const auto &speaker : speakerInfos) {
                            speakers.append(QJsonObject{
                                {QStringLiteral("speaker_id"), speaker.id()                  },
                                {QStringLiteral("name"),       speaker.name()                },
                                {QStringLiteral("languages"),  languageIds                   },
                                {QStringLiteral("mixable"),    speaker.mixable()             },
                                {QStringLiteral("default"),    speaker.id() == defaultSpeaker},
                            });
                        }
                        const auto &capability = singer.info.capability();
                        const bool mixingSupported =
                            capability && capability->mixableSpeakers.size() > 1;
                        return AutomationResult<QJsonObject>(QJsonObject{
                            {QStringLiteral("snapshot"),
                             QJsonObject{
                                 {QStringLiteral("package_id"), singer.packageId},
                                 {QStringLiteral("singer_id"), singer.singerId},
                                 {QStringLiteral("name"), singer.name},
                                 {QStringLiteral("version"), singer.packageVersion.toString()},
                                 {QStringLiteral("speakers"), speakers},
                                 {QStringLiteral("languages"), languages},
                                 {QStringLiteral("default_speaker_id"),
                                  defaultSpeaker.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                           : QJsonValue(defaultSpeaker)},
                                 {QStringLiteral("default_language"),
                                  defaultLanguage.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                            : QJsonValue(defaultLanguage)},
                                 {QStringLiteral("g2p_ready"), defaultG2pReady},
                                 {QStringLiteral("resolution_state"),
                                  resolutionStates.value(static_cast<qsizetype>(resolutionState))},
                                 {QStringLiteral("mixing_supported"), mixingSupported},
                             }},
                        });
                    }
                }
            }
            return AutomationResult<QJsonObject>(error(AutomationErrorCode::NotFound,
                                                       QStringLiteral("Singer was not found"),
                                                       QStringLiteral("singer")));
        });
        addBinding(QStringLiteral("P2-TOOL-018"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       QList<TrackDraftDto> tracks;
                       for (const auto &value : arguments.value(QStringLiteral("tracks")).toArray())
                           tracks.append(decodeTrack(value.toObject()));
                       return mutationResult(m_runtime.project().insertTracks(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("index")).toInteger(), tracks));
                   });
        addBinding(
            QStringLiteral("P2-TOOL-019"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().removeTracks(
                    commandContext(arguments, invocation),
                    objectIds<TrackId>(arguments.value(QStringLiteral("track_ids")).toArray())));
            });
        addBinding(
            QStringLiteral("P2-TOOL-020"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().moveTracks(
                    commandContext(arguments, invocation),
                    objectIds<TrackId>(arguments.value(QStringLiteral("track_ids")).toArray()),
                    arguments.value(QStringLiteral("target_index")).toInteger()));
            });
        addBinding(QStringLiteral("P2-TOOL-021"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().renameTrack(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("name")).toString()));
                   });
        addBinding(QStringLiteral("P2-TOOL-022"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setTrackColor(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("color_index")).toInt()));
                   });
        addBinding(QStringLiteral("P2-TOOL-027"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setTrackDefaultLanguage(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("language_id")).toString()));
                   });
        addBinding(
            QStringLiteral("P2-TOOL-038"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QList<ClipInsertDto> clips;
                for (const auto &value : arguments.value(QStringLiteral("clips")).toArray()) {
                    const auto object = value.toObject();
                    clips.append({TrackId(object.value(QStringLiteral("track_id")).toInt()),
                                  decodeClip(object)});
                }
                return mutationResult(
                    m_runtime.project().insertClips(commandContext(arguments, invocation), clips));
            });
        addBinding(
            QStringLiteral("P2-TOOL-040"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().removeClips(
                    commandContext(arguments, invocation),
                    objectIds<ClipId>(arguments.value(QStringLiteral("clip_ids")).toArray())));
            });
        addBinding(QStringLiteral("P2-TOOL-044"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().renameClip(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("name")).toString()));
                   });
        addBinding(QStringLiteral("P2-TOOL-047"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setSingingClipDefaultLanguage(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("language_id")).toString()));
                   });
        addBinding(
            QStringLiteral("P2-TOOL-070"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                QString defaultLanguage;
                QString defaultLyric = QStringLiteral("la");
                auto settings = m_runtime.settings().getSettings();
                if (settings)
                    defaultLanguage = settings.get().general.defaultSingingLanguage;
                auto project = m_runtime.project().getProject(documentId(arguments));
                if (!project)
                    return AutomationResult<QJsonObject>(project.getError());
                for (const auto &track : project.get().tracks) {
                    for (const auto &clip : track.clips) {
                        if (clip.id != clipId)
                            continue;
                        if (!clip.data.defaultLanguage.isEmpty())
                            defaultLanguage = clip.data.defaultLanguage;
                        else if (clip.data.usesTrackVoiceContext) {
                            if (!track.data.defaultLanguage.isEmpty())
                                defaultLanguage = track.data.defaultLanguage;
                            else if (!track.data.singerInfo.defaultLanguage().isEmpty())
                                defaultLanguage = track.data.singerInfo.defaultLanguage();
                        } else if (!clip.data.ownSingerInfo.defaultLanguage().isEmpty()) {
                            defaultLanguage = clip.data.ownSingerInfo.defaultLanguage();
                        }
                    }
                }
                if (settings)
                    defaultLyric = settings.get().general.defaultLyrics.value(defaultLanguage,
                                                                              QStringLiteral("la"));
                QList<NoteDraftDto> notes;
                for (const auto &value : arguments.value(QStringLiteral("notes")).toArray())
                    notes.append(decodeNote(value.toObject(), defaultLanguage, defaultLyric));
                return mutationResult(m_runtime.notes().insertNotes(
                    commandContext(arguments, invocation), clipId, notes));
            });
        addBinding(
            QStringLiteral("P2-TOOL-072"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.notes().removeNotes(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray())));
            });
        addBinding(QStringLiteral("P2-TOOL-073"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().moveNotes(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           arguments.value(QStringLiteral("delta_tick")).toInt(),
                           arguments.value(QStringLiteral("delta_key")).toInt()));
                   });
        addBinding(QStringLiteral("P2-TOOL-074"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().resizeNotesLeft(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           arguments.value(QStringLiteral("delta_tick")).toInt(), 1));
                   });
        addBinding(QStringLiteral("P2-TOOL-075"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().resizeNotesRight(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           arguments.value(QStringLiteral("delta_tick")).toInt(), 1));
                   });
        addBinding(QStringLiteral("P2-TOOL-076"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().splitNoteAt(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt()),
                           arguments.value(QStringLiteral("local_position")).toInt()));
                   });
        addBinding(QStringLiteral("P2-TOOL-077"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().quantizeNotes(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           arguments.value(QStringLiteral("quantize")).toInt(),
                           arguments.value(QStringLiteral("quantize_start")).toBool(),
                           arguments.value(QStringLiteral("quantize_length")).toBool()));
                   });
        addBinding(QStringLiteral("P2-TOOL-078"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().setLyric(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt()),
                           arguments.value(QStringLiteral("lyric")).toString()));
                   });
        addBinding(QStringLiteral("P2-TOOL-083"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       QList<int> offsets;
                       for (const auto &value :
                            arguments.value(QStringLiteral("offsets")).toArray())
                           offsets.append(value.toInt());
                       return mutationResult(m_runtime.notes().setPhonemeOffsets(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt()), offsets));
                   });
        addBinding(QStringLiteral("P2-TOOL-088"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       QList<CurveDraftDto> curves;
                       for (const auto &value : arguments.value(QStringLiteral("curves")).toArray())
                           curves.append(decodeCurve(value.toObject()));
                       return mutationResult(m_runtime.parameters().replaceParameter(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           parameterName(arguments.value(QStringLiteral("name")).toString()),
                           parameterType(arguments.value(QStringLiteral("layer")).toString()),
                           curves));
                   });
        addBinding(QStringLiteral("P2-TOOL-089"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       QList<int> values;
                       for (const auto &value : arguments.value(QStringLiteral("values")).toArray())
                           values.append(value.toInt());
                       return mutationResult(m_runtime.parameters().drawParameter(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           parameterName(arguments.value(QStringLiteral("name")).toString()),
                           parameterType(arguments.value(QStringLiteral("layer")).toString()),
                           arguments.value(QStringLiteral("local_start")).toInt(),
                           arguments.value(QStringLiteral("step")).toInt(), std::move(values),
                           arguments.value(QStringLiteral("merge_mode")).toString() ==
                               QStringLiteral("overlay")));
                   });
        addBinding(QStringLiteral("P2-TOOL-090"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().eraseParameter(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           parameterName(arguments.value(QStringLiteral("name")).toString()),
                           parameterType(arguments.value(QStringLiteral("layer")).toString()),
                           arguments.value(QStringLiteral("local_start")).toInt(),
                           arguments.value(QStringLiteral("local_end")).toInt()));
                   });
        addBinding(
            QStringLiteral("P2-TOOL-092"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto curveId = arguments.contains(QStringLiteral("curve_id"))
                                         ? std::optional<CurveId>(CurveId(
                                               arguments.value(QStringLiteral("curve_id")).toInt()))
                                         : std::nullopt;
                QList<AnchorInsertDto> anchors;
                for (const auto &value : arguments.value(QStringLiteral("anchors")).toArray()) {
                    const auto anchor = value.toObject();
                    anchors.append({anchor.value(QStringLiteral("position")).toInt(),
                                    anchor.value(QStringLiteral("value")).toInt(),
                                    interpolation(anchor.value(QStringLiteral("interpolation"))
                                                      .toString(QStringLiteral("hermite")))});
                }
                return mutationResult(m_runtime.parameters().insertAnchors(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()), curveId,
                    anchors));
            });
        addBinding(
            QStringLiteral("P2-TOOL-093"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QList<AnchorMoveDto> moves;
                for (const auto &value : arguments.value(QStringLiteral("moves")).toArray()) {
                    const auto move = value.toObject();
                    moves.append({AnchorId(move.value(QStringLiteral("anchor_id")).toInt()),
                                  move.value(QStringLiteral("position")).toInt(),
                                  move.value(QStringLiteral("value")).toInt()});
                }
                return mutationResult(m_runtime.parameters().moveAnchors(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()), moves));
            });
        addBinding(
            QStringLiteral("P2-TOOL-094"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.parameters().removeAnchors(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()),
                    objectIds<AnchorId>(arguments.value(QStringLiteral("anchor_ids")).toArray())));
            });
        addBinding(
            QStringLiteral("P2-TOOL-095"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.parameters().setAnchorInterpolations(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()),
                    objectIds<AnchorId>(arguments.value(QStringLiteral("anchor_ids")).toArray()),
                    interpolation(arguments.value(QStringLiteral("interpolation")).toString())));
            });
        addBinding(
            QStringLiteral("P2-TOOL-091"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.parameters().bakeParameter(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    arguments.contains(QStringLiteral("local_start"))
                        ? std::optional<int>(arguments.value(QStringLiteral("local_start")).toInt())
                        : std::nullopt,
                    arguments.contains(QStringLiteral("local_end"))
                        ? std::optional<int>(arguments.value(QStringLiteral("local_end")).toInt())
                        : std::nullopt));
            });
        addBinding(QStringLiteral("P2-TOOL-029"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto voice = resolveVoiceSelection(
                           m_runtime, arguments.value(QStringLiteral("voice")).toObject(),
                           QStringLiteral("voice"));
                       if (!voice)
                           return AutomationResult<QJsonObject>(voice.getError());
                       return mutationResult(m_runtime.parameters().selectTrackSingleSpeaker(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           voice.get().first, voice.get().second));
                   });
        addBinding(QStringLiteral("P2-TOOL-060"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto mix = resolveSpeakerMix(
                           m_runtime, arguments.value(QStringLiteral("mix")).toObject(),
                           QStringLiteral("mix"));
                       if (!mix)
                           return AutomationResult<QJsonObject>(mix.getError());
                       const auto target = decodeSpeakerMixTarget(
                           arguments.value(QStringLiteral("target")).toObject());
                       const auto speaker = mix.get().second.sources.isEmpty()
                                                ? SpeakerInfo{}
                                                : mix.get().second.sources.constFirst().speaker;
                       return mutationResult(m_runtime.parameters().setFixedSpeakerMix(
                           commandContext(arguments, invocation), target, mix.get().first, speaker,
                           mix.get().second));
                   });
        addBinding(QStringLiteral("P2-TOOL-059"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.parameters().getSpeakerMix(
                documentId(arguments),
                decodeSpeakerMixTarget(arguments.value(QStringLiteral("target")).toObject()));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            if (result.get().singer.isEmpty() || result.get().speaker.isEmpty()) {
                return AutomationResult<QJsonObject>(
                    error(AutomationErrorCode::NotFound,
                          QStringLiteral("The target does not have an assigned voice"),
                          QStringLiteral("target")));
            }
            return AutomationResult<QJsonObject>(queryResult(
                result.get().document, QStringLiteral("snapshot"), encodeSpeakerMix(result.get())));
        });
        addBinding(QStringLiteral("P2-TOOL-049"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().useTrackVoiceContext(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
                   });
        addBinding(QStringLiteral("P2-TOOL-050"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto voice = resolveVoiceSelection(
                           m_runtime, arguments.value(QStringLiteral("voice")).toObject(),
                           QStringLiteral("voice"));
                       if (!voice)
                           return AutomationResult<QJsonObject>(voice.getError());
                       return mutationResult(m_runtime.parameters().selectClipSingleSpeaker(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           voice.get().first, voice.get().second));
                   });
        addBinding(QStringLiteral("P2-TOOL-061"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().enableClipDynamicSpeakerMix(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
                   });
        addBinding(QStringLiteral("P2-TOOL-062"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().disableClipDynamicSpeakerMix(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
                   });
        addBinding(QStringLiteral("P2-TOOL-063"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(
                           m_runtime.parameters().setClipDynamicSpeakerMixBypassed(
                               commandContext(arguments, invocation),
                               ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                               arguments.value(QStringLiteral("bypassed")).toBool()));
                   });
        addBinding(QStringLiteral("P2-TOOL-097"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setTempo(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("tick")).toInt(),
                           arguments.value(QStringLiteral("tempo")).toDouble()));
                   });
        addBinding(QStringLiteral("P2-TOOL-098"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().deleteTempo(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("tick")).toInt()));
                   });
        addBinding(QStringLiteral("P2-TOOL-099"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setTimeSignature(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("bar_index")).toInt(),
                           arguments.value(QStringLiteral("numerator")).toInt(),
                           arguments.value(QStringLiteral("denominator")).toInt()));
                   });
        addBinding(QStringLiteral("P2-TOOL-100"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().deleteTimeSignature(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("bar_index")).toInt()));
                   });
        addBinding(QStringLiteral("P2-TOOL-031"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto result = m_runtime.timeline().getMaster(documentId(arguments));
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(
                           queryResult(m_runtime.documentVersion(), QStringLiteral("snapshot"),
                                       QJsonObject{
                                           {QStringLiteral("gain"),               result.get().gain()},
                                           {QStringLiteral("pan"),                result.get().pan() },
                                           {QStringLiteral("mute"),               result.get().mute()},
                                           {QStringLiteral("solo"),               result.get().solo()},
                                           {QStringLiteral("metering_available"), false              }
                       }));
                   });
        addBinding(QStringLiteral("P2-TOOL-102"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(
                           m_runtime.history().undo(commandContext(arguments, invocation)));
                   });
        addBinding(QStringLiteral("P2-TOOL-103"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(
                           m_runtime.history().redo(commandContext(arguments, invocation)));
                   });
        addBinding(QStringLiteral("P2-TOOL-005"),
                   [this](const QJsonObject &, const PublicInvocationContext &) {
                       const auto snapshot = m_fileGuard.snapshot();
                       QJsonArray readRoots;
                       for (const auto &path : snapshot.readRoots)
                           readRoots.append(path);
                       QJsonArray writeRoots;
                       for (const auto &path : snapshot.writeRoots)
                           writeRoots.append(path);
                       QJsonArray grants;
                       for (const auto &path : snapshot.sessionReadGrants) {
                           grants.append(QJsonObject{
                               {QStringLiteral("path"),   path                  },
                               {QStringLiteral("access"), QStringLiteral("read")}
                           });
                       }
                       for (const auto &path : snapshot.sessionWriteGrants) {
                           grants.append(QJsonObject{
                               {QStringLiteral("path"),   path                   },
                               {QStringLiteral("access"), QStringLiteral("write")}
                           });
                       }
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("read_roots"),     readRoots },
                           {QStringLiteral("write_roots"),    writeRoots},
                           {QStringLiteral("session_grants"), grants    },
                       });
                   });
        addBinding(
            QStringLiteral("P2-TOOL-008"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto context = replacementCommandContext(m_runtime, arguments, invocation);
                auto current = m_runtime.documents().getDocument(context.expected.documentId);
                if (!current)
                    return AutomationResult<QJsonObject>(current.getError());
                if (!current.get().saved &&
                    arguments.value(QStringLiteral("unsaved_policy")).toString() ==
                        QStringLiteral("reject")) {
                    return AutomationResult<QJsonObject>(
                        error(AutomationErrorCode::Busy,
                              QStringLiteral("The current document has unsaved changes"),
                              QStringLiteral("unsaved_policy")));
                }
                const auto document = DocumentAutomationFacade::newDocumentDraft(
                    arguments.value(QStringLiteral("template")).toString() ==
                    QStringLiteral("default"));
                auto committed = m_runtime.documents().commitNewDocument(context, document);
                if (!committed)
                    return AutomationResult<QJsonObject>(committed.getError());
                QString path;
                bool dirty = false;
                if (!committed.get().validatedOnly) {
                    auto current =
                        m_runtime.documents().getDocument(committed.get().current.documentId);
                    if (current) {
                        path = current.get().path;
                        dirty = !current.get().saved;
                    }
                }
                return AutomationResult<QJsonObject>(
                    encodeDocumentLifecycleResult(committed.get(), path, dirty));
            });
        addBinding(QStringLiteral("P2-TOOL-009"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       if (!m_hostServices.openDocument)
                           return AutomationResult<QJsonObject>(
                               unavailable(QStringLiteral("Document open service is unavailable")));
                       PublicDocumentOpenRequest request{
                           replacementCommandContext(m_runtime, arguments, invocation),
                           arguments.value(QStringLiteral("path")).toString(),
                           arguments.value(QStringLiteral("unsaved_policy")).toString() ==
                                   QStringLiteral("discard")
                               ? PublicUnsavedPolicy::Discard
                               : PublicUnsavedPolicy::Reject,
                       };
                       return taskAcceptedResult(m_hostServices.openDocument(request));
                   });
        addBinding(QStringLiteral("P2-TOOL-012"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       if (!m_hostServices.importDocument)
                           return AutomationResult<QJsonObject>(unavailable(
                               QStringLiteral("Document import service is unavailable")));
                       PublicDocumentImportRequest request{
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("path")).toString(),
                           arguments.value(QStringLiteral("options"))
                               .toObject()
                               .value(QStringLiteral("import_tempo"))
                               .toBool(true),
                           arguments.value(QStringLiteral("options"))
                               .toObject()
                               .value(QStringLiteral("import_time_signatures"))
                               .toBool(true),
                           QStringLiteral("append"),
                       };
                       return taskAcceptedResult(m_hostServices.importDocument(request));
                   });
        addBinding(
            QStringLiteral("P2-TOOL-010"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                auto reauthorized = m_fileGuard.reauthorize(
                    {arguments.value(QStringLiteral("path")).toString(), FileAccessPurpose::Write});
                if (!reauthorized)
                    return AutomationResult<QJsonObject>(reauthorized.getError());
                return mutationResult(m_runtime.documents().saveDocument(
                    commandContext(arguments, invocation),
                    arguments.value(QStringLiteral("path")).toString(),
                    arguments.value(QStringLiteral("overwrite_policy")).toString() ==
                        QStringLiteral("overwrite")));
            });
        addBinding(QStringLiteral("P2-TOOL-014"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.files().listFormats();
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            QJsonArray formats;
            for (const auto &format : result.get()) {
                const auto purpose = arguments.value(QStringLiteral("purpose")).toString();
                if ((purpose == QStringLiteral("open") && !format.canOpen) ||
                    (purpose == QStringLiteral("import") && !format.canImport) ||
                    (purpose == QStringLiteral("export") && !format.canExport)) {
                    continue;
                }
                if (format.optionSchema.isEmpty() ||
                    !AutomationWire::checkJsonSchema(format.optionSchema).valid()) {
                    return AutomationResult<QJsonObject>(
                        error(AutomationErrorCode::InternalError,
                              QStringLiteral("Project format option schema is unavailable")));
                }
                QJsonArray extensions;
                for (const auto &extension : format.extensions)
                    extensions.append(extension);
                formats.append(QJsonObject{
                    {QStringLiteral("id"),                 format.id               },
                    {QStringLiteral("display_name"),       format.displayName      },
                    {QStringLiteral("extensions"),         extensions              },
                    {QStringLiteral("can_open"),           format.canOpen          },
                    {QStringLiteral("can_import"),         format.canImport        },
                    {QStringLiteral("can_export"),         format.canExport        },
                    {QStringLiteral("available"),          format.available        },
                    {QStringLiteral("unavailable_reason"), format.unavailableReason},
                    {QStringLiteral("option_schema"),      format.optionSchema     },
                });
            }
            return AutomationResult<QJsonObject>(QJsonObject{
                {QStringLiteral("formats"), formats}
            });
        });
        addBinding(QStringLiteral("P2-TOOL-053"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       if (!m_hostServices.importAudioClip)
                           return AutomationResult<QJsonObject>(
                               unavailable(QStringLiteral("Audio import service is unavailable")));
                       PublicAudioClipProperties properties;
                       properties.start = arguments.value(QStringLiteral("start")).toInt();
                       if (arguments.contains(QStringLiteral("name")))
                           properties.name = arguments.value(QStringLiteral("name")).toString();
                       if (arguments.contains(QStringLiteral("gain")))
                           properties.gain = arguments.value(QStringLiteral("gain")).toDouble();
                       if (arguments.contains(QStringLiteral("mute")))
                           properties.mute = arguments.value(QStringLiteral("mute")).toBool();
                       return taskAcceptedResult(m_hostServices.importAudioClip(
                           {commandContext(arguments, invocation),
                            TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                            arguments.value(QStringLiteral("path")).toString(),
                            properties,
                            {}}));
                   });
        addBinding(QStringLiteral("P2-TOOL-054"), [this](
                                                      const QJsonObject &arguments,
                                                      const PublicInvocationContext &invocation) {
            if (!m_hostServices.importAudioClips)
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Audio batch import service is unavailable")));
            PublicAudioClipBatchImportRequest request;
            request.command = commandContext(arguments, invocation);
            request.failurePolicy = arguments.value(QStringLiteral("failure_policy")).toString() ==
                                            QStringLiteral("best_effort")
                                        ? PublicBatchFailurePolicy::BestEffort
                                        : PublicBatchFailurePolicy::Atomic;
            for (const auto &value : arguments.value(QStringLiteral("items")).toArray()) {
                const auto item = value.toObject();
                request.items.append({
                    TrackId(item.value(QStringLiteral("track_id")).toInt()),
                    item.value(QStringLiteral("path")).toString(),
                    PublicAudioClipProperties{
                                              .name = item.contains(QStringLiteral("name"))
                                    ? std::optional<QString>(
                                          item.value(QStringLiteral("name")).toString())
                                    : std::nullopt,
                                              .start = item.value(QStringLiteral("start")).toInt(),
                                              .gain = item.contains(QStringLiteral("gain"))
                                    ? std::optional<double>(
                                          item.value(QStringLiteral("gain")).toDouble())
                                    : std::nullopt,
                                              .mute =
                            item.contains(QStringLiteral("mute"))
                                ? std::optional<bool>(item.value(QStringLiteral("mute")).toBool())
                                : std::nullopt,
                                              },
                    {},
                });
            }
            return taskAcceptedResult(m_hostServices.importAudioClips(request));
        });
        addBinding(
            QStringLiteral("P2-TOOL-055"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto path = arguments.value(QStringLiteral("path")).toString();
                auto prepared = prepareAuthorizedAudioPath(m_hostServices, m_fileGuard, path);
                if (!prepared)
                    return AutomationResult<QJsonObject>(prepared.getError());
                return completedTaskResult(
                    m_runtime, OperationIds::audio_clips::relocate, invocation.clientId,
                    m_runtime.project().relocateAudioClip(
                        commandContext(arguments, invocation),
                        ClipId(arguments.value(QStringLiteral("clip_id")).toInt()), path,
                        AudioPathInfo{{}, prepared.get().sha512}, prepared.get().formatData));
            });
        addBinding(
            QStringLiteral("P2-TOOL-056"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto path = arguments.value(QStringLiteral("path")).toString();
                if (path.isEmpty()) {
                    return completedTaskResult(
                        m_runtime, OperationIds::audio_clips::confirm_path, invocation.clientId,
                        m_runtime.project().confirmAudioClipPath(
                            commandContext(arguments, invocation),
                            ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
                }
                auto prepared = prepareAuthorizedAudioPath(m_hostServices, m_fileGuard, path);
                if (!prepared)
                    return AutomationResult<QJsonObject>(prepared.getError());
                return completedTaskResult(
                    m_runtime, OperationIds::audio_clips::confirm_path, invocation.clientId,
                    m_runtime.project().confirmAudioClipPath(
                        commandContext(arguments, invocation),
                        ClipId(arguments.value(QStringLiteral("clip_id")).toInt()), path,
                        AudioPathInfo{{}, prepared.get().sha512}, prepared.get().formatData));
            });
        addBinding(
            QStringLiteral("P2-TOOL-114"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto context = commandContext(arguments, invocation);
                auto current = m_runtime.documents().getDocument(context.expected.documentId);
                if (!current)
                    return AutomationResult<QJsonObject>(current.getError());
                if (current.get().document.revision != context.expected.revision) {
                    return AutomationResult<QJsonObject>(AutomationError::revisionConflict(
                        context.expected.documentId, context.expected.revision,
                        current.get().document.revision));
                }
                const auto path = arguments.value(QStringLiteral("path")).toString();
                const auto allowOverwrite =
                    arguments.value(QStringLiteral("overwrite_policy")).toString() ==
                    QStringLiteral("overwrite");
                const auto encodedOptions = arguments.value(QStringLiteral("options")).toObject();
                MidiExportOptionsDto options;
                options.includeTempo =
                    encodedOptions.value(QStringLiteral("include_tempo")).toBool(true);
                options.includeTimeSignatures =
                    encodedOptions.value(QStringLiteral("include_time_signatures")).toBool(true);
                if (context.validateOnly) {
                    auto validated =
                        m_runtime.files().prepareMidiExport(context, path, allowOverwrite, options);
                    if (!validated)
                        return AutomationResult<QJsonObject>(validated.getError());
                    return AutomationResult<QJsonObject>(
                        encodeTaskAccepted({{}, context.expected, true}));
                }
                auto prepared =
                    m_runtime.files().prepareMidiExport(context, path, allowOverwrite, options);
                if (!prepared)
                    return AutomationResult<QJsonObject>(prepared.getError());
                auto snapshot = m_runtime.automationTasks().createTask(
                    OperationIds::exports::midi::start, context.expected, std::nullopt, {},
                    invocation.clientId);
                const auto taskId = snapshot.taskId;
                auto *tasks = &m_runtime.automationTasks();
                auto *files = &m_runtime.files();
                auto *fileGuard = &m_fileGuard;
                const AuthorizedPath authorizedPath{path, FileAccessPurpose::Write};
                auto execute = [tasks, files, fileGuard, authorizedPath, taskId, context,
                                prepared = prepared.get()] {
                    if (!tasks->markRunning(taskId)) {
                        tasks->beginCommitting(taskId);
                        return;
                    }
                    auto authorized = fileGuard->reauthorize(authorizedPath);
                    if (!authorized) {
                        auto error = authorized.getError();
                        error.taskId = taskId;
                        tasks->fail(taskId, std::move(error));
                        return;
                    }
                    const auto committing = tasks->beginCommitting(taskId);
                    if (!committing || !committing.get())
                        return;
                    auto exported = files->writePreparedMidiExport(prepared);
                    if (!exported) {
                        tasks->fail(taskId, exported.getError());
                        return;
                    }
                    authorized = fileGuard->reauthorize(authorizedPath);
                    if (!authorized) {
                        auto error = authorized.getError();
                        error.taskId = taskId;
                        tasks->fail(taskId, std::move(error));
                        return;
                    }
                    MutationResult completed;
                    completed.previous = context.expected;
                    completed.current = context.expected;
                    completed.validatedOnly = context.validateOnly;
                    tasks->succeed(taskId, completed);
                };
                QThreadPool::globalInstance()->start(std::move(execute));
                return AutomationResult<QJsonObject>(
                    encodeTaskAccepted({snapshot.taskId, context.expected, context.validateOnly}));
            });
        addBinding(QStringLiteral("P2-TOOL-115"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            if (!m_hostServices.audioExportCapabilities)
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Audio export capability service is unavailable")));
            auto capabilities = m_hostServices.audioExportCapabilities(documentId(arguments));
            if (!capabilities)
                return AutomationResult<QJsonObject>(capabilities.getError());
            return AutomationResult<QJsonObject>(queryResult(
                m_runtime.documentVersion(), QStringLiteral("capabilities"), capabilities.get()));
        });
        addBinding(QStringLiteral("P2-TOOL-116"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            const auto options = arguments.value(QStringLiteral("options")).toObject();
            if (options.contains(QStringLiteral("range"))) {
                return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                    QStringLiteral("options.range"),
                    QStringLiteral(
                        "The active audio exporter does not support an arbitrary time range")));
            }
            const auto document = documentId(arguments);
            const auto previewPath = arguments.value(QStringLiteral("path")).toString();
            auto config = resolveAudioExportSources(m_runtime, document,
                                                    decodeAudioExportConfig(options, previewPath));
            if (!config)
                return AutomationResult<QJsonObject>(config.getError());
            auto result = m_runtime.audioExports().preview(document, config.get());
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            auto encoded = encodeAudioExportPreview(result.get());
            if (!encoded)
                return AutomationResult<QJsonObject>(encoded.getError());
            const auto raw = encoded.get();
            QJsonArray targets;
            for (const auto &value : raw.value(QStringLiteral("plan"))
                                         .toObject()
                                         .value(QStringLiteral("targets"))
                                         .toArray()) {
                targets.append(value.toObject().value(QStringLiteral("path")).toString());
            }
            QJsonArray diagnostics;
            for (const auto &value : raw.value(QStringLiteral("diagnostics")).toArray()) {
                const auto diagnostic = value.toObject();
                diagnostics.append(QJsonObject{
                    {QStringLiteral("code"),     diagnostic.value(QStringLiteral("code"))    },
                    {QStringLiteral("message"),  diagnostic.value(QStringLiteral("message")) },
                    {QStringLiteral("blocking"), diagnostic.value(QStringLiteral("blocking"))},
                });
            }
            const QJsonObject plan{
                {QStringLiteral("targets"),     targets                          },
                {QStringLiteral("diagnostics"), diagnostics                      },
                {QStringLiteral("plan_digest"), AutomationWire::sha256Digest(raw)}
            };
            return AutomationResult<QJsonObject>(
                queryResult(m_runtime.documentVersion(), QStringLiteral("plan"), plan));
        });
        addBinding(
            QStringLiteral("P2-TOOL-117"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto options = arguments.value(QStringLiteral("options")).toObject();
                if (options.contains(QStringLiteral("range"))) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("options.range"),
                        QStringLiteral(
                            "The active audio exporter does not support an arbitrary time range")));
                }
                AudioExportPolicyDto policy;
                policy.allowOverwrite =
                    arguments.value(QStringLiteral("overwrite_policy")).toString() ==
                    QStringLiteral("overwrite");
                auto config = resolveAudioExportSources(
                    m_runtime, documentId(arguments),
                    decodeAudioExportConfig(options,
                                            arguments.value(QStringLiteral("path")).toString()));
                if (!config)
                    return AutomationResult<QJsonObject>(config.getError());
                const AuthorizedPath authorizedPath{
                    arguments.value(QStringLiteral("path")).toString(), FileAccessPurpose::Write};
                auto derivedPaths = std::make_shared<QList<AuthorizedPath>>();
                return taskAcceptedResult(m_runtime.audioExports().start(
                    commandContext(arguments, invocation), config.get(), policy, {},
                    [guard = &m_fileGuard, derivedPaths](
                        const AudioExportPreviewDto &preview) -> AutomationResult<AutomationUnit> {
                        QList<AuthorizedPath> authorizedPaths;
                        authorizedPaths.reserve(preview.filePaths.size());
                        for (const auto &path : preview.filePaths) {
                            auto authorized = guard->authorize(path, FileAccessPurpose::Write);
                            if (!authorized)
                                return authorized.getError();
                            authorizedPaths.append(authorized.get());
                        }
                        *derivedPaths = std::move(authorizedPaths);
                        return AutomationUnit{};
                    },
                    [guard = &m_fileGuard, authorizedPath, derivedPaths] {
                        auto authorized = guard->reauthorize(authorizedPath);
                        if (!authorized)
                            return AutomationResult<AutomationUnit>(authorized.getError());
                        for (const auto &path : std::as_const(*derivedPaths)) {
                            authorized = guard->reauthorize(path);
                            if (!authorized)
                                return AutomationResult<AutomationUnit>(authorized.getError());
                        }
                        return AutomationResult<AutomationUnit>(AutomationUnit{});
                    }));
            });
        addBinding(QStringLiteral("P2-TOOL-118"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            const auto clipId =
                ClipId(arguments.value(QStringLiteral("source_audio_clip_id")).toInt());
            if (!m_hostServices.extractionCapabilities)
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Extraction capability service is unavailable")));
            auto capabilities =
                m_hostServices.extractionCapabilities(documentId(arguments), clipId);
            if (!capabilities)
                return AutomationResult<QJsonObject>(capabilities.getError());
            return AutomationResult<QJsonObject>(queryResult(
                m_runtime.documentVersion(), QStringLiteral("capabilities"), capabilities.get()));
        });
        addBinding(
            QStringLiteral("P2-TOOL-119"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto options = arguments.value(QStringLiteral("options")).toObject();
                PitchExtractionOptionsDto extractionOptions;
                extractionOptions.modelId = options.value(QStringLiteral("model_id")).toString();
                extractionOptions.authorizeSource = sourcePathAuthorizer(m_fileGuard);
                return taskAcceptedResult(m_runtime.extractions().startPitch(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("source_audio_clip_id")).toInt()),
                    ClipId(arguments.value(QStringLiteral("target_singing_clip_id")).toInt()),
                    std::move(extractionOptions)));
            });
        addBinding(
            QStringLiteral("P2-TOOL-120"),
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto options = arguments.value(QStringLiteral("options")).toObject();
                MidiExtractionOptionsDto extractionOptions;
                extractionOptions.modelId = options.value(QStringLiteral("model_id")).toString();
                extractionOptions.authorizeSource = sourcePathAuthorizer(m_fileGuard);
                extractionOptions.defaultLanguage =
                    options.value(QStringLiteral("default_language")).toString();
                extractionOptions.defaultLyric =
                    options.value(QStringLiteral("default_lyric")).toString();
                extractionOptions.clientRef =
                    options.value(QStringLiteral("client_ref")).toString();
                if (options.contains(QStringLiteral("minimum_note_length")))
                    extractionOptions.minimumNoteLength =
                        options.value(QStringLiteral("minimum_note_length")).toInt();
                const auto destination = arguments.value(QStringLiteral("destination")).toObject();
                extractionOptions.destinationMode =
                    destination.value(QStringLiteral("mode")).toString();
                extractionOptions.targetTrackId =
                    TrackId(destination.value(QStringLiteral("target_track_id")).toInt());
                if (destination.contains(QStringLiteral("target_clip_id"))) {
                    extractionOptions.targetClipId =
                        ClipId(destination.value(QStringLiteral("target_clip_id")).toInt());
                }
                extractionOptions.targetStart = destination.value(QStringLiteral("start")).toInt();
                return taskAcceptedResult(m_runtime.extractions().startMidi(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("source_audio_clip_id")).toInt()),
                    std::move(extractionOptions)));
            });
        addBinding(QStringLiteral("P2-TOOL-121"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            const auto scope = arguments.value(QStringLiteral("scope")).toObject();
            if (!m_hostServices.inferenceCapabilities)
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Inference capability service is unavailable")));
            auto capabilities = m_hostServices.inferenceCapabilities(documentId(arguments), scope);
            if (!capabilities)
                return AutomationResult<QJsonObject>(capabilities.getError());
            return AutomationResult<QJsonObject>(queryResult(
                m_runtime.documentVersion(), QStringLiteral("capabilities"), capabilities.get()));
        });
        addBinding(QStringLiteral("P2-TOOL-123"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       if (!m_hostServices.startInference)
                           return AutomationResult<QJsonObject>(unavailable(
                               QStringLiteral("Inference orchestration service is unavailable")));
                       PublicInferenceStartRequest request;
                       request.command = commandContext(arguments, invocation);
                       request.scope = arguments.value(QStringLiteral("scope")).toObject();
                       request.options = arguments.value(QStringLiteral("options")).toObject();
                       for (const auto &value : arguments.value(QStringLiteral("stages")).toArray())
                           request.stages.append(value.toString());
                       if (request.stages.isEmpty())
                           request.stages = InferenceAutomationFacade::supportedStages();
                       return taskAcceptedResult(m_hostServices.startInference(request));
                   });
        addBinding(QStringLiteral("P2-TOOL-124"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       if (!m_hostServices.resetInferenceStage)
                           return AutomationResult<QJsonObject>(unavailable(QStringLiteral(
                               "Inference reset orchestration service is unavailable")));
                       return mutationResult(m_hostServices.resetInferenceStage({
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("scope")).toObject(),
                           arguments.value(QStringLiteral("stage")).toString(),
                       }));
                   });
        addBinding(QStringLiteral("P2-TOOL-125"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto tasks = m_runtime.tasks().listTasks(documentId(arguments));
            if (!tasks)
                return AutomationResult<QJsonObject>(tasks.getError());
            auto filtered = tasks.get();
            const auto stateFilter = arguments.value(QStringLiteral("state")).toString();
            const auto kindFilter = arguments.value(QStringLiteral("kind")).toString();
            filtered.removeIf([&](const AutomationTaskSnapshot &task) {
                return (!stateFilter.isEmpty() && taskStateName(task.state) != stateFilter) ||
                       (!kindFilter.isEmpty() && task.operationId != kindFilter);
            });
            QJsonArray snapshot;
            for (const auto &task : std::as_const(filtered))
                snapshot.append(encodeTaskSnapshot(task));
            QString digestError;
            const auto snapshotDigest = AutomationWire::sha256Digest(
                QJsonObject{
                    {QStringLiteral("document"),
                     encodeDocumentVersion(m_runtime.documentVersion())},
                    {QStringLiteral("state"),    stateFilter           },
                    {QStringLiteral("kind"),     kindFilter            },
                    {QStringLiteral("tasks"),    snapshot              },
            },
                &digestError);
            if (!digestError.isEmpty()) {
                return AutomationResult<QJsonObject>(
                    error(AutomationErrorCode::InternalError,
                          QStringLiteral("Task cursor snapshot could not be encoded")));
            }
            qint64 offset = 0;
            const auto cursorText = arguments.value(QStringLiteral("cursor")).toString();
            if (!cursorText.isEmpty()) {
                const auto parsed = m_taskCursorCodec.parse(
                    cursorText, QStringLiteral("editor-public-tasks-list/v1"), snapshotDigest);
                if (!parsed.valid()) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("cursor"), QStringLiteral("Task cursor is invalid")));
                }
                offset = *parsed.offset;
            }
            if (offset < 0 || offset > filtered.size()) {
                return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                    QStringLiteral("cursor"), QStringLiteral("Task cursor is invalid")));
            }
            const auto limit = arguments.value(QStringLiteral("limit")).toInt(filtered.size());
            QJsonArray resultItems;
            const auto end = std::min<qint64>(offset + limit, filtered.size());
            for (auto index = offset; index < end; ++index)
                resultItems.append(encodeTaskSnapshot(filtered.at(static_cast<qsizetype>(index))));
            QJsonObject result =
                queryResult(m_runtime.documentVersion(), QStringLiteral("tasks"), resultItems);
            if (end < filtered.size()) {
                const auto nextCursor = m_taskCursorCodec.issue(
                    QStringLiteral("editor-public-tasks-list/v1"), snapshotDigest, end);
                if (nextCursor.isEmpty()) {
                    return AutomationResult<QJsonObject>(
                        error(AutomationErrorCode::InternalError,
                              QStringLiteral("Task cursor could not be issued"),
                              QStringLiteral("next_cursor")));
                }
                result.insert(QStringLiteral("next_cursor"), nextCursor);
            }
            return AutomationResult<QJsonObject>(std::move(result));
        });
        addBinding(QStringLiteral("P2-TOOL-126"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.tasks().getTask(
                documentId(arguments),
                TaskId::fromString(arguments.value(QStringLiteral("task_id")).toString()));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            return AutomationResult<QJsonObject>(encodeTaskSnapshot(result.get()));
        });
        addBinding(QStringLiteral("P2-TOOL-127"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto context = commandContext(arguments, invocation);
                       context.expected.documentId = documentId(arguments);
                       auto result = m_runtime.tasks().cancelTask(
                           context, TaskId::fromString(
                                        arguments.value(QStringLiteral("task_id")).toString()));
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(encodeTaskSnapshot(result.get()));
                   });
        addBinding(QStringLiteral("P2-TOOL-104"), [this](const QJsonObject &arguments,
                                                         const PublicInvocationContext &) {
            auto result = m_runtime.playback().getPlayback(documentId(arguments));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            return AutomationResult<QJsonObject>(queryResult(
                result.get().document, QStringLiteral("snapshot"), encodePlayback(result.get())));
        });
        addBinding(QStringLiteral("P2-TOOL-105"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackMutationResult(
                           m_runtime, m_runtime.playback().play(playbackCommandContext(
                                          m_runtime, arguments, invocation)));
                   });
        addBinding(QStringLiteral("P2-TOOL-106"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackMutationResult(
                           m_runtime, m_runtime.playback().pause(playbackCommandContext(
                                          m_runtime, arguments, invocation)));
                   });
        addBinding(QStringLiteral("P2-TOOL-107"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackMutationResult(
                           m_runtime, m_runtime.playback().stop(playbackCommandContext(
                                          m_runtime, arguments, invocation)));
                   });
        addBinding(QStringLiteral("P2-TOOL-108"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackMutationResult(
                           m_runtime, m_runtime.playback().seek(
                                          playbackCommandContext(m_runtime, arguments, invocation),
                                          arguments.value(QStringLiteral("position")).toDouble()));
                   });
        addBinding(QStringLiteral("P2-TOOL-032"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setMasterGain(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("gain")).toDouble()));
                   });
        addBinding(QStringLiteral("P2-TOOL-109"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       const auto start = arguments.value(QStringLiteral("start")).toDouble();
                       const auto end = arguments.value(QStringLiteral("end")).toDouble();
                       return playbackMutationResult(
                           m_runtime, m_runtime.playback().setLoop(
                                          playbackCommandContext(m_runtime, arguments, invocation),
                                          LoopSettings(true, static_cast<int>(start),
                                                       static_cast<int>(end - start))));
                   });
        addBinding(QStringLiteral("P2-TOOL-110"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackMutationResult(
                           m_runtime, m_runtime.playback().setLoopEnabled(
                                          playbackCommandContext(m_runtime, arguments, invocation),
                                          arguments.value(QStringLiteral("enabled")).toBool()));
                   });
        addBinding(QStringLiteral("P2-TOOL-111"),
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackMutationResult(
                           m_runtime, m_runtime.playback().clearLoop(playbackCommandContext(
                                          m_runtime, arguments, invocation)));
                   });

        addOperationBinding(
            OperationIds::documents::save_as,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                auto path = arguments.value(QStringLiteral("path")).toString();
                const auto extensionPolicy =
                    arguments.value(QStringLiteral("extension_policy")).toString();
                if ((extensionPolicy == QStringLiteral("append_if_missing") ||
                     extensionPolicy == QStringLiteral("replace")) &&
                    QFileInfo(path).suffix().compare(QStringLiteral("dspx"), Qt::CaseInsensitive) !=
                        0) {
                    if (extensionPolicy == QStringLiteral("replace")) {
                        QFileInfo info(path);
                        path = QDir(info.absolutePath())
                                   .filePath(info.completeBaseName() + QStringLiteral(".dspx"));
                    } else if (QFileInfo(path).suffix().isEmpty()) {
                        path += QStringLiteral(".dspx");
                    }
                }
                auto authorized = m_fileGuard.authorize(path, FileAccessPurpose::Write);
                if (!authorized)
                    return AutomationResult<QJsonObject>(authorized.getError());
                return mutationResult(m_runtime.documents().saveDocument(
                    commandContext(arguments, invocation), authorized.get().canonicalPath,
                    arguments.value(QStringLiteral("overwrite_policy")).toString() ==
                        QStringLiteral("overwrite")));
            });

        addOperationBinding(
            OperationIds::documents::import_batch,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                if (!m_hostServices.importDocuments) {
                    return AutomationResult<QJsonObject>(unavailable(
                        QStringLiteral("Document batch import service is unavailable")));
                }
                auto formats = m_runtime.files().listFormats();
                if (!formats)
                    return AutomationResult<QJsonObject>(formats.getError());
                PublicDocumentBatchImportRequest request;
                request.command = commandContext(arguments, invocation);
                request.failurePolicy =
                    arguments.value(QStringLiteral("failure_policy")).toString() ==
                            QStringLiteral("best_effort")
                        ? PublicBatchFailurePolicy::BestEffort
                        : PublicBatchFailurePolicy::Atomic;
                for (const auto &value : arguments.value(QStringLiteral("items")).toArray()) {
                    const auto item = value.toObject();
                    const auto path = item.value(QStringLiteral("path")).toString();
                    const auto suffix = QFileInfo(path).suffix();
                    PublicDocumentBatchImportItem requestItem{
                        path,
                        item.value(QStringLiteral("format_id")).toString(),
                        item.value(QStringLiteral("options")).toObject(),
                        item.value(QStringLiteral("plan_digest")).toString(),
                    };
                    const auto format = std::find_if(
                        formats.get().cbegin(), formats.get().cend(),
                        [&](const ProjectFormatDto &entry) {
                            return entry.canImport &&
                                   std::any_of(entry.extensions.cbegin(), entry.extensions.cend(),
                                               [&](QString extension) {
                                                   extension.remove(u'*');
                                                   extension.remove(u'.');
                                                   return extension.compare(
                                                              suffix, Qt::CaseInsensitive) == 0;
                                               });
                        });
                    if (format == formats.get().cend()) {
                        requestItem.validationError =
                            error(AutomationErrorCode::FormatUnsupported,
                                  QStringLiteral("No project format accepts an import item"),
                                  QStringLiteral("items.path"));
                        request.items.append(std::move(requestItem));
                        continue;
                    }
                    if (!requestItem.formatId.isEmpty() && requestItem.formatId != format->id) {
                        requestItem.validationError = AutomationError::invalidArgument(
                            QStringLiteral("items.format_id"),
                            QStringLiteral("The requested format does not match the import path"));
                        request.items.append(std::move(requestItem));
                        continue;
                    }
                    const QJsonObject plan{
                        {QStringLiteral("path"),               path                    },
                        {QStringLiteral("purpose"),            QStringLiteral("import")},
                        {QStringLiteral("format_id"),          format->id              },
                        {QStringLiteral("sources"),            QJsonArray{}            },
                        {QStringLiteral("encoding"),           QString()               },
                        {QStringLiteral("lyrics_preview"),     QJsonArray{}            },
                        {QStringLiteral("timeline_supported"), true                    },
                        {QStringLiteral("option_schema"),      format->optionSchema    },
                    };
                    if (!requestItem.planDigest.isEmpty() &&
                        requestItem.planDigest != AutomationWire::sha256Digest(plan)) {
                        requestItem.validationError = AutomationError::invalidArgument(
                            QStringLiteral("items.plan_digest"),
                            QStringLiteral("The import plan digest is stale or invalid"));
                        request.items.append(std::move(requestItem));
                        continue;
                    }
                    requestItem.formatId = format->id;
                    request.items.append(std::move(requestItem));
                }
                return taskAcceptedResult(m_hostServices.importDocuments(request));
            });

        addOperationBinding(OperationIds::formats::inspect, [this](
                                                                const QJsonObject &arguments,
                                                                const PublicInvocationContext &) {
            auto formats = m_runtime.files().listFormats();
            if (!formats)
                return AutomationResult<QJsonObject>(formats.getError());
            const auto path = arguments.value(QStringLiteral("path")).toString();
            const auto suffix = QFileInfo(path).suffix();
            const auto purpose = arguments.value(QStringLiteral("purpose")).toString();
            const auto found = std::find_if(
                formats.get().cbegin(), formats.get().cend(), [&](const ProjectFormatDto &format) {
                    const bool purposeSupported =
                        (purpose == QStringLiteral("open") && format.canOpen) ||
                        (purpose == QStringLiteral("import") && format.canImport) ||
                        (purpose == QStringLiteral("export") && format.canExport);
                    return purposeSupported &&
                           std::any_of(format.extensions.cbegin(), format.extensions.cend(),
                                       [&](QString extension) {
                                           extension.remove(u'*');
                                           extension.remove(u'.');
                                           return extension.compare(suffix, Qt::CaseInsensitive) ==
                                                  0;
                                       });
                });
            if (found == formats.get().cend()) {
                return AutomationResult<QJsonObject>(
                    error(AutomationErrorCode::FormatUnsupported,
                          QStringLiteral("No project format accepts the path for this purpose"),
                          QStringLiteral("path")));
            }
            QJsonObject result{
                {QStringLiteral("path"),               path               },
                {QStringLiteral("purpose"),            purpose            },
                {QStringLiteral("format_id"),          found->id          },
                {QStringLiteral("sources"),            QJsonArray{}       },
                {QStringLiteral("encoding"),           QString()          },
                {QStringLiteral("lyrics_preview"),     QJsonArray{}       },
                {QStringLiteral("timeline_supported"), true               },
                {QStringLiteral("option_schema"),      found->optionSchema},
            };
            result.insert(QStringLiteral("plan_digest"), AutomationWire::sha256Digest(result));
            return AutomationResult<QJsonObject>(std::move(result));
        });

        addOperationBinding(OperationIds::tracks::list, [this](const QJsonObject &arguments,
                                                               const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            QJsonArray tracks;
            for (qsizetype index = 0; index < project.get().tracks.size(); ++index)
                tracks.append(encodeTrackSnapshot(project.get().tracks.at(index), index));
            auto page = paginateJson(
                m_collectionCursorCodec, tracks, arguments,
                QStringLiteral("editor-public-tracks/v1"),
                QJsonObject{
                    {QStringLiteral("document"), encodeDocumentVersion(project.get().document)},
                    {QStringLiteral("tracks"),   tracks                                       }
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            auto result =
                queryResult(project.get().document, QStringLiteral("tracks"), page.get().items);
            if (!page.get().nextCursor.isEmpty())
                result.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            return AutomationResult<QJsonObject>(std::move(result));
        });

        addOperationBinding(OperationIds::tracks::get, [this](const QJsonObject &arguments,
                                                              const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            const auto trackId = TrackId(arguments.value(QStringLiteral("track_id")).toInt());
            for (qsizetype index = 0; index < project.get().tracks.size(); ++index) {
                const auto &track = project.get().tracks.at(index);
                if (track.id == trackId) {
                    return AutomationResult<QJsonObject>(
                        queryResult(project.get().document, QStringLiteral("snapshot"),
                                    encodeTrackSnapshot(track, index)));
                }
            }
            return AutomationResult<QJsonObject>(AutomationError::notFound(
                {ObjectKind::Track, trackId.value()}, QStringLiteral("Track was not found")));
        });

        addOperationBinding(
            OperationIds::tracks::set_gain,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().setTrackGain(
                    commandContext(arguments, invocation),
                    TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                    arguments.value(QStringLiteral("gain")).toDouble()));
            });
        addOperationBinding(
            OperationIds::tracks::set_pan,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().setTrackPan(
                    commandContext(arguments, invocation),
                    TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                    arguments.value(QStringLiteral("pan")).toDouble()));
            });
        addOperationBinding(
            OperationIds::tracks::set_mute,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().setTrackMute(
                    commandContext(arguments, invocation),
                    TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                    arguments.value(QStringLiteral("mute")).toBool()));
            });
        addOperationBinding(
            OperationIds::tracks::set_solo,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().setTrackSolo(
                    commandContext(arguments, invocation),
                    TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                    arguments.value(QStringLiteral("solo")).toBool()));
            });

        addOperationBinding(
            OperationIds::tracks::get_voice_context,
            [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                auto project = m_runtime.project().getProject(documentId(arguments));
                if (!project)
                    return AutomationResult<QJsonObject>(project.getError());
                const auto trackId = TrackId(arguments.value(QStringLiteral("track_id")).toInt());
                for (const auto &track : project.get().tracks) {
                    if (track.id != trackId)
                        continue;
                    const auto language = !track.data.defaultLanguage.isEmpty()
                                              ? track.data.defaultLanguage
                                              : track.data.singerInfo.defaultLanguage();
                    return AutomationResult<QJsonObject>(queryResult(
                        project.get().document, QStringLiteral("snapshot"),
                        encodeVoiceContext(track.data.singerInfo, track.data.speakerInfo,
                                           track.data.singerInfo, track.data.speakerInfo, false,
                                           language)));
                }
                return AutomationResult<QJsonObject>(AutomationError::notFound(
                    {ObjectKind::Track, trackId.value()}, QStringLiteral("Track was not found")));
            });

        addOperationBinding(
            OperationIds::tracks::clear_voice,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.parameters().clearTrackVoice(
                    commandContext(arguments, invocation),
                    TrackId(arguments.value(QStringLiteral("track_id")).toInt())));
            });

        addOperationBinding(
            OperationIds::master::set_pan,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.timeline().setMasterPan(
                    commandContext(arguments, invocation),
                    arguments.value(QStringLiteral("pan")).toDouble()));
            });
        addOperationBinding(
            OperationIds::master::set_mute,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.timeline().setMasterMute(
                    commandContext(arguments, invocation),
                    arguments.value(QStringLiteral("mute")).toBool()));
            });
        addOperationBinding(
            OperationIds::master::set_solo,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.timeline().setMasterSolo(
                    commandContext(arguments, invocation),
                    arguments.value(QStringLiteral("solo")).toBool()));
            });

        addOperationBinding(OperationIds::clips::list, [this](const QJsonObject &arguments,
                                                              const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            const auto trackFilter =
                arguments.contains(QStringLiteral("track_id"))
                    ? TrackId(arguments.value(QStringLiteral("track_id")).toInt())
                    : TrackId{};
            const auto typeFilter = arguments.value(QStringLiteral("type")).toString();
            const auto range = arguments.value(QStringLiteral("range")).toObject();
            const auto hasRange = !range.isEmpty();
            const auto rangeStart = range.value(QStringLiteral("start")).toInt();
            const auto rangeEnd = range.value(QStringLiteral("end")).toInt();
            QJsonArray clips;
            for (const auto &track : project.get().tracks) {
                if (trackFilter.isValid() && track.id != trackFilter)
                    continue;
                for (const auto &clip : track.clips) {
                    const auto type = clip.data.type == ClipDraftDto::Type::Singing
                                          ? QStringLiteral("singing")
                                          : QStringLiteral("audio");
                    if (!typeFilter.isEmpty() && type != typeFilter)
                        continue;
                    const auto start = clip.data.properties.start;
                    const auto end = start + clip.data.properties.length;
                    if (hasRange && (end <= rangeStart || start >= rangeEnd))
                        continue;
                    clips.append(encodeClipSnapshot(clip));
                }
            }
            auto page = paginateJson(
                m_collectionCursorCodec, clips, arguments, QStringLiteral("editor-public-clips/v1"),
                QJsonObject{
                    {QStringLiteral("document"), encodeDocumentVersion(project.get().document)},
                    {QStringLiteral("track_id"), arguments.value(QStringLiteral("track_id"))  },
                    {QStringLiteral("type"),     typeFilter                                   },
                    {QStringLiteral("range"),    range                                        },
                    {QStringLiteral("clips"),    clips                                        }
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            auto result =
                queryResult(project.get().document, QStringLiteral("clips"), page.get().items);
            if (!page.get().nextCursor.isEmpty())
                result.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            return AutomationResult<QJsonObject>(std::move(result));
        });

        addOperationBinding(OperationIds::clips::get, [this](const QJsonObject &arguments,
                                                             const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
            for (const auto &track : project.get().tracks) {
                for (const auto &clip : track.clips) {
                    if (clip.id == clipId) {
                        return AutomationResult<QJsonObject>(queryResult(project.get().document,
                                                                         QStringLiteral("snapshot"),
                                                                         encodeClipSnapshot(clip)));
                    }
                }
            }
            return AutomationResult<QJsonObject>(AutomationError::notFound(
                {ObjectKind::Clip, clipId.value()}, QStringLiteral("Clip was not found")));
        });

        addOperationBinding(
            OperationIds::clips::duplicate,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto destination = arguments.value(QStringLiteral("destination")).toObject();
                ClipDuplicateDestinationDto decoded;
                if (destination.contains(QStringLiteral("target_track_id"))) {
                    decoded.targetTrackId =
                        TrackId(destination.value(QStringLiteral("target_track_id")).toInt());
                }
                decoded.targetStart = destination.value(QStringLiteral("start")).toInt();
                return mutationResult(m_runtime.project().duplicateClips(
                    commandContext(arguments, invocation),
                    objectIds<ClipId>(arguments.value(QStringLiteral("clip_ids")).toArray()),
                    decoded));
            });

        addOperationBinding(
            OperationIds::clips::move,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QList<ClipMoveDto> moves;
                for (const auto &value : arguments.value(QStringLiteral("moves")).toArray()) {
                    const auto move = value.toObject();
                    moves.append({ClipId(move.value(QStringLiteral("clip_id")).toInt()),
                                  TrackId(move.value(QStringLiteral("target_track_id")).toInt()),
                                  move.value(QStringLiteral("start")).toInt()});
                }
                return mutationResult(
                    m_runtime.project().moveClips(commandContext(arguments, invocation), moves));
            });

        addOperationBinding(
            OperationIds::clips::resize_left,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().resizeClipLeft(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    arguments.value(QStringLiteral("start")).toInt()));
            });
        addOperationBinding(
            OperationIds::clips::resize_right,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().resizeClipRight(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    arguments.value(QStringLiteral("end")).toInt()));
            });
        addOperationBinding(
            OperationIds::clips::set_gain,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().setClipGain(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    arguments.value(QStringLiteral("gain")).toDouble()));
            });
        addOperationBinding(
            OperationIds::clips::set_mute,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.project().setClipMute(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    arguments.value(QStringLiteral("mute")).toBool()));
            });

        addOperationBinding(
            OperationIds::clips::get_voice_context,
            [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                auto project = m_runtime.project().getProject(documentId(arguments));
                if (!project)
                    return AutomationResult<QJsonObject>(project.getError());
                const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                for (const auto &track : project.get().tracks) {
                    for (const auto &clip : track.clips) {
                        if (clip.id != clipId)
                            continue;
                        if (clip.data.type != ClipDraftDto::Type::Singing) {
                            return AutomationResult<QJsonObject>(AutomationError::wrongObjectType(
                                {ObjectKind::Clip, clipId.value()},
                                QStringLiteral(
                                    "Voice context is available only for singing clips")));
                        }
                        const auto effectiveSinger = clip.data.usesTrackVoiceContext
                                                         ? track.data.singerInfo
                                                         : clip.data.ownSingerInfo;
                        const auto effectiveSpeaker = clip.data.usesTrackVoiceContext
                                                          ? track.data.speakerInfo
                                                          : clip.data.ownSpeakerInfo;
                        auto language = clip.data.defaultLanguage;
                        if (language.isEmpty()) {
                            language = clip.data.usesTrackVoiceContext
                                           ? (!track.data.defaultLanguage.isEmpty()
                                                  ? track.data.defaultLanguage
                                                  : effectiveSinger.defaultLanguage())
                                           : effectiveSinger.defaultLanguage();
                        }
                        return AutomationResult<QJsonObject>(queryResult(
                            project.get().document, QStringLiteral("snapshot"),
                            encodeVoiceContext(clip.data.ownSingerInfo, clip.data.ownSpeakerInfo,
                                               effectiveSinger, effectiveSpeaker,
                                               clip.data.usesTrackVoiceContext, language)));
                    }
                }
                return AutomationResult<QJsonObject>(AutomationError::notFound(
                    {ObjectKind::Clip, clipId.value()}, QStringLiteral("Clip was not found")));
            });

        addOperationBinding(
            OperationIds::clips::clear_voice,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.parameters().clearClipVoice(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
            });

        addOperationBinding(
            OperationIds::audio_clips::get,
            [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                auto project = m_runtime.project().getProject(documentId(arguments));
                if (!project)
                    return AutomationResult<QJsonObject>(project.getError());
                const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                for (const auto &track : project.get().tracks) {
                    for (const auto &clip : track.clips) {
                        if (clip.id != clipId)
                            continue;
                        if (clip.data.type != ClipDraftDto::Type::Audio) {
                            return AutomationResult<QJsonObject>(AutomationError::wrongObjectType(
                                {ObjectKind::Clip, clipId.value()},
                                QStringLiteral("Clip is not an audio clip")));
                        }
                        return AutomationResult<QJsonObject>(
                            queryResult(project.get().document, QStringLiteral("snapshot"),
                                        encodeAudioClipSnapshot(clip)));
                    }
                }
                return AutomationResult<QJsonObject>(AutomationError::notFound(
                    {ObjectKind::Clip, clipId.value()}, QStringLiteral("Clip was not found")));
            });

        addOperationBinding(
            OperationIds::speaker_mix::keyframes::insert,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                std::optional<QVector<double>> weights;
                if (arguments.contains(QStringLiteral("weights"))) {
                    weights.emplace();
                    for (const auto &value : arguments.value(QStringLiteral("weights")).toArray())
                        weights->append(value.toDouble());
                }
                return mutationResult(m_runtime.parameters().insertSpeakerMixKeyframe(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    arguments.value(QStringLiteral("position")).toInt(), weights));
            });
        addOperationBinding(
            OperationIds::speaker_mix::keyframes::move,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QList<QPair<SpeakerMixKeyframeId, int>> moves;
                for (const auto &value : arguments.value(QStringLiteral("moves")).toArray()) {
                    const auto move = value.toObject();
                    moves.append(
                        {SpeakerMixKeyframeId(move.value(QStringLiteral("keyframe_id")).toInt()),
                         move.value(QStringLiteral("position")).toInt()});
                }
                return mutationResult(m_runtime.parameters().moveSpeakerMixKeyframes(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()), moves));
            });
        addOperationBinding(
            OperationIds::speaker_mix::keyframes::set_weights,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QVector<double> weights;
                for (const auto &value : arguments.value(QStringLiteral("weights")).toArray())
                    weights.append(value.toDouble());
                return mutationResult(m_runtime.parameters().setSpeakerMixKeyframeWeights(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    SpeakerMixKeyframeId(arguments.value(QStringLiteral("keyframe_id")).toInt()),
                    std::move(weights)));
            });
        addOperationBinding(
            OperationIds::speaker_mix::keyframes::remove,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.parameters().removeSpeakerMixKeyframes(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    objectIds<SpeakerMixKeyframeId>(
                        arguments.value(QStringLiteral("keyframe_ids")).toArray())));
            });

        addOperationBinding(OperationIds::notes::search, [this](const QJsonObject &arguments,
                                                                const PublicInvocationContext &) {
            auto matches = m_runtime.notes().searchNotes(
                documentId(arguments), ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                arguments.value(QStringLiteral("query")).toString(),
                arguments.value(QStringLiteral("mode")).toString(),
                arguments.value(QStringLiteral("case_sensitive")).toBool(false),
                arguments.value(QStringLiteral("regex")).toBool(false));
            if (!matches)
                return AutomationResult<QJsonObject>(matches.getError());
            QJsonArray encoded;
            for (const auto &match : matches.get()) {
                encoded.append(QJsonObject{
                    {QStringLiteral("note_id"),     match.noteId.value()},
                    {QStringLiteral("local_start"), match.localStart    },
                    {QStringLiteral("length"),      match.length        },
                    {QStringLiteral("lyric"),       match.lyric         },
                });
            }
            return AutomationResult<QJsonObject>(
                queryResult(m_runtime.documentVersion(), QStringLiteral("matches"), encoded));
        });

        addOperationBinding(
            OperationIds::notes::duplicate,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.notes().duplicateNotes(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("source_clip_id")).toInt()),
                    objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                    ClipId(arguments.value(QStringLiteral("target_clip_id")).toInt()),
                    arguments.value(QStringLiteral("target_start")).toInt()));
            });

        addOperationBinding(
            OperationIds::notes::set_language,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                auto project = m_runtime.project().getProject(documentId(arguments));
                if (!project)
                    return AutomationResult<QJsonObject>(project.getError());
                const auto language =
                    decodeLanguageSelection(arguments.value(QStringLiteral("language")),
                                            effectiveDefaultLanguage(project.get(), clipId));
                return mutationResult(m_runtime.notes().setLanguages(
                    commandContext(arguments, invocation), clipId,
                    objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                    language));
            });

        addOperationBinding(
            OperationIds::notes::set_pronunciation,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.notes().setPronunciation(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    NoteId(arguments.value(QStringLiteral("note_id")).toInt()),
                    arguments.value(QStringLiteral("source")).toString() ==
                        QStringLiteral("original"),
                    arguments.value(QStringLiteral("pronunciation")).toString()));
            });

        addOperationBinding(
            OperationIds::notes::reset_pronunciation,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.notes().resetPronunciation(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    NoteId(arguments.value(QStringLiteral("note_id")).toInt())));
            });

        addOperationBinding(
            OperationIds::notes::set_phonemes,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                const auto noteId = NoteId(arguments.value(QStringLiteral("note_id")).toInt());
                auto notes = m_runtime.notes().getNotes(documentId(arguments), clipId);
                if (!notes)
                    return AutomationResult<QJsonObject>(notes.getError());
                std::optional<NoteSnapshotDto> selectedNote;
                for (const auto &note : notes.get()) {
                    if (note.id == noteId) {
                        selectedNote = note;
                        break;
                    }
                }
                if (!selectedNote) {
                    return AutomationResult<QJsonObject>(AutomationError::notFound(
                        {ObjectKind::Note, noteId.value()}, QStringLiteral("Note was not found")));
                }
                auto language = selectedNote->data.language;
                if (language.isEmpty()) {
                    auto project = m_runtime.project().getProject(documentId(arguments));
                    if (!project)
                        return AutomationResult<QJsonObject>(project.getError());
                    language = effectiveDefaultLanguage(project.get(), clipId);
                }
                auto phonemes = selectedNote->data.phonemes;
                phonemes.nameSeq.edited.clear();
                for (const auto &value : arguments.value(QStringLiteral("names")).toArray()) {
                    PhonemeName name;
                    name.language = language;
                    name.name = value.toString();
                    phonemes.nameSeq.edited.append(std::move(name));
                }
                phonemes.offsetSeq.clear();
                return mutationResult(m_runtime.notes().setPhonemes(
                    commandContext(arguments, invocation), clipId, noteId, phonemes));
            });

        addOperationBinding(
            OperationIds::notes::reset_phonemes,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.notes().resetPhonemes(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    NoteId(arguments.value(QStringLiteral("note_id")).toInt())));
            });

        addOperationBinding(
            OperationIds::notes::fill_lyrics,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                auto notes = m_runtime.notes().getNotes(documentId(arguments), clipId);
                if (!notes)
                    return AutomationResult<QJsonObject>(notes.getError());
                QSet<int> selected;
                for (const auto id :
                     objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray())) {
                    selected.insert(id.value());
                }
                QList<NoteSnapshotDto> ordered;
                for (const auto &note : notes.get()) {
                    if (selected.contains(note.id.value()))
                        ordered.append(note);
                }
                std::sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right) {
                    return left.data.localStart < right.data.localStart;
                });
                if (ordered.size() != selected.size()) {
                    return AutomationResult<QJsonObject>(AutomationError::notFound(
                        {ObjectKind::Note, -1}, QStringLiteral("A selected note was not found")));
                }
                auto project = m_runtime.project().getProject(documentId(arguments));
                if (!project)
                    return AutomationResult<QJsonObject>(project.getError());
                QStringList priority{effectiveDefaultLanguage(project.get(), clipId)};
                std::vector<std::string> priorityLanguages;
                for (const auto &language : std::as_const(priority)) {
                    if (!language.isEmpty())
                        priorityLanguages.push_back(language.toStdString());
                }
                const auto split = FillLyric::LyricSplitter::splitAuto(
                    arguments.value(QStringLiteral("text")).toString(), priorityLanguages);
                QList<LangNote> lyrics;
                for (const auto &line : split) {
                    for (const auto &word : line)
                        lyrics.append(word);
                }
                const auto options = arguments.value(QStringLiteral("options")).toObject();
                const bool skipSlur = options.value(QStringLiteral("skip_slur")).toBool(false);
                const auto languageOverride =
                    options.contains(QStringLiteral("language"))
                        ? decodeLanguageSelection(options.value(QStringLiteral("language")),
                                                  priority.value(0))
                        : QString();
                QList<NoteWordEditDto> edits;
                qsizetype lyricIndex = 0;
                for (const auto &note : ordered) {
                    if (skipSlur && Note::isSlurLyric(note.data.lyric))
                        continue;
                    if (lyricIndex >= lyrics.size())
                        break;
                    const auto &word = lyrics.at(lyricIndex++);
                    NoteWordEditDto edit;
                    edit.noteId = note.id;
                    edit.lyric = word.lyric;
                    edit.language = languageOverride.isEmpty() ? word.language : languageOverride;
                    edit.pronunciation = Pronunciation(word.syllable, word.syllableRevised);
                    edit.pronunciationCandidates = word.candidates;
                    edit.phonemes =
                        (Note::isSlurLyric(edit.lyric) || Note::isSyllabificationLyric(edit.lyric))
                            ? Phonemes{}
                            : note.data.phonemes;
                    edit.replacePronunciation = word.revised;
                    edit.replacePronunciationCandidates = true;
                    edits.append(std::move(edit));
                }
                return mutationResult(m_runtime.notes().setWordProperties(
                    commandContext(arguments, invocation), clipId, edits));
            });

        addOperationBinding(
            OperationIds::exports::midi::get_capabilities,
            [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                auto project = m_runtime.project().getProject(documentId(arguments));
                if (!project)
                    return AutomationResult<QJsonObject>(project.getError());
                QJsonArray trackIds;
                QJsonArray clipIds;
                for (const auto &track : project.get().tracks) {
                    trackIds.append(track.id.value());
                    for (const auto &clip : track.clips) {
                        if (clip.data.type == ClipDraftDto::Type::Singing)
                            clipIds.append(clip.id.value());
                    }
                }
                return AutomationResult<QJsonObject>(
                    queryResult(project.get().document, QStringLiteral("capabilities"),
                                QJsonObject{
                                    {QStringLiteral("track_ids"),        trackIds                         },
                                    {QStringLiteral("clip_ids"),         clipIds                          },
                                    {QStringLiteral("formats"),          QJsonArray{QStringLiteral("mid")}},
                                    {QStringLiteral("lyrics_supported"), true                             },
                                    {QStringLiteral("option_schema"),    QJsonObject{}                    },
                }));
            });

        addOperationBinding(OperationIds::exports::midi::preview, [this](
                                                                      const QJsonObject &arguments,
                                                                      const PublicInvocationContext
                                                                          &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());

            QSet<int> availableTracks;
            QSet<int> availableClips;
            for (const auto &track : project.get().tracks) {
                availableTracks.insert(track.id.value());
                for (const auto &clip : track.clips) {
                    if (clip.data.type == ClipDraftDto::Type::Singing)
                        availableClips.insert(clip.id.value());
                }
            }
            const auto options = arguments.value(QStringLiteral("options")).toObject();
            for (const auto &value : options.value(QStringLiteral("track_ids")).toArray()) {
                if (!availableTracks.contains(value.toInt())) {
                    return AutomationResult<QJsonObject>(AutomationError::notFound(
                        {ObjectKind::Track, value.toInt()},
                        QStringLiteral("A selected MIDI export track was not found")));
                }
            }
            for (const auto &value : options.value(QStringLiteral("clip_ids")).toArray()) {
                if (!availableClips.contains(value.toInt())) {
                    return AutomationResult<QJsonObject>(AutomationError::notFound(
                        {ObjectKind::Clip, value.toInt()},
                        QStringLiteral(
                            "A selected MIDI export clip was not found or is not a singing clip")));
                }
            }

            QJsonArray diagnostics;
            const auto path = arguments.value(QStringLiteral("path")).toString();
            if (QFileInfo::exists(path)) {
                diagnostics.append(QJsonObject{
                    {QStringLiteral("code"),     QStringLiteral("target_exists")                  },
                    {QStringLiteral("message"),
                     QStringLiteral("The target already exists and requires overwrite permission")},
                    {QStringLiteral("blocking"), true                                             },
                });
            }
            QJsonObject plan{
                {QStringLiteral("targets"),     QJsonArray{path}},
                {QStringLiteral("diagnostics"), diagnostics     },
            };
            plan.insert(QStringLiteral("plan_digest"), AutomationWire::sha256Digest(plan));
            return AutomationResult<QJsonObject>(
                queryResult(project.get().document, QStringLiteral("plan"), plan));
        });

        addOperationBinding(
            OperationIds::inference::get_status,
            [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                const auto document = documentId(arguments);
                auto project = m_runtime.project().getProject(document);
                if (!project)
                    return AutomationResult<QJsonObject>(project.getError());

                std::optional<AutomationTaskSnapshot> activeTask;
                const auto tasks = m_runtime.automationTasks().list(document);
                for (auto it = tasks.crbegin(); it != tasks.crend(); ++it) {
                    if (it->operationId != OperationIds::inference::start)
                        continue;
                    if (it->state == AutomationTaskState::Queued ||
                        it->state == AutomationTaskState::Running ||
                        it->state == AutomationTaskState::CancelRequested ||
                        it->state == AutomationTaskState::Committing) {
                        activeTask = *it;
                        break;
                    }
                }

                QJsonArray stages;
                for (const auto &stage : InferenceAutomationFacade::supportedStages()) {
                    QString state = QStringLiteral("idle");
                    QString reason;
                    QJsonValue taskId = QJsonValue(QJsonValue::Null);
                    if (activeTask) {
                        state = activeTask->state == AutomationTaskState::Queued
                                    ? QStringLiteral("queued")
                                    : QStringLiteral("running");
                        taskId = activeTask->taskId.toString();
                    }
                    stages.append(QJsonObject{
                        {QStringLiteral("stage"),   stage },
                        {QStringLiteral("state"),   state },
                        {QStringLiteral("reason"),  reason},
                        {QStringLiteral("task_id"), taskId},
                    });
                }
                return AutomationResult<QJsonObject>(queryResult(
                    project.get().document, QStringLiteral("status"),
                    QJsonObject{
                        {QStringLiteral("scope"),  arguments.value(QStringLiteral("scope"))},
                        {QStringLiteral("stages"), stages                                  },
                }));
            });

        Q_ASSERT(m_handlers.size() == contracts().size());
        Q_ASSERT(isComplete());
    }

} // namespace Automation
