#include "NoteTransfer.h"

#include <lite/AutomationWire/PublicConstants.h>
#include <lite/ProjectModel/AppModel/AnchorCurve.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <algorithm>
#include <array>
#include <limits>
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
                                                    const int end, bool *sampleLimitExceeded) {
            if (sampleLimitExceeded)
                *sampleLimitExceeded = false;
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
            const auto pointCount =
                (static_cast<qint64>(clippedEnd) - clippedStart + commonStep - 1) / commonStep;
            if (pointCount > AutomationWire::MaximumCurveSampleItems) {
                if (sampleLimitExceeded)
                    *sampleLimitExceeded = true;
                return std::nullopt;
            }
            result.values.reserve(static_cast<qsizetype>(pointCount));
            for (qint64 tick = clippedStart; tick < clippedEnd; tick += commonStep)
                result.values.append(drawValueAt(curve, static_cast<int>(tick)));
            return result;
        }

        std::unique_ptr<DrawCurve> sampleAnchorRange(const AnchorCurve &anchor, const int start,
                                                     const int end, bool *sampleLimitExceeded) {
            if (sampleLimitExceeded)
                *sampleLimitExceeded = false;
            const auto &nodes = anchor.nodes().toList();
            if (nodes.size() < 2 || start >= end)
                return {};

            constexpr qint64 step = 5;
            const auto fullStart = static_cast<qint64>(nodes.first()->pos()) / step * step;
            const auto lastNode = static_cast<qint64>(nodes.last()->pos());
            const auto lastSample = fullStart + (lastNode - fullStart) / step * step;
            const auto requestedStart = std::max<qint64>(start, fullStart);
            const auto requestedEnd = std::min<qint64>(end, lastSample + step);
            if (requestedStart >= requestedEnd)
                return {};

            const auto sampleStart =
                fullStart + (requestedStart - fullStart) / step * step;
            const auto sampleEnd = std::min(
                lastSample,
                fullStart + (requestedEnd - 1 - fullStart + step - 1) / step * step);
            const auto pointCount = (sampleEnd - sampleStart) / step + 1;
            const auto materializedEnd = sampleStart + pointCount * step;
            if (pointCount > AutomationWire::MaximumCurveSampleItems) {
                if (sampleLimitExceeded)
                    *sampleLimitExceeded = true;
                return {};
            }
            if (pointCount <= 0 || materializedEnd > std::numeric_limits<int>::max()) {
                return {};
            }

            int segmentIndex = 0;
            while (segmentIndex < nodes.size() - 2 &&
                   sampleStart >= nodes.at(segmentIndex + 1)->pos()) {
                ++segmentIndex;
            }
            auto *leftReference = segmentIndex > 0 ? nodes.at(segmentIndex - 1) : nullptr;
            auto *rightReference =
                segmentIndex + 2 < nodes.size() ? nodes.at(segmentIndex + 2) : nullptr;
            auto interpolator = AnchorCurve::createInterpolator(
                nodes.at(segmentIndex), nodes.at(segmentIndex + 1), leftReference, rightReference);

            QList<int> values;
            values.reserve(static_cast<qsizetype>(pointCount));
            for (qint64 tick = sampleStart; tick <= sampleEnd; tick += step) {
                while (segmentIndex < nodes.size() - 2 &&
                       tick >= nodes.at(segmentIndex + 1)->pos()) {
                    ++segmentIndex;
                    leftReference = segmentIndex > 0 ? nodes.at(segmentIndex - 1) : nullptr;
                    rightReference =
                        segmentIndex + 2 < nodes.size() ? nodes.at(segmentIndex + 2) : nullptr;
                    interpolator = AnchorCurve::createInterpolator(
                        nodes.at(segmentIndex), nodes.at(segmentIndex + 1), leftReference,
                        rightReference);
                }
                const auto sampleTick = std::clamp<qint64>(
                    tick, nodes.at(segmentIndex)->pos(), nodes.at(segmentIndex + 1)->pos());
                values.append(static_cast<int>(interpolator.evaluate(sampleTick)));
            }

            auto result = std::make_unique<DrawCurve>();
            result->setLocalStart(static_cast<int>(sampleStart));
            result->step = static_cast<int>(step);
            result->setValues(values);
            return result;
        }

        std::optional<QList<CurveDraftDto>> sliceCurve(const Curve &curve, const int start,
                                                       const int end) {
            if (start >= end || curveEnd(curve) <= start || curveStart(curve) >= end)
                return QList<CurveDraftDto>{};

            if (curve.type() == Curve::Draw) {
                bool sampleLimitExceeded = false;
                const auto result = sliceDrawCurve(static_cast<const DrawCurve &>(curve), start,
                                                   end, &sampleLimitExceeded);
                if (sampleLimitExceeded)
                    return std::nullopt;
                return result ? QList<CurveDraftDto>{*result} : QList<CurveDraftDto>{};
            }
            if (curve.type() != Curve::Anchor)
                return QList<CurveDraftDto>{};

            const auto &anchor = static_cast<const AnchorCurve &>(curve);
            const auto &nodes = anchor.nodes().toList();
            if (!nodes.isEmpty() && nodes.first()->pos() >= start && nodes.last()->pos() <= end) {
                auto result = curveDraftDto(anchor);
                result.localStart = nodes.first()->pos();
                clearIdentity(result);
                return QList<CurveDraftDto>{std::move(result)};
            }

            bool sampleLimitExceeded = false;
            const auto sampled = sampleAnchorRange(anchor, start, end, &sampleLimitExceeded);
            if (sampleLimitExceeded)
                return std::nullopt;
            if (!sampled)
                return QList<CurveDraftDto>{};
            const auto result = sliceDrawCurve(*sampled, start, end, &sampleLimitExceeded);
            if (sampleLimitExceeded)
                return std::nullopt;
            return result ? QList<CurveDraftDto>{*result} : QList<CurveDraftDto>{};
        }

        std::optional<QList<CurveDraftDto>> subtractRange(const Curve &curve, const int start,
                                                          const int end) {
            const auto curveRangeStart = curveStart(curve);
            const auto curveRangeEnd = curveEnd(curve);
            if (start >= end || curveRangeEnd <= start || curveRangeStart >= end)
                return QList<CurveDraftDto>{curveDraftDto(curve)};

            QList<CurveDraftDto> result;
            if (curveRangeStart < start) {
                const auto prefix = sliceCurve(curve, curveRangeStart, start);
                if (!prefix || prefix->isEmpty())
                    return std::nullopt;
                result.append(*prefix);
            }
            if (curveRangeEnd > end) {
                const auto suffix = sliceCurve(curve, end, curveRangeEnd);
                if (!suffix || suffix->isEmpty())
                    return std::nullopt;
                result.append(*suffix);
            }
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

    AutomationResult<NoteTransferPayload> captureNoteTransfer(const SingingClip &clip,
                                                              const QList<Note *> &notes) {
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
                    const auto sliced = sliceCurve(*curve, result.sourceStart, result.sourceEnd);
                    if (!sliced) {
                        return AutomationError{
                            .code = AutomationErrorCode::Unsupported,
                            .message = QStringLiteral(
                                "Source parameter curve is too long to capture during note transfer"),
                        };
                    }
                    captured.curves.append(*sliced);
                }
                if (!captured.curves.isEmpty())
                    result.parameters.append(std::move(captured));
            }
        }
        return result;
    }

    AutomationResult<QList<ParamCurvesDraftDto>>
        mergeNoteTransferParameters(const SingingClip &target, const NoteTransferPayload &payload,
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
                    if (!curve)
                        continue;
                    const auto retained = subtractRange(*curve, targetStart, targetEnd);
                    if (!retained) {
                        return AutomationError{
                            .code = AutomationErrorCode::Unsupported,
                            .message = QStringLiteral(
                                "Target parameter curve is too long to preserve during note transfer"),
                        };
                    }
                    replacement.curves.append(*retained);
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
