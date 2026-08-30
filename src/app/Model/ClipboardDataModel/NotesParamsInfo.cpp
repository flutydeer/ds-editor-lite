#include "NotesParamsInfo.h"

#include <lite/ProjectModel/AppModel/Note.h>

#include <QJsonArray>

#include <algorithm>
#include <limits>
#include <optional>

namespace {
    QJsonObject serializeCurve(const Automation::CurveDraftDto &curve) {
        QJsonObject result{
            {QStringLiteral("type"),        curve.type == Automation::CurveDraftDto::Type::Anchor
                                         ? QStringLiteral("anchor")
                                         : QStringLiteral("draw")},
            {QStringLiteral("local_start"), curve.localStart                                         },
        };
        if (curve.type == Automation::CurveDraftDto::Type::Draw) {
            result.insert(QStringLiteral("step"), curve.step);
            QJsonArray values;
            for (const auto value : curve.values)
                values.append(value);
            result.insert(QStringLiteral("values"), values);
        } else {
            QJsonArray nodes;
            for (const auto &node : curve.nodes) {
                nodes.append(QJsonObject{
                    {QStringLiteral("position"),      node.position                       },
                    {QStringLiteral("value"),         node.value                          },
                    {QStringLiteral("interpolation"), static_cast<int>(node.interpolation)},
                });
            }
            result.insert(QStringLiteral("nodes"), nodes);
        }
        return result;
    }

    std::optional<Automation::CurveDraftDto> deserializeCurve(const QJsonObject &object) {
        Automation::CurveDraftDto result;
        const auto type = object.value(QStringLiteral("type")).toString();
        result.localStart = object.value(QStringLiteral("local_start")).toInt();
        if (type == QStringLiteral("draw")) {
            result.type = Automation::CurveDraftDto::Type::Draw;
            result.step = object.value(QStringLiteral("step")).toInt();
            for (const auto value : object.value(QStringLiteral("values")).toArray())
                result.values.append(value.toInt());
            if (result.step <= 0 || result.values.isEmpty())
                return std::nullopt;
            return result;
        }
        if (type != QStringLiteral("anchor"))
            return std::nullopt;

        result.type = Automation::CurveDraftDto::Type::Anchor;
        for (const auto value : object.value(QStringLiteral("nodes")).toArray()) {
            const auto node = value.toObject();
            const auto interpolation = node.value(QStringLiteral("interpolation")).toInt();
            if (interpolation < AnchorNode::Linear || interpolation > AnchorNode::None)
                return std::nullopt;
            result.nodes.append({
                .position = node.value(QStringLiteral("position")).toInt(),
                .value = node.value(QStringLiteral("value")).toInt(),
                .interpolation = static_cast<AnchorNode::InterpMode>(interpolation),
            });
        }
        if (result.nodes.size() < 2)
            return std::nullopt;
        return result;
    }
}

QJsonObject NotesParamsInfo::serializeToJson(const NotesParamsInfo &info) {
    QJsonArray noteList;
    for (const auto &draft : info.payload.notes) {
        const auto note = Automation::buildNote(draft, nullptr);
        noteList.append(note->serialize());
    }

    QJsonArray parameters;
    for (const auto &parameter : info.payload.parameters) {
        QJsonArray curves;
        for (const auto &curve : parameter.curves)
            curves.append(serializeCurve(curve));
        parameters.append(QJsonObject{
            {QStringLiteral("name"),   static_cast<int>(parameter.name)},
            {QStringLiteral("layer"),  static_cast<int>(parameter.type)},
            {QStringLiteral("curves"), curves                          },
        });
    }

    return QJsonObject{
        {QStringLiteral("schema_version"), 2                       },
        {QStringLiteral("source_start"),   info.payload.sourceStart},
        {QStringLiteral("source_end"),     info.payload.sourceEnd  },
        {QStringLiteral("notes"),          noteList                },
        {QStringLiteral("parameters"),     parameters              },
    };
}

NotesParamsInfo NotesParamsInfo::deserializeFromJson(const QJsonObject &obj) {
    NotesParamsInfo info;
    const auto arrNotes = obj.value(QStringLiteral("notes")).toArray();
    for (const auto &valNote : arrNotes) {
        Note note;
        if (!note.deserialize(valNote.toObject()))
            continue;
        auto draft = Automation::noteDraftDto(note);
        const auto noteEnd = static_cast<qint64>(draft.localStart) + draft.length;
        if (draft.localStart < 0 || draft.length <= 0 ||
            noteEnd > std::numeric_limits<int>::max()) {
            return {};
        }
        info.payload.notes.append(std::move(draft));
    }
    if (!info.payload.notes.isEmpty()) {
        info.payload.sourceStart = info.payload.notes.first().localStart;
        info.payload.sourceEnd = static_cast<int>(
            static_cast<qint64>(info.payload.sourceStart) + info.payload.notes.first().length);
        for (const auto &note : info.payload.notes) {
            info.payload.sourceStart = std::min(info.payload.sourceStart, note.localStart);
            const auto noteEnd = static_cast<qint64>(note.localStart) + note.length;
            info.payload.sourceEnd =
                std::max(info.payload.sourceEnd, static_cast<int>(noteEnd));
        }
    }

    if (obj.value(QStringLiteral("schema_version")).toInt() < 2)
        return info;

    const auto serializedStart = obj.value(QStringLiteral("source_start")).toInt();
    const auto serializedEnd = obj.value(QStringLiteral("source_end")).toInt();
    if (serializedStart == info.payload.sourceStart && serializedEnd == info.payload.sourceEnd) {
        for (const auto parameterValue : obj.value(QStringLiteral("parameters")).toArray()) {
            const auto parameterObject = parameterValue.toObject();
            const auto name = parameterObject.value(QStringLiteral("name")).toInt();
            const auto layer = parameterObject.value(QStringLiteral("layer")).toInt();
            if (name < ParamInfo::Pitch || name > ParamInfo::ToneShift ||
                (layer != Param::Edited && layer != Param::Envelope)) {
                continue;
            }
            Automation::ParamCurvesDraftDto parameter{
                .name = static_cast<ParamInfo::Name>(name),
                .type = static_cast<Param::Type>(layer),
            };
            const auto serializedCurves = parameterObject.value(QStringLiteral("curves")).toArray();
            for (const auto curveValue : serializedCurves) {
                auto curve = deserializeCurve(curveValue.toObject());
                if (curve)
                    parameter.curves.append(std::move(*curve));
            }
            if (!parameter.curves.isEmpty())
                info.payload.parameters.append(std::move(parameter));
        }
    }
    return info;
}
