#include "ParameterCurvesJson.h"

#include <QJsonObject>
#include <QSet>

#include <optional>
#include <limits>

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

    std::optional<int> integerValue(const QJsonValue &value) {
        if (!value.isDouble())
            return std::nullopt;
        const auto number = value.toInt();
        if (value.toDouble() != number)
            return std::nullopt;
        return number;
    }

    std::optional<Automation::CurveDraftDto> deserializeCurve(const ParamInfo::Name name,
                                                              const QJsonObject &object) {
        Automation::CurveDraftDto result;
        const auto type = object.value(QStringLiteral("type")).toString();
        const auto localStart = integerValue(object.value(QStringLiteral("local_start")));
        if (!localStart)
            return std::nullopt;
        result.localStart = *localStart;
        if (type == QStringLiteral("draw")) {
            result.type = Automation::CurveDraftDto::Type::Draw;
            const auto step = integerValue(object.value(QStringLiteral("step")));
            if (!step)
                return std::nullopt;
            result.step = *step;
            for (const auto value : object.value(QStringLiteral("values")).toArray()) {
                const auto sample = integerValue(value);
                if (!sample)
                    return std::nullopt;
                result.values.append(*sample);
            }
        } else if (type == QStringLiteral("anchor")) {
            result.type = Automation::CurveDraftDto::Type::Anchor;
            for (const auto value : object.value(QStringLiteral("nodes")).toArray()) {
                const auto node = value.toObject();
                const auto position = integerValue(node.value(QStringLiteral("position")));
                const auto nodeValue = integerValue(node.value(QStringLiteral("value")));
                const auto interpolation =
                    integerValue(node.value(QStringLiteral("interpolation")));
                if (!position || !nodeValue || !interpolation)
                    return std::nullopt;
                result.nodes.append({
                    .position = *position,
                    .value = *nodeValue,
                    .interpolation = static_cast<AnchorNode::InterpMode>(*interpolation),
                });
            }
        } else {
            return std::nullopt;
        }
        if (!Automation::validCurveDraft(name, result, 0, std::numeric_limits<int>::max()))
            return std::nullopt;
        return result;
    }
}

namespace ClipboardDataModel {
    QJsonArray serializeParameters(const QList<Automation::ParamCurvesDraftDto> &parameters) {
        QJsonArray result;
        for (const auto &parameter : parameters) {
            QJsonArray curves;
            for (const auto &curve : parameter.curves)
                curves.append(serializeCurve(curve));
            result.append(QJsonObject{
                {QStringLiteral("name"),   static_cast<int>(parameter.name)},
                {QStringLiteral("layer"),  static_cast<int>(parameter.type)},
                {QStringLiteral("curves"), curves                          },
            });
        }
        return result;
    }

    QList<Automation::ParamCurvesDraftDto> deserializeParameters(const QJsonArray &parameters) {
        QList<Automation::ParamCurvesDraftDto> result;
        QSet<QPair<int, int>> acceptedGroups;
        for (const auto parameterValue : parameters) {
            const auto parameterObject = parameterValue.toObject();
            const auto name =
                integerValue(parameterObject.value(QStringLiteral("name"))).value_or(-1);
            const auto layer =
                integerValue(parameterObject.value(QStringLiteral("layer"))).value_or(-1);
            if (name < ParamInfo::Pitch || name > ParamInfo::ToneShift ||
                (layer != Param::Original && layer != Param::Edited && layer != Param::Envelope)) {
                continue;
            }
            const auto key = qMakePair(name, layer);
            if (acceptedGroups.contains(key))
                continue;
            Automation::ParamCurvesDraftDto parameter{
                .name = static_cast<ParamInfo::Name>(name),
                .type = static_cast<Param::Type>(layer),
            };
            for (const auto curveValue :
                 parameterObject.value(QStringLiteral("curves")).toArray()) {
                auto curve = deserializeCurve(parameter.name, curveValue.toObject());
                if (curve)
                    parameter.curves.append(std::move(*curve));
            }
            if (!parameter.curves.isEmpty() &&
                !Automation::hasOverlappingAnchorCurves(parameter.curves)) {
                acceptedGroups.insert(key);
                result.append(std::move(parameter));
            }
        }
        return result;
    }
} // namespace ClipboardDataModel
