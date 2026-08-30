#include "NoteTransfer.h"

#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <algorithm>
#include <array>
#include <memory>
#include <numeric>
#include <optional>

namespace Automation {
    namespace {
        constexpr std::array kParameterNames{
            ParamInfo::Pitch,        ParamInfo::Expressiveness, ParamInfo::Energy,
            ParamInfo::Breathiness,  ParamInfo::Voicing,        ParamInfo::Tension,
            ParamInfo::MouthOpening, ParamInfo::Gender,         ParamInfo::Velocity,
            ParamInfo::ToneShift,
        };

        void clearIdentity(CurveDraftDto &draft) {
            draft.id = {};
            for (auto &node : draft.nodes)
                node.id = {};
        }

        int curveStart(const Curve &curve) {
            if (curve.type() != Curve::Anchor)
                return curve.localStart();
            const auto &nodes = static_cast<const AnchorCurve &>(curve).nodes().toList();
            return nodes.isEmpty() ? curve.localStart() : nodes.first()->pos();
        }

        int curveEnd(const Curve &curve) {
            if (curve.type() != Curve::Anchor)
                return curve.localEndTick();
            const auto &nodes = static_cast<const AnchorCurve &>(curve).nodes().toList();
            return nodes.isEmpty() ? curve.localStart() : nodes.last()->pos();
        }

        int drawValueAt(const DrawCurve &curve, const int tick) {
            const auto offset = tick - curve.localStart();
            const auto leftIndex = offset / curve.step;
            if (leftIndex >= curve.values().size() - 1)
                return curve.values().last();
            const auto remainder = offset % curve.step;
            if (remainder == 0)
                return curve.values().at(leftIndex);
            const auto left = curve.values().at(leftIndex);
            const auto right = curve.values().at(leftIndex + 1);
            return qRound(left + (right - left) * static_cast<double>(remainder) / curve.step);
        }

        std::optional<CurveDraftDto> sliceDrawCurve(const DrawCurve &curve, const int start,
                                                    const int end) {
            if (curve.step <= 0 || curve.values().isEmpty() || start >= end ||
                curve.localEndTick() <= start || curve.localStart() >= end) {
                return std::nullopt;
            }

            const auto clippedStart = std::max(start, curve.localStart());
            const auto clippedEnd = std::min(end, curve.localEndTick());
            if (clippedStart >= clippedEnd)
                return std::nullopt;

            auto commonStep = curve.step;
            commonStep = std::gcd(commonStep, clippedStart - curve.localStart());
            commonStep = std::gcd(commonStep, clippedEnd - curve.localStart());
            if (commonStep <= 0)
                return std::nullopt;

            CurveDraftDto result;
            result.type = CurveDraftDto::Type::Draw;
            result.localStart = clippedStart;
            result.step = commonStep;
            for (auto tick = clippedStart; tick < clippedEnd; tick += commonStep)
                result.values.append(drawValueAt(curve, tick));
            return result;
        }

        QList<CurveDraftDto> sliceCurve(const Curve &curve, const int start, const int end) {
            if (start >= end || curveEnd(curve) <= start || curveStart(curve) >= end)
                return {};

            if (curve.type() == Curve::Draw) {
                const auto result =
                    sliceDrawCurve(static_cast<const DrawCurve &>(curve), start, end);
                return result ? QList<CurveDraftDto>{*result} : QList<CurveDraftDto>{};
            }
            if (curve.type() != Curve::Anchor)
                return {};

            const auto &anchor = static_cast<const AnchorCurve &>(curve);
            const auto &nodes = anchor.nodes().toList();
            if (!nodes.isEmpty() && nodes.first()->pos() >= start && nodes.last()->pos() <= end) {
                auto result = curveDraftDto(anchor);
                result.localStart = nodes.first()->pos();
                clearIdentity(result);
                return {std::move(result)};
            }

            const std::unique_ptr<DrawCurve> sampled(anchor.toDrawCurve());
            if (!sampled)
                return {};
            const auto result = sliceDrawCurve(*sampled, start, end);
            return result ? QList<CurveDraftDto>{*result} : QList<CurveDraftDto>{};
        }

        QList<CurveDraftDto> subtractRange(const Curve &curve, const int start, const int end) {
            const auto curveRangeStart = curveStart(curve);
            const auto curveRangeEnd = curveEnd(curve);
            if (start >= end || curveRangeEnd <= start || curveRangeStart >= end)
                return {curveDraftDto(curve)};

            QList<CurveDraftDto> result;
            if (curveRangeStart < start)
                result.append(sliceCurve(curve, curveRangeStart, start));
            if (curveRangeEnd > end)
                result.append(sliceCurve(curve, end, curveRangeEnd));
            for (auto &draft : result)
                clearIdentity(draft);
            return result;
        }

        void translate(CurveDraftDto &draft, const int delta) {
            if (draft.type == CurveDraftDto::Type::Draw) {
                draft.localStart += delta;
            } else {
                draft.localStart += delta;
                for (auto &node : draft.nodes)
                    node.position += delta;
            }
            clearIdentity(draft);
        }

        const ParamCurvesDraftDto *findParameter(const NoteTransferPayload &payload,
                                                 const ParamInfo::Name name,
                                                 const Param::Type type) {
            for (const auto &parameter : payload.parameters) {
                if (parameter.name == name && parameter.type == type)
                    return &parameter;
            }
            return nullptr;
        }

    } // namespace

    NoteTransferPayload captureNoteTransfer(const SingingClip &clip, const QList<Note *> &notes) {
        NoteTransferPayload result;
        QList<Note *> ordered;
        ordered.reserve(notes.size());
        for (auto *note : notes) {
            if (note && note->clip() == &clip)
                ordered.append(note);
        }
        std::sort(ordered.begin(), ordered.end(), [](const Note *left, const Note *right) {
            if (left->localStart() != right->localStart())
                return left->localStart() < right->localStart();
            return left->id() < right->id();
        });
        if (ordered.isEmpty())
            return result;

        result.sourceStart = ordered.first()->localStart();
        result.sourceEnd = result.sourceStart + ordered.first()->length();
        result.notes.reserve(ordered.size());
        for (const auto *note : ordered) {
            result.sourceStart = std::min(result.sourceStart, note->localStart());
            result.sourceEnd = std::max(result.sourceEnd, note->localStart() + note->length());
            result.notes.append(noteDraftDto(*note));
        }

        for (const auto name : kParameterNames) {
            const auto *parameter = clip.params.getParamByName(name);
            if (!parameter)
                continue;
            for (const auto type : {Param::Edited, Param::Envelope}) {
                ParamCurvesDraftDto captured{.name = name, .type = type};
                for (const auto *curve : parameter->curves(type)) {
                    if (!curve)
                        continue;
                    captured.curves.append(
                        sliceCurve(*curve, result.sourceStart, result.sourceEnd));
                }
                if (!captured.curves.isEmpty())
                    result.parameters.append(std::move(captured));
            }
        }
        return result;
    }

    QList<ParamCurvesDraftDto> mergeNoteTransferParameters(const SingingClip &target,
                                                           const NoteTransferPayload &payload,
                                                           const int targetStart) {
        QList<ParamCurvesDraftDto> result;
        if (payload.sourceEnd <= payload.sourceStart)
            return result;

        const auto delta = targetStart - payload.sourceStart;
        const auto targetEnd = static_cast<int>(static_cast<qint64>(targetStart) +
                                                payload.sourceEnd - payload.sourceStart);
        for (const auto name : kParameterNames) {
            const auto *targetParameter = target.params.getParamByName(name);
            if (!targetParameter)
                continue;
            for (const auto type : {Param::Edited, Param::Envelope}) {
                const auto *sourceParameter = findParameter(payload, name, type);
                if (!sourceParameter || sourceParameter->curves.isEmpty())
                    continue;

                ParamCurvesDraftDto replacement{.name = name, .type = type};
                for (const auto *curve : targetParameter->curves(type)) {
                    if (curve)
                        replacement.curves.append(subtractRange(*curve, targetStart, targetEnd));
                }
                for (auto curve : sourceParameter->curves) {
                    translate(curve, delta);
                    replacement.curves.append(std::move(curve));
                }
                std::sort(replacement.curves.begin(), replacement.curves.end(),
                          [](const CurveDraftDto &left, const CurveDraftDto &right) {
                              return left.localStart < right.localStart;
                          });
                result.append(std::move(replacement));
            }
        }
        return result;
    }

} // namespace Automation
