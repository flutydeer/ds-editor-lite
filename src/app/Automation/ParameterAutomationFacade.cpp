#include "ParameterAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/Param/ParamsActions.h"
#include "Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.h"

#include <lite/AutomationWire/PublicConstants.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>

#include <QCryptographicHash>
#include <QHash>
#include <QJsonDocument>
#include <QSet>

#include <memory>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace Automation {
    namespace {
        void hashInteger(QCryptographicHash &hash, const qint64 value) {
            hash.addData(QByteArray::number(value));
            hash.addData(";", 1);
        }

        void hashCurves(QCryptographicHash &hash, const QList<CurveDraftDto> &curves) {
            hashInteger(hash, curves.size());
            for (const auto &curve : curves) {
                hashInteger(hash, curve.id.value());
                hashInteger(hash, static_cast<int>(curve.type));
                hashInteger(hash, curve.localStart);
                hashInteger(hash, curve.step);
                hashInteger(hash, curve.values.size());
                for (const auto value : curve.values)
                    hashInteger(hash, value);
                hashInteger(hash, curve.nodes.size());
                for (const auto &node : curve.nodes) {
                    hashInteger(hash, node.id.value());
                    hashInteger(hash, node.position);
                    hashInteger(hash, node.value);
                    hashInteger(hash, node.interpolation);
                }
            }
        }

        void hashCurveShapes(QCryptographicHash &hash, const QList<CurveDraftDto> &curves) {
            hashInteger(hash, curves.size());
            for (const auto &curve : curves) {
                hashInteger(hash, static_cast<int>(curve.type));
                hashInteger(hash, curve.localStart);
                hashInteger(hash, curve.step);
                hashInteger(hash, curve.values.size());
                for (const auto value : curve.values)
                    hashInteger(hash, value);
                hashInteger(hash, curve.nodes.size());
                for (const auto &node : curve.nodes) {
                    hashInteger(hash, node.position);
                    hashInteger(hash, node.value);
                    hashInteger(hash, node.interpolation);
                }
            }
        }

        bool hasExplicitCurveIdentity(const QList<CurveDraftDto> &curves) {
            for (const auto &curve : curves) {
                if (curve.id.isValid())
                    return true;
                for (const auto &node : curve.nodes) {
                    if (node.id.isValid())
                        return true;
                }
            }
            return false;
        }

        void clearCurveIdentity(CurveDraftDto &curve) {
            curve.id = {};
            for (auto &node : curve.nodes)
                node.id = {};
        }

        DrawCurve *freshDrawCurve(const CurveDraftDto &draft) {
            if (draft.type == CurveDraftDto::Type::Draw) {
                auto *result = new DrawCurve;
                result->Curve::setLocalStart(draft.localStart);
                result->step = draft.step;
                result->setValues(draft.values);
                return result;
            }
            const auto curve = buildCurve(draft);
            return static_cast<const AnchorCurve *>(curve.get())->toDrawCurve();
        }

        std::optional<qint64> drawMaterializationPointCount(const CurveDraftDto &draft) {
            if (draft.type == CurveDraftDto::Type::Draw) {
                if (draft.step <= 0 || draft.localStart < 0)
                    return std::nullopt;
                const auto end = static_cast<qint64>(draft.localStart) +
                                 static_cast<qint64>(draft.step) * draft.values.size();
                if (end > std::numeric_limits<int>::max())
                    return std::nullopt;
                return draft.values.size();
            }
            if (draft.nodes.size() < 2)
                return 0;

            const auto first = static_cast<qint64>(draft.nodes.constFirst().position);
            const auto last = static_cast<qint64>(draft.nodes.constLast().position);
            if (first < 0 || last < first)
                return std::nullopt;
            constexpr qint64 step = 5;
            const auto start = first / step * step;
            const auto count = (last - start) / step + 1;
            if (start + step * count > std::numeric_limits<int>::max())
                return std::nullopt;
            return count;
        }

        bool reserveBakeMaterialization(const CurveDraftDto &draft, qint64 &reservedPoints) {
            const auto count = drawMaterializationPointCount(draft);
            if (!count || *count > AutomationWire::MaximumCurveSampleItems - reservedPoints)
                return false;
            reservedPoints += *count;
            return true;
        }

        void retainDrawRange(QList<DrawCurve *> &curves, const int localStart, const int localEnd) {
            if (localStart > 0)
                AppModelUtils::eraseDrawCurveRange(curves, 0, localStart);
            int maximumEnd = localEnd;
            for (const auto *curve : curves) {
                if (curve)
                    maximumEnd = std::max(maximumEnd, curve->localEndTick());
            }
            if (maximumEnd > localEnd)
                AppModelUtils::eraseDrawCurveRange(curves, localEnd, maximumEnd);
        }

        QByteArray parameterMutationFingerprint(const QByteArray &operationTag, const ClipId clipId,
                                                const ParamInfo::Name name, const Param::Type type,
                                                const QList<qint64> &scalars,
                                                const QList<int> &values = {}) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(operationTag);
            hashInteger(hash, clipId.value());
            hashInteger(hash, name);
            hashInteger(hash, type);
            hashInteger(hash, scalars.size());
            for (const auto value : scalars)
                hashInteger(hash, value);
            hashInteger(hash, values.size());
            for (const auto value : values)
                hashInteger(hash, value);
            return hash.result();
        }

        SpeakerMixModel::SpeakerMixData
            canonicalVoiceSpeakerMixPayload(const SpeakerMixModel::SpeakerMixData &data) {
            auto result = SpeakerMixModel::normalizeSpeakerMixData(data);
            for (auto &keyframe : result.dynamicKeyframes)
                keyframe.id = 0;
            return result;
        }

        bool sameVoiceContextPayload(const EffectiveVoiceContext &left,
                                     const EffectiveVoiceContext &right) {
            return left.singer == right.singer && left.speaker == right.speaker &&
                   left.followsTrack == right.followsTrack &&
                   canonicalVoiceSpeakerMixPayload(left.speakerMix) ==
                       canonicalVoiceSpeakerMixPayload(right.speakerMix);
        }

        QByteArray speakerMixEditFingerprint(const QByteArray &operationTag, const ClipId clipId,
                                             const QList<qint64> &scalars,
                                             const QVector<double> &weights = {}) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hash.addData(operationTag);
            hashInteger(hash, clipId.value());
            for (const auto scalar : scalars)
                hashInteger(hash, scalar);
            for (const auto weight : weights)
                hash.addData(QByteArray::number(weight, 'g', 17));
            return hash.result();
        }

        AutomationResult<QVector<double>> storedSpeakerMixWeights(const QVector<double> &weights,
                                                                  const int sourceCount) {
            if (sourceCount < 2) {
                return AutomationError::invalidArgument(
                    QStringLiteral("weights"),
                    QStringLiteral("Speaker mix requires at least two sources"));
            }
            for (const auto weight : weights) {
                if (!std::isfinite(weight)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("weights"),
                        QStringLiteral("Speaker mix weights must be finite"));
                }
            }
            if (weights.size() == sourceCount) {
                return SpeakerMixModel::explicitWeightsFromFullWeights(weights);
            }
            if (weights.size() == sourceCount - 1) {
                return SpeakerMixModel::explicitWeightsFromFullWeights(
                    SpeakerMixModel::fullWeightsFromExplicitWeights(weights));
            }
            return AutomationError::invalidArgument(
                QStringLiteral("weights"),
                QStringLiteral("Speaker mix weights do not match the source count"));
        }

        QVector<double> interpolatedSpeakerMixWeights(const SpeakerMixModel::SpeakerMixData &data,
                                                      const int position) {
            const auto &keyframes = data.dynamicKeyframes;
            if (keyframes.isEmpty())
                return {};
            if (position <= keyframes.first().tick)
                return keyframes.first().weights;
            if (position >= keyframes.last().tick)
                return keyframes.last().weights;
            for (int index = 0; index + 1 < keyframes.size(); ++index) {
                const auto &left = keyframes.at(index);
                const auto &right = keyframes.at(index + 1);
                if (position < left.tick || position >= right.tick)
                    continue;
                const auto ratio = static_cast<double>(position - left.tick) /
                                   static_cast<double>(right.tick - left.tick);
                QVector<double> result;
                result.reserve(left.weights.size());
                for (int weightIndex = 0; weightIndex < left.weights.size(); ++weightIndex) {
                    result.append(
                        left.weights.at(weightIndex) +
                        ratio * (right.weights.value(weightIndex) - left.weights.at(weightIndex)));
                }
                return SpeakerMixModel::explicitWeightsFromFullWeights(
                    SpeakerMixModel::fullWeightsFromExplicitWeights(result));
            }
            return keyframes.last().weights;
        }

        bool supportedParameter(const ParamInfo::Name name, const Param::Type type) {
            return name >= ParamInfo::Pitch && name <= ParamInfo::ToneShift &&
                   (type == Param::Original || type == Param::Edited || type == Param::Envelope);
        }

        bool validParameterValue(const ParamInfo::Name name, const int value) {
            const auto spec = ParamInfo::valueSpec(name);
            return value >= spec.minimum && value <= spec.maximum &&
                   (value - spec.minimum) % spec.step == 0;
        }

        bool validInterpolation(const AnchorNode::InterpMode interpolation) {
            return interpolation == AnchorNode::Linear || interpolation == AnchorNode::Hermite ||
                   interpolation == AnchorNode::None;
        }

        AutomationResult<AutomationUnit> validateAnchorDrafts(const QList<AnchorInsertDto> &anchors,
                                                              const QString &fieldPath,
                                                              const int minimumCount = 1) {
            if (anchors.size() < minimumCount) {
                return AutomationError::invalidArgument(
                    fieldPath, QStringLiteral("An anchor curve requires at least two anchors"));
            }
            QSet<int> positions;
            for (const auto &anchor : anchors) {
                if (anchor.position < 0) {
                    return AutomationError::invalidArgument(
                        fieldPath + QStringLiteral(".position"),
                        QStringLiteral("Anchor position must be non-negative"));
                }
                if (!validInterpolation(anchor.interpolation)) {
                    return AutomationError::invalidArgument(
                        fieldPath + QStringLiteral(".interpolation"),
                        QStringLiteral("Interpolation must be Hermite, linear, or step"));
                }
                if (positions.contains(anchor.position)) {
                    return AutomationError::invalidArgument(
                        fieldPath + QStringLiteral(".position"),
                        QStringLiteral("Anchor positions must be unique"));
                }
                positions.insert(anchor.position);
            }
            return AutomationUnit{};
        }

        std::optional<QPair<int, int>> anchorCurveRange(const CurveDraftDto &curve) {
            if (curve.type != CurveDraftDto::Type::Anchor || curve.nodes.isEmpty())
                return std::nullopt;
            auto minimum = curve.nodes.constFirst().position;
            auto maximum = minimum;
            for (const auto &node : curve.nodes) {
                minimum = std::min(minimum, node.position);
                maximum = std::max(maximum, node.position);
            }
            return QPair<int, int>{minimum, maximum};
        }

        bool rangesOverlap(const QPair<int, int> &left, const QPair<int, int> &right) {
            return left.first <= right.second && right.first <= left.second;
        }

        AutomationResult<AutomationUnit>
            validateTargetAnchorRange(const QList<CurveDraftDto> &curves, const CurveId targetId,
                                      const QString &fieldPath) {
            const CurveDraftDto *target = nullptr;
            for (const auto &curve : curves) {
                if (curve.id == targetId) {
                    target = &curve;
                    break;
                }
            }
            if (!target)
                return AutomationError::notFound({ObjectKind::Curve, targetId.value()},
                                                 QStringLiteral("Anchor curve was not found"));
            const auto targetRange = anchorCurveRange(*target);
            if (!targetRange)
                return AutomationError::invalidArgument(
                    fieldPath, QStringLiteral("The target is not a complete anchor curve"));
            for (const auto &curve : curves) {
                if (curve.id == targetId)
                    continue;
                const auto range = anchorCurveRange(curve);
                if (range && rangesOverlap(*targetRange, *range)) {
                    return AutomationError::invalidArgument(
                        fieldPath,
                        QStringLiteral("The anchor curve would overlap another anchor curve"));
                }
            }
            return AutomationUnit{};
        }

        bool validCurveValues(const ParamInfo::Name name, const QList<CurveDraftDto> &curves) {
            for (const auto &curve : curves) {
                for (const auto value : curve.values) {
                    if (!validParameterValue(name, value))
                        return false;
                }
                for (const auto &node : curve.nodes) {
                    if (!validParameterValue(name, node.value))
                        return false;
                }
            }
            return true;
        }
    }

    ParameterAutomationFacade::ParameterAutomationFacade(AutomationDispatcher &dispatcher,
                                                         CommandCommitter &committer,
                                                         DocumentObjectResolver &objects)
        : m_dispatcher(dispatcher), m_committer(committer), m_objects(objects) {
    }

    AutomationResult<ParameterSnapshotDto>
        ParameterAutomationFacade::getParameter(const DocumentId &documentId, const ClipId clipId,
                                                const ParamInfo::Name name,
                                                const Param::Type type) {
        return m_dispatcher.dispatchDocumentQuery<ParameterSnapshotDto>(
            OperationIds::parameters::get, documentId,
            [this, clipId, name, type](DocumentSession &session) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<ParameterSnapshotDto>(resolved.getError());
                if (!supportedParameter(name, type)) {
                    return AutomationResult<ParameterSnapshotDto>(AutomationError::invalidArgument(
                        QStringLiteral("parameter"), QStringLiteral("Parameter is unsupported")));
                }
                const auto *clip = static_cast<const SingingClip *>(resolved.get().clip);
                const auto *param = clip->params.getParamByName(name);
                ParameterSnapshotDto result{session.version(), clipId, name, type, {}};
                for (const auto *curve : param->curves(type)) {
                    if (curve && (curve->type() == Curve::Draw || curve->type() == Curve::Anchor))
                        result.curves.append(curveDraftDto(*curve));
                }
                return AutomationResult<ParameterSnapshotDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::replaceParameter(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const QList<CurveDraftDto> &curves) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::parameters::replace, context,
            [this, clipId, name, type, curves](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (!supportedParameter(name, type)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("parameter"), QStringLiteral("Parameter is unsupported")));
                }
                for (const auto &curve : curves) {
                    if (curve.type != CurveDraftDto::Type::Draw &&
                        curve.type != CurveDraftDto::Type::Anchor) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("curves.type"),
                            QStringLiteral("Curve type is unsupported")));
                    }
                    if (curve.type == CurveDraftDto::Type::Draw && curve.step <= 0) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("curves.step"),
                            QStringLiteral("Curve step must be positive")));
                    }
                    if (curve.type == CurveDraftDto::Type::Draw &&
                        static_cast<qint64>(curve.localStart) +
                                static_cast<qint64>(curve.step) * curve.values.size() >
                            std::numeric_limits<int>::max()) {
                        return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("curves.values"),
                            QStringLiteral("Draw curve range exceeds the supported timeline")));
                    }
                }
                if (!validCurveValues(name, curves)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("curves"),
                        QStringLiteral("Parameter value is outside the editable range")));
                }
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                QList<CurveDraftDto> existing;
                for (const auto *curve : clip->params.getParamByName(name)->curves(type)) {
                    if (curve && (curve->type() == Curve::Draw || curve->type() == Curve::Anchor))
                        existing.append(curveDraftDto(*curve));
                }
                QCryptographicHash oldHash(QCryptographicHash::Sha256);
                QCryptographicHash newHash(QCryptographicHash::Sha256);
                if (hasExplicitCurveIdentity(curves)) {
                    hashCurves(oldHash, existing);
                    hashCurves(newHash, curves);
                } else {
                    hashCurveShapes(oldHash, existing);
                    hashCurveShapes(newHash, curves);
                }
                const bool changed = oldHash.result() != newHash.result();
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                std::vector<std::unique_ptr<Curve>> ownedCurves;
                QList<Curve *> rawCurves;
                ownedCurves.reserve(static_cast<size_t>(curves.size()));
                for (const auto &draft : curves) {
                    auto curve = buildCurve(draft);
                    rawCurves.append(curve.get());
                    ownedCurves.push_back(std::move(curve));
                }
                auto actions = std::make_unique<ParamsActions>();
                actions->replaceParam(name, type, rawCurves, clip);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<ParameterCapabilitiesDto>
        ParameterAutomationFacade::getCapabilities(const DocumentId &documentId,
                                                   const ClipId clipId) {
        return m_dispatcher.dispatchDocumentQuery<ParameterCapabilitiesDto>(
            OperationIds::parameters::get_capabilities, documentId,
            [this, clipId](DocumentSession &session) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<ParameterCapabilitiesDto>(resolved.getError());
                const auto *clip = static_cast<const SingingClip *>(resolved.get().clip);
                ParameterCapabilitiesDto result{session.version(), clipId, {}};
                for (int value = ParamInfo::Pitch; value <= ParamInfo::ToneShift; ++value) {
                    const auto name = static_cast<ParamInfo::Name>(value);
                    if (!clip->params.getParamByName(name))
                        continue;
                    ParameterCapabilityDto capability;
                    capability.name = name;
                    if (ParamInfo::hasOriginalParam(name))
                        capability.types.append(Param::Original);
                    capability.types.append(Param::Edited);
                    capability.types.append(Param::Envelope);
                    capability.supportsDraw = true;
                    capability.supportsAnchor = true;
                    capability.interpolations = {AnchorNode::Hermite, AnchorNode::Linear,
                                                 AnchorNode::None};
                    capability.editable = true;
                    capability.valueSpec = ParamInfo::valueSpec(name);
                    result.parameters.append(std::move(capability));
                }
                return AutomationResult<ParameterCapabilitiesDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::mutateParameter(
        const OperationId &operationId, const CommandContext &context, const ClipId clipId,
        const ParamInfo::Name name, const Param::Type type, CurveMutation mutation,
        QString createdCurveClientRef) {
        return mutateParameterImpl(operationId, context, std::nullopt, clipId, name, type,
                                   std::move(mutation), std::move(createdCurveClientRef));
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::mutateIdempotentParameter(
        const OperationId &operationId, const CommandContext &context,
        const QByteArray &operationTag, const QByteArray &requestFingerprint, const ClipId clipId,
        const ParamInfo::Name name, const Param::Type type, CurveMutation mutation,
        QString createdCurveClientRef) {
        auto isolatedContext = context;
        if (!isolatedContext.idempotencyKey.isEmpty()) {
            isolatedContext.idempotencyKey = QString::fromLatin1(operationTag) + QLatin1Char(':') +
                                             isolatedContext.idempotencyKey;
        }
        return mutateParameterImpl(operationId, isolatedContext, requestFingerprint, clipId, name,
                                   type, std::move(mutation), std::move(createdCurveClientRef));
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::mutateParameterImpl(
        const OperationId &operationId, const CommandContext &context,
        const std::optional<QByteArray> &requestFingerprint, const ClipId clipId,
        const ParamInfo::Name name, const Param::Type type, CurveMutation mutation,
        QString createdCurveClientRef) {
        const AutomationDispatcher::DocumentCommandHandler handler =
            [this, clipId, name, type, mutation = std::move(mutation),
             createdCurveClientRef = std::move(createdCurveClientRef)](DocumentSession &session,
                                                                       const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (!supportedParameter(name, type)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("parameter"), QStringLiteral("Parameter is unsupported")));
                }
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                QList<CurveDraftDto> curves;
                for (const auto *curve : clip->params.getParamByName(name)->curves(type)) {
                    if (curve && (curve->type() == Curve::Draw || curve->type() == Curve::Anchor))
                        curves.append(curveDraftDto(*curve));
                }
                auto mutated = mutation(curves);
                if (!mutated)
                    return AutomationResult<MutationResult>(mutated.getError());
                if (!validCurveValues(name, curves)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("value"),
                        QStringLiteral("Parameter value is outside the editable range")));
                }
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, mutated.get(), affected));
                if (!mutated.get())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));

                std::vector<std::unique_ptr<Curve>> ownedCurves;
                QList<Curve *> rawCurves;
                QList<CreatedObjectRef> createdObjects;
                ownedCurves.reserve(static_cast<size_t>(curves.size()));
                for (const auto &draft : curves) {
                    auto curve = buildCurve(draft);
                    if (!createdCurveClientRef.isEmpty() && !draft.id.isValid() &&
                        curve->type() == Curve::Anchor) {
                        createdObjects.append({
                            createdCurveClientRef, {ObjectKind::Curve, curve->id()}
                        });
                    }
                    rawCurves.append(curve.get());
                    ownedCurves.push_back(std::move(curve));
                }
                auto actions = std::make_unique<ParamsActions>();
                actions->replaceParam(name, type, rawCurves, clip);
                return m_committer.commit(session, std::move(actions), affected,
                                          std::move(createdObjects));
            };
        if (requestFingerprint) {
            return m_dispatcher.dispatchIdempotentDocumentCommand(operationId, context,
                                                                  *requestFingerprint, handler);
        }
        return m_dispatcher.dispatchDocumentCommand(operationId, context, handler);
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::drawParameter(const CommandContext &context, const ClipId clipId,
                                                 const ParamInfo::Name name, const Param::Type type,
                                                 const int localStart, const int step,
                                                 QList<int> values, const bool overlay) {
        return mutateParameter(
            OperationIds::parameters::draw, context, clipId, name, type,
            [localStart, step, values = std::move(values),
             overlay](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                QCryptographicHash beforeHash(QCryptographicHash::Sha256);
                hashCurveShapes(beforeHash, curves);
                const auto beforeDigest = beforeHash.result();
                if (localStart < 0 || step <= 0 || values.size() < 2) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("values"),
                        QStringLiteral("Draw curves require a non-negative start, positive step, "
                                       "and at least two values"));
                }
                const auto computedEnd = static_cast<qint64>(localStart) +
                                         static_cast<qint64>(step) * values.size();
                if (computedEnd > std::numeric_limits<int>::max()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("values"),
                        QStringLiteral("Draw curve range exceeds the supported timeline"));
                }
                const auto localEnd = static_cast<int>(computedEnd);
                QList<DrawCurve *> existing;
                for (const auto &draft : curves) {
                    if (draft.type == CurveDraftDto::Type::Draw)
                        existing.append(static_cast<DrawCurve *>(buildCurve(draft).release()));
                }
                if (!overlay)
                    AppModelUtils::eraseDrawCurveRange(existing, localStart, localEnd);
                DrawCurve incoming;
                incoming.setLocalStart(localStart);
                incoming.step = step;
                incoming.setValues(values);
                QList<DrawCurve *> replacement;
                if (overlay) {
                    replacement = AppModelUtils::mergeCurves(existing, {&incoming});
                } else {
                    replacement = existing;
                    replacement.append(new DrawCurve(incoming));
                    std::sort(replacement.begin(), replacement.end(),
                              [](const auto *left, const auto *right) {
                                  return left->localStart() < right->localStart();
                              });
                    existing.clear();
                }
                QList<CurveDraftDto> result;
                for (const auto &draft : curves) {
                    if (draft.type == CurveDraftDto::Type::Anchor)
                        result.append(draft);
                }
                for (const auto *curve : replacement)
                    result.append(curveDraftDto(*curve));
                qDeleteAll(existing);
                qDeleteAll(replacement);
                curves = std::move(result);
                QCryptographicHash afterHash(QCryptographicHash::Sha256);
                hashCurveShapes(afterHash, curves);
                return beforeDigest != afterHash.result();
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::eraseParameter(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const int localStart, const int localEnd) {
        return mutateParameter(
            OperationIds::parameters::erase, context, clipId, name, type,
            [localStart, localEnd](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                if (localStart < 0 || localEnd <= localStart) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("local_end"),
                        QStringLiteral("Erase range must be non-empty and ordered"));
                }
                QList<DrawCurve *> draws;
                for (const auto &draft : curves) {
                    if (draft.type == CurveDraftDto::Type::Draw)
                        draws.append(static_cast<DrawCurve *>(buildCurve(draft).release()));
                }
                const bool changed =
                    AppModelUtils::eraseDrawCurveRange(draws, localStart, localEnd);
                if (changed) {
                    QList<CurveDraftDto> result;
                    for (const auto &draft : curves) {
                        if (draft.type == CurveDraftDto::Type::Anchor)
                            result.append(draft);
                    }
                    for (const auto *curve : draws)
                        result.append(curveDraftDto(*curve));
                    curves = std::move(result);
                }
                qDeleteAll(draws);
                return changed;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::insertAnchor(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const CurveId curveId, const int position, const int value,
        const AnchorNode::InterpMode interpolation) {
        return insertAnchors(context, clipId, name, type, curveId,
                             {
                                 {position, value, interpolation}
        });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::insertAnchors(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const CurveId curveId, const QList<AnchorInsertDto> &anchors) {
        QByteArray requestFingerprint;
        if (!context.idempotencyKey.isEmpty()) {
            QList<qint64> scalars{curveId.value(), anchors.size()};
            for (const auto &anchor : anchors) {
                scalars.append(anchor.position);
                scalars.append(anchor.value);
                scalars.append(static_cast<qint64>(anchor.interpolation));
            }
            requestFingerprint = parameterMutationFingerprint(QByteArrayLiteral("insert_anchors"),
                                                              clipId, name, type, scalars);
        }
        return mutateIdempotentParameter(
            OperationIds::parameters::insert_anchors, context, QByteArrayLiteral("insert_anchors"),
            requestFingerprint, clipId, name, type,
            [curveId, anchors](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                if (anchors.isEmpty())
                    return false;
                if (!curveId.isValid()) {
                    return AutomationError::invalidArgument(QStringLiteral("curve_id"),
                                                            QStringLiteral("Curve ID is invalid"));
                }
                const auto validation = validateAnchorDrafts(anchors, QStringLiteral("anchors"));
                if (!validation)
                    return validation.getError();
                CurveDraftDto *target = nullptr;
                for (auto &curve : curves) {
                    if (curve.type == CurveDraftDto::Type::Anchor && curve.id == curveId) {
                        target = &curve;
                        break;
                    }
                }
                if (!target) {
                    return AutomationError::notFound({ObjectKind::Curve, curveId.value()},
                                                     QStringLiteral("Anchor curve was not found"));
                }
                QSet<int> positions;
                for (const auto &node : target->nodes)
                    positions.insert(node.position);
                for (const auto &anchor : anchors) {
                    if (positions.contains(anchor.position)) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("anchors.position"),
                            QStringLiteral("An anchor already exists at this position"));
                    }
                    positions.insert(anchor.position);
                    target->nodes.append({anchor.position, anchor.value, anchor.interpolation});
                }
                std::sort(target->nodes.begin(), target->nodes.end(),
                          [](const auto &left, const auto &right) {
                              return left.position < right.position;
                          });
                const auto rangeValidation =
                    validateTargetAnchorRange(curves, curveId, QStringLiteral("anchors.position"));
                if (!rangeValidation)
                    return rangeValidation.getError();
                return true;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::createAnchorCurve(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const QString &clientRef, const QList<AnchorInsertDto> &anchors) {
        if (clientRef.trimmed().isEmpty()) {
            return AutomationError::invalidArgument(QStringLiteral("client_ref"),
                                                    QStringLiteral("Client reference is empty"));
        }
        QByteArray requestFingerprint;
        if (!context.idempotencyKey.isEmpty()) {
            QCryptographicHash fingerprintHash(QCryptographicHash::Sha256);
            fingerprintHash.addData(clientRef.toUtf8());
            hashInteger(fingerprintHash, anchors.size());
            for (const auto &anchor : anchors) {
                hashInteger(fingerprintHash, anchor.position);
                hashInteger(fingerprintHash, anchor.value);
                hashInteger(fingerprintHash, static_cast<qint64>(anchor.interpolation));
            }
            fingerprintHash.addData(parameterMutationFingerprint(
                QByteArrayLiteral("create_anchor_curve"), clipId, name, type, {}));
            requestFingerprint = fingerprintHash.result();
        }
        return mutateIdempotentParameter(
            OperationIds::parameters::create_anchor_curve, context,
            QByteArrayLiteral("create_anchor_curve"), requestFingerprint, clipId, name, type,
            [anchors](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                const auto validation = validateAnchorDrafts(anchors, QStringLiteral("anchors"), 2);
                if (!validation)
                    return validation.getError();
                CurveDraftDto created;
                created.type = CurveDraftDto::Type::Anchor;
                for (const auto &anchor : anchors) {
                    created.nodes.append({anchor.position, anchor.value, anchor.interpolation, {}});
                }
                std::sort(created.nodes.begin(), created.nodes.end(),
                          [](const auto &left, const auto &right) {
                              return left.position < right.position;
                          });
                const auto createdRange = anchorCurveRange(created);
                for (const auto &curve : curves) {
                    const auto existingRange = anchorCurveRange(curve);
                    if (existingRange && rangesOverlap(*createdRange, *existingRange)) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("anchors.position"),
                            QStringLiteral("The anchor curve would overlap another anchor curve"));
                    }
                }
                curves.append(std::move(created));
                return true;
            },
            clientRef);
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::mergeAnchorCurves(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const CurveId targetCurveId, const CurveId sourceCurveId) {
        return mutateParameter(
            OperationIds::parameters::merge_anchor_curves, context, clipId, name, type,
            [targetCurveId, sourceCurveId](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                if (!targetCurveId.isValid() || !sourceCurveId.isValid() ||
                    targetCurveId == sourceCurveId) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("source_curve_id"),
                        QStringLiteral("Target and source curve IDs must be valid and distinct"));
                }
                auto targetIt = curves.end();
                auto sourceIt = curves.end();
                for (auto it = curves.begin(); it != curves.end(); ++it) {
                    if (it->id == targetCurveId)
                        targetIt = it;
                    if (it->id == sourceCurveId)
                        sourceIt = it;
                }
                if (targetIt == curves.end()) {
                    return AutomationError::notFound(
                        {ObjectKind::Curve, targetCurveId.value()},
                        QStringLiteral("Target anchor curve was not found"));
                }
                if (sourceIt == curves.end()) {
                    return AutomationError::notFound(
                        {ObjectKind::Curve, sourceCurveId.value()},
                        QStringLiteral("Source anchor curve was not found"));
                }
                const auto targetRange = anchorCurveRange(*targetIt);
                const auto sourceRange = anchorCurveRange(*sourceIt);
                if (!targetRange || !sourceRange) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("source_curve_id"),
                        QStringLiteral("Both curves must be complete anchor curves"));
                }
                if (rangesOverlap(*targetRange, *sourceRange)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("source_curve_id"),
                        QStringLiteral("Overlapping anchor curves cannot be merged"));
                }
                const auto leftEnd = std::min(targetRange->second, sourceRange->second);
                const auto rightStart = std::max(targetRange->first, sourceRange->first);
                for (const auto &curve : curves) {
                    if (curve.id == targetCurveId || curve.id == sourceCurveId)
                        continue;
                    const auto range = anchorCurveRange(curve);
                    if (range && range->second > leftEnd && range->first < rightStart) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("source_curve_id"),
                            QStringLiteral("Only adjacent anchor curves can be merged"));
                    }
                }
                targetIt->nodes.append(sourceIt->nodes);
                std::sort(targetIt->nodes.begin(), targetIt->nodes.end(),
                          [](const auto &left, const auto &right) {
                              return left.position < right.position;
                          });
                curves.erase(sourceIt);
                return true;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::moveAnchor(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const AnchorId anchorId, const int position, const int value) {
        return moveAnchors(context, clipId, name, type,
                           {
                               {anchorId, position, value}
        });
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::moveAnchors(const CommandContext &context, const ClipId clipId,
                                               const ParamInfo::Name name, const Param::Type type,
                                               const QList<AnchorMoveDto> &moves) {
        return mutateParameter(
            OperationIds::parameters::move_anchors, context, clipId, name, type,
            [moves](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                if (moves.isEmpty())
                    return false;
                QSet<int> requestedIds;
                QHash<int, AnchorMoveDto> requested;
                for (const auto &move : moves) {
                    if (move.position < 0) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("moves.position"),
                            QStringLiteral("Anchor position must be non-negative"));
                    }
                    if (!move.anchorId.isValid() || requestedIds.contains(move.anchorId.value())) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("moves.anchor_id"),
                            QStringLiteral("Anchor IDs must be valid and unique"));
                    }
                    requestedIds.insert(move.anchorId.value());
                    requested.insert(move.anchorId.value(), move);
                }
                QSet<int> found;
                bool changed = false;
                for (auto &curve : curves) {
                    for (auto &node : curve.nodes) {
                        const auto it = requested.constFind(node.id.value());
                        if (it == requested.cend())
                            continue;
                        found.insert(node.id.value());
                        changed |= node.position != it->position || node.value != it->value;
                        node.position = it->position;
                        node.value = it->value;
                    }
                    QSet<int> positions;
                    for (const auto &node : curve.nodes) {
                        if (positions.contains(node.position)) {
                            return AutomationError::invalidArgument(
                                QStringLiteral("moves.position"),
                                QStringLiteral("Moved anchors would overlap"));
                        }
                        positions.insert(node.position);
                    }
                    std::sort(curve.nodes.begin(), curve.nodes.end(),
                              [](const auto &left, const auto &right) {
                                  return left.position < right.position;
                              });
                }
                for (const auto &move : moves) {
                    if (!found.contains(move.anchorId.value())) {
                        return AutomationError::notFound(
                            {ObjectKind::Anchor, move.anchorId.value()},
                            QStringLiteral("Anchor was not found"));
                    }
                }
                for (const auto &curve : curves) {
                    if (curve.type != CurveDraftDto::Type::Anchor)
                        continue;
                    const auto validation = validateTargetAnchorRange(
                        curves, curve.id, QStringLiteral("moves.position"));
                    if (!validation)
                        return validation.getError();
                }
                return changed;
            });
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::removeAnchor(const CommandContext &context, const ClipId clipId,
                                                const ParamInfo::Name name, const Param::Type type,
                                                const AnchorId anchorId) {
        return removeAnchors(context, clipId, name, type, {anchorId});
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::removeAnchors(const CommandContext &context, const ClipId clipId,
                                                 const ParamInfo::Name name, const Param::Type type,
                                                 QList<AnchorId> anchorIds) {
        return mutateParameter(
            OperationIds::parameters::remove_anchors, context, clipId, name, type,
            [anchorIds =
                 std::move(anchorIds)](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                if (anchorIds.isEmpty())
                    return false;
                QSet<int> remaining;
                for (const auto id : anchorIds) {
                    if (!id.isValid() || remaining.contains(id.value())) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("anchor_ids"),
                            QStringLiteral("Anchor IDs must be valid and unique"));
                    }
                    remaining.insert(id.value());
                }
                for (auto curveIt = curves.begin(); curveIt != curves.end(); ++curveIt) {
                    auto &nodes = curveIt->nodes;
                    nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                                               [&remaining](const auto &node) {
                                                   return remaining.remove(node.id.value());
                                               }),
                                nodes.end());
                }
                if (!remaining.isEmpty()) {
                    const auto missing = *remaining.cbegin();
                    return AutomationError::notFound({ObjectKind::Anchor, missing},
                                                     QStringLiteral("Anchor was not found"));
                }
                for (auto curveIt = curves.begin(); curveIt != curves.end();) {
                    if (curveIt->type == CurveDraftDto::Type::Anchor && curveIt->nodes.size() < 2) {
                        curveIt = curves.erase(curveIt);
                    } else {
                        ++curveIt;
                    }
                }
                return true;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::setAnchorInterpolation(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const AnchorId anchorId,
        const AnchorNode::InterpMode interpolation) {
        return setAnchorInterpolations(context, clipId, name, type, {anchorId}, interpolation);
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::setAnchorInterpolations(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, QList<AnchorId> anchorIds,
        const AnchorNode::InterpMode interpolation) {
        return mutateParameter(
            OperationIds::parameters::set_anchor_interpolation, context, clipId, name, type,
            [anchorIds = std::move(anchorIds),
             interpolation](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                if (interpolation != AnchorNode::Linear && interpolation != AnchorNode::Hermite &&
                    interpolation != AnchorNode::None) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("interpolation"),
                        QStringLiteral("Interpolation must be linear, Hermite, or step"));
                }
                QSet<int> remaining;
                for (const auto id : anchorIds) {
                    if (!id.isValid() || remaining.contains(id.value())) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("anchor_ids"),
                            QStringLiteral("Anchor IDs must be valid and unique"));
                    }
                    remaining.insert(id.value());
                }
                bool changed = false;
                for (auto &curve : curves) {
                    for (auto &node : curve.nodes) {
                        if (!remaining.remove(node.id.value()))
                            continue;
                        changed |= node.interpolation != interpolation;
                        node.interpolation = interpolation;
                    }
                }
                if (!remaining.isEmpty()) {
                    const auto missing = *remaining.cbegin();
                    return AutomationError::notFound({ObjectKind::Anchor, missing},
                                                     QStringLiteral("Anchor was not found"));
                }
                return changed;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::bakeParameter(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const std::optional<int> localStart, const std::optional<int> localEnd) {
        const int rangeStart = localStart.value_or(-1);
        const int rangeEnd = localEnd.value_or(-1);
        auto isolatedContext = context;
        if (!isolatedContext.idempotencyKey.isEmpty()) {
            isolatedContext.idempotencyKey =
                QStringLiteral("bake:") + isolatedContext.idempotencyKey;
        }
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::parameters::bake, isolatedContext,
            [this, clipId, name, localStart, localEnd](DocumentSession &session,
                                                       const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                if (!supportedParameter(name, Param::Edited)) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("name"), QStringLiteral("Parameter is unsupported")));
                }
                if (localStart.has_value() != localEnd.has_value() ||
                    (localStart && (*localStart < 0 || *localEnd <= *localStart))) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("local_end"),
                        QStringLiteral(
                            "Bake range must provide an ordered non-negative interval")));
                }
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const auto *param = clip->params.getParamByName(name);
                QList<CurveDraftDto> original;
                QList<CurveDraftDto> edited;
                for (const auto *curve : param->curves(Param::Original))
                    original.append(curveDraftDto(*curve));
                for (const auto *curve : param->curves(Param::Edited))
                    edited.append(curveDraftDto(*curve));

                QList<CurveDraftDto> replacement;
                if (!localStart) {
                    replacement = original;
                    for (auto &curve : replacement)
                        clearCurveIdentity(curve);
                } else {
                    qint64 materializedPoints = 0;
                    for (const auto &draft : edited) {
                        if (draft.type == CurveDraftDto::Type::Anchor) {
                            const int anchorEnd = draft.nodes.isEmpty()
                                                      ? draft.localStart
                                                      : draft.nodes.constLast().position;
                            if (anchorEnd <= *localStart || draft.localStart >= *localEnd)
                                continue;
                        }
                        if (!reserveBakeMaterialization(draft, materializedPoints)) {
                            return AutomationResult<MutationResult>(
                                AutomationError::invalidArgument(
                                    QStringLiteral("local_end"),
                                    QStringLiteral("Bake curve materialization exceeds the "
                                                   "supported point limit")));
                        }
                    }
                    for (const auto &draft : original) {
                        if (!reserveBakeMaterialization(draft, materializedPoints)) {
                            return AutomationResult<MutationResult>(
                                AutomationError::invalidArgument(
                                    QStringLiteral("local_end"),
                                    QStringLiteral("Bake curve materialization exceeds the "
                                                   "supported point limit")));
                        }
                    }

                    QList<DrawCurve *> editedDraws;
                    for (const auto &draft : edited) {
                        if (draft.type == CurveDraftDto::Type::Anchor) {
                            const int anchorEnd = draft.nodes.isEmpty()
                                                      ? draft.localStart
                                                      : draft.nodes.constLast().position;
                            if (anchorEnd <= *localStart || draft.localStart >= *localEnd) {
                                replacement.append(draft);
                                continue;
                            }
                        }
                        if (auto *draw = freshDrawCurve(draft))
                            editedDraws.append(draw);
                    }
                    AppModelUtils::eraseDrawCurveRange(editedDraws, *localStart, *localEnd);

                    QList<DrawCurve *> bakedDraws;
                    for (const auto &draft : original) {
                        if (auto *draw = freshDrawCurve(draft))
                            bakedDraws.append(draw);
                    }
                    retainDrawRange(bakedDraws, *localStart, *localEnd);
                    auto merged = AppModelUtils::mergeCurves(editedDraws, bakedDraws);
                    for (const auto *curve : merged)
                        replacement.append(curveDraftDto(*curve));
                    qDeleteAll(editedDraws);
                    qDeleteAll(bakedDraws);
                    qDeleteAll(merged);
                }
                QCryptographicHash originalHash(QCryptographicHash::Sha256);
                QCryptographicHash editedHash(QCryptographicHash::Sha256);
                hashCurveShapes(originalHash, replacement);
                hashCurveShapes(editedHash, edited);
                const bool changed = originalHash.result() != editedHash.result();
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                std::vector<std::unique_ptr<Curve>> owned;
                QList<Curve *> raw;
                for (const auto &draft : replacement) {
                    auto curve = buildCurve(draft);
                    raw.append(curve.get());
                    owned.push_back(std::move(curve));
                }
                auto actions = std::make_unique<ParamsActions>();
                actions->replaceParam(name, Param::Edited, raw, clip);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::replaceClipSpeakerMix(
        const CommandContext &context, const ClipId clipId,
        const SpeakerMixModel::SpeakerMixData &data) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::speaker_mix::clip::replace, context,
            [this, clipId, data](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const auto previous = clip->effectiveVoiceContext();
                const EffectiveVoiceContext next{
                    previous.singer, previous.speaker,
                    SpeakerMixModel::preservePresetSourceAsDirty(previous.speakerMix, data), false};
                const bool changed = !sameVoiceContextPayload(previous, next);
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                actions->replaceSpeakerMix(data, clip);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::useTrackVoiceContext(const CommandContext &context,
                                                        const ClipId clipId) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::use_track_voice, context,
            [this, clipId](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const bool changed = !clip->usesTrackVoiceContext();
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                actions->useTrackVoiceContext(clip);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::selectClipSingleSpeaker(
        const CommandContext &context, const ClipId clipId, const SingerInfo &singerInfo,
        const SpeakerInfo &speakerInfo) {
        return setClipVoiceContext(OperationIds::clips::set_voice, ClipVoiceAction::SelectSingle,
                                   context, clipId, singerInfo, speakerInfo, {});
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::enableClipDynamicSpeakerMix(
        const CommandContext &context, const ClipId clipId, const SingerInfo &singerInfo,
        const SpeakerInfo &speakerInfo, const SpeakerMixModel::SpeakerMixData &data) {
        return setClipVoiceContext(OperationIds::speaker_mix::clip::enable_dynamic,
                                   ClipVoiceAction::EnableDynamic, context, clipId, singerInfo,
                                   speakerInfo, data);
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::enableClipDynamicSpeakerMix(const CommandContext &context,
                                                               const ClipId clipId) {
        return mutateClipSpeakerMix(
            OperationIds::speaker_mix::enable_dynamic, context, clipId,
            [](SpeakerMixModel::SpeakerMixData &data) -> AutomationResult<bool> {
                data = SpeakerMixModel::normalizeSpeakerMixData(data);
                if (data.mode == SpeakerMixModel::SingerSourceMode::DynamicMix)
                    return false;
                if (data.mode != SpeakerMixModel::SingerSourceMode::FixedMix ||
                    data.sources.size() < 2 ||
                    data.fixedWeights.size() != data.sources.size() - 1) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("clip_id"),
                        QStringLiteral(
                            "Dynamic mix requires a fixed mix with at least two sources"));
                }
                data.mode = SpeakerMixModel::SingerSourceMode::DynamicMix;
                data.dynamicBypassed = false;
                data.dynamicKeyframes = {
                    {0, data.fixedWeights}
                };
                data = SpeakerMixModel::normalizeSpeakerMixData(data);
                return true;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::applyClipSpeakerMix(
        const CommandContext &context, const ClipId clipId, const SingerInfo &singerInfo,
        const SpeakerInfo &speakerInfo, const SpeakerMixModel::SpeakerMixData &data) {
        return setClipVoiceContext(OperationIds::speaker_mix::clip::apply,
                                   ClipVoiceAction::ApplyPreset, context, clipId, singerInfo,
                                   speakerInfo, data);
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::setClipVoiceContext(
        const OperationId &operationId, const ClipVoiceAction action, const CommandContext &context,
        const ClipId clipId, const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
        const SpeakerMixModel::SpeakerMixData &data) {
        return m_dispatcher.dispatchDocumentCommand(
            operationId, context,
            [this, clipId, singerInfo, speakerInfo, data, action](DocumentSession &session,
                                                                  const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const EffectiveVoiceContext next{
                    singerInfo, speakerInfo, SpeakerMixModel::normalizeSpeakerMixData(data), false};
                const bool changed = !sameVoiceContextPayload(clip->effectiveVoiceContext(), next);
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                switch (action) {
                    case ClipVoiceAction::SelectSingle:
                        actions->selectClipSingleSpeaker(singerInfo, speakerInfo, clip);
                        break;
                    case ClipVoiceAction::EnableDynamic:
                        actions->enableClipDynamicSpeakerMix(singerInfo, speakerInfo, data, clip);
                        break;
                    case ClipVoiceAction::ApplyPreset:
                        actions->applyClipSpeakerMixPreset(singerInfo, speakerInfo, data, clip);
                        break;
                }
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::selectTrackSingleSpeaker(
        const CommandContext &context, const TrackId trackId, const SingerInfo &singerInfo,
        const SpeakerInfo &speakerInfo) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::set_voice, context,
            [this, trackId, singerInfo, speakerInfo](DocumentSession &session,
                                                     const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const EffectiveVoiceContext next{singerInfo, speakerInfo, {}, false};
                const bool changed = !sameVoiceContextPayload(track->voiceContext(), next);
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, trackId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                actions->selectTrackSingleSpeaker(singerInfo, speakerInfo, track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::applyTrackSpeakerMix(
        const CommandContext &context, const TrackId trackId, const SingerInfo &singerInfo,
        const SpeakerInfo &speakerInfo, const SpeakerMixModel::SpeakerMixData &data) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::speaker_mix::track::apply, context,
            [this, trackId, singerInfo, speakerInfo, data](DocumentSession &session,
                                                           const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const EffectiveVoiceContext next{
                    singerInfo, speakerInfo, SpeakerMixModel::normalizeSpeakerMixData(data), false};
                const bool changed = !sameVoiceContextPayload(track->voiceContext(), next);
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, trackId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                actions->applyTrackSpeakerMixPreset(singerInfo, speakerInfo, data, track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::replaceTrackSpeakerMix(
        const CommandContext &context, const TrackId trackId,
        const SpeakerMixModel::SpeakerMixData &data) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::speaker_mix::track::replace, context,
            [this, trackId, data](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const auto previous = track->voiceContext();
                const EffectiveVoiceContext next{
                    previous.singer, previous.speaker,
                    SpeakerMixModel::preservePresetSourceAsDirty(previous.speakerMix, data), false};
                const bool changed = !sameVoiceContextPayload(previous, next);
                const auto affected = QList<ObjectRef>{
                    {ObjectKind::Track, trackId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                actions->replaceTrackSpeakerMix(data, track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<SpeakerMixSnapshotDto>
        ParameterAutomationFacade::getSpeakerMix(const DocumentId &documentId,
                                                 const SpeakerMixTargetDto target) {
        return m_dispatcher.dispatchDocumentQuery<SpeakerMixSnapshotDto>(
            OperationIds::speaker_mix::get, documentId, [this, target](DocumentSession &session) {
                SpeakerMixSnapshotDto result;
                result.document = session.version();
                result.target = target;
                if (target.kind == SpeakerMixTargetKind::Track) {
                    auto resolved = m_objects.track(session, TrackId(target.id));
                    if (!resolved)
                        return AutomationResult<SpeakerMixSnapshotDto>(resolved.getError());
                    const auto context = resolved.get()->voiceContext();
                    result.singer = context.singer;
                    result.speaker = context.speaker;
                    result.mix = SpeakerMixModel::normalizeSpeakerMixData(context.speakerMix);
                    return AutomationResult<SpeakerMixSnapshotDto>(std::move(result));
                }
                auto resolved = m_objects.singingClip(session, ClipId(target.id));
                if (!resolved)
                    return AutomationResult<SpeakerMixSnapshotDto>(resolved.getError());
                const auto *clip = static_cast<const SingingClip *>(resolved.get().clip);
                const auto context = clip->effectiveVoiceContext();
                result.singer = context.singer;
                result.speaker = context.speaker;
                result.mix = SpeakerMixModel::normalizeSpeakerMixData(context.speakerMix);
                result.inherited = clip->usesTrackVoiceContext();
                return AutomationResult<SpeakerMixSnapshotDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::setFixedSpeakerMix(
        const CommandContext &context, const SpeakerMixTargetDto target,
        const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
        const SpeakerMixModel::SpeakerMixData &data) {
        const auto normalized = SpeakerMixModel::normalizeSpeakerMixData(data);
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::speaker_mix::set_fixed, context,
            [this, target, singerInfo, speakerInfo, normalized](DocumentSession &session,
                                                                const bool validateOnly) {
                if (normalized.mode == SpeakerMixModel::SingerSourceMode::DynamicMix) {
                    return AutomationResult<MutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("mix"),
                        QStringLiteral("Fixed speaker mix cannot contain dynamic keyframes")));
                }
                const EffectiveVoiceContext next{singerInfo, speakerInfo, normalized, false};
                const QList<ObjectRef> affected{
                    {target.kind == SpeakerMixTargetKind::Track ? ObjectKind::Track
                                                                : ObjectKind::Clip,
                     target.id}
                };
                if (target.kind == SpeakerMixTargetKind::Track) {
                    auto resolved = m_objects.track(session, TrackId(target.id));
                    if (!resolved)
                        return AutomationResult<MutationResult>(resolved.getError());
                    auto *track = resolved.get();
                    const bool changed = track->voiceContext() != next;
                    if (validateOnly)
                        return AutomationResult<MutationResult>(
                            m_committer.preview(session, changed, affected));
                    if (!changed)
                        return AutomationResult<MutationResult>(m_committer.unchanged(session));
                    auto actions = std::make_unique<SpeakerMixActions>();
                    if (normalized.mode == SpeakerMixModel::SingerSourceMode::Single)
                        actions->selectTrackSingleSpeaker(singerInfo, speakerInfo, track);
                    else
                        actions->applyTrackSpeakerMixPreset(singerInfo, speakerInfo, normalized,
                                                            track);
                    return m_committer.commit(session, std::move(actions), affected);
                }
                auto resolved = m_objects.singingClip(session, ClipId(target.id));
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const bool changed =
                    clip->usesTrackVoiceContext() || clip->effectiveVoiceContext() != next;
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                if (normalized.mode == SpeakerMixModel::SingerSourceMode::Single)
                    actions->selectClipSingleSpeaker(singerInfo, speakerInfo, clip);
                else
                    actions->applyClipSpeakerMixPreset(singerInfo, speakerInfo, normalized, clip);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::mutateClipSpeakerMix(
        const OperationId &operationId, const CommandContext &context, const ClipId clipId,
        SpeakerMixMutation mutation) {
        return mutateClipSpeakerMixImpl(operationId, context, clipId, std::nullopt,
                                        std::move(mutation));
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::mutateIdempotentClipSpeakerMix(
        const OperationId &operationId, const CommandContext &context, const ClipId clipId,
        const QByteArray &requestFingerprint, SpeakerMixMutation mutation) {
        return mutateClipSpeakerMixImpl(operationId, context, clipId, requestFingerprint,
                                        std::move(mutation));
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::mutateClipSpeakerMixImpl(
        const OperationId &operationId, const CommandContext &context, const ClipId clipId,
        const std::optional<QByteArray> &requestFingerprint, SpeakerMixMutation mutation) {
        const AutomationDispatcher::DocumentCommandHandler handler =
            [this, clipId, mutation = std::move(mutation)](DocumentSession &session,
                                                           const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                auto data = SpeakerMixModel::normalizeSpeakerMixData(
                    clip->effectiveVoiceContext().speakerMix);
                auto changed = mutation(data);
                if (!changed)
                    return AutomationResult<MutationResult>(changed.getError());
                data = SpeakerMixModel::normalizeSpeakerMixData(data);
                const QList<ObjectRef> affected{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed.get(), affected));
                if (!changed.get())
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                actions->replaceSpeakerMix(data, clip);
                return m_committer.commit(session, std::move(actions), affected);
            };
        if (requestFingerprint) {
            return m_dispatcher.dispatchIdempotentDocumentCommand(operationId, context,
                                                                  *requestFingerprint, handler);
        }
        return m_dispatcher.dispatchDocumentCommand(operationId, context, handler);
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::disableClipDynamicSpeakerMix(const CommandContext &context,
                                                                const ClipId clipId) {
        return mutateClipSpeakerMix(
            OperationIds::speaker_mix::disable_dynamic, context, clipId,
            [](SpeakerMixModel::SpeakerMixData &data) -> AutomationResult<bool> {
                if (data.mode != SpeakerMixModel::SingerSourceMode::DynamicMix ||
                    data.dynamicKeyframes.isEmpty())
                    return false;
                if (data.fixedWeights.size() != data.sources.size() - 1)
                    data.fixedWeights = data.dynamicKeyframes.first().weights;
                data.dynamicKeyframes.clear();
                data.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
                data.dynamicBypassed = false;
                return true;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::setClipDynamicSpeakerMixBypassed(
        const CommandContext &context, const ClipId clipId, const bool bypassed) {
        return mutateClipSpeakerMix(
            OperationIds::speaker_mix::set_dynamic_bypass, context, clipId,
            [bypassed](SpeakerMixModel::SpeakerMixData &data) -> AutomationResult<bool> {
                if (data.mode != SpeakerMixModel::SingerSourceMode::DynamicMix ||
                    data.dynamicKeyframes.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("clip_id"),
                        QStringLiteral("Clip has no dynamic speaker mix"));
                }
                const bool changed = data.dynamicBypassed != bypassed;
                data.dynamicBypassed = bypassed;
                if (bypassed && data.fixedWeights.size() != data.sources.size() - 1)
                    data.fixedWeights = data.dynamicKeyframes.first().weights;
                return changed;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::insertSpeakerMixKeyframe(
        const CommandContext &context, const ClipId clipId, const int position,
        const std::optional<QVector<double>> weights) {
        const auto requestFingerprint =
            context.idempotencyKey.isEmpty()
                ? QByteArray{}
                : speakerMixEditFingerprint(QByteArrayLiteral("insert_keyframe"), clipId,
                                            {position}, weights.value_or(QVector<double>{}));
        return mutateIdempotentClipSpeakerMix(
            OperationIds::speaker_mix::keyframes::insert, context, clipId, requestFingerprint,
            [position, weights](SpeakerMixModel::SpeakerMixData &data) -> AutomationResult<bool> {
                if (data.mode != SpeakerMixModel::SingerSourceMode::DynamicMix ||
                    data.dynamicKeyframes.isEmpty()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("clip_id"),
                        QStringLiteral("Clip has no dynamic speaker mix"));
                }
                if (position < 0) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("position"),
                        QStringLiteral("Keyframe position must be non-negative"));
                }
                for (const auto &keyframe : data.dynamicKeyframes) {
                    if (keyframe.tick == position) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("position"),
                            QStringLiteral("A keyframe already exists at this position"));
                    }
                }
                QVector<double> stored;
                if (weights) {
                    auto converted = storedSpeakerMixWeights(*weights, data.sources.size());
                    if (!converted)
                        return AutomationResult<bool>(converted.getError());
                    stored = converted.get();
                } else {
                    stored = interpolatedSpeakerMixWeights(data, position);
                }
                data.dynamicKeyframes.append({position, stored});
                std::sort(
                    data.dynamicKeyframes.begin(), data.dynamicKeyframes.end(),
                    [](const auto &left, const auto &right) { return left.tick < right.tick; });
                return true;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::moveSpeakerMixKeyframes(
        const CommandContext &context, const ClipId clipId,
        const QList<QPair<SpeakerMixKeyframeId, int>> &moves) {
        QList<qint64> scalars{moves.size()};
        for (const auto &[id, position] : moves) {
            scalars.append(id.value());
            scalars.append(position);
        }
        return mutateClipSpeakerMix(
            OperationIds::speaker_mix::keyframes::move, context, clipId,
            [moves](SpeakerMixModel::SpeakerMixData &data) -> AutomationResult<bool> {
                if (data.mode != SpeakerMixModel::SingerSourceMode::DynamicMix)
                    return AutomationError::invalidArgument(
                        QStringLiteral("clip_id"), QStringLiteral("Dynamic mix is not enabled"));
                QHash<int, int> requested;
                for (const auto &[id, position] : moves) {
                    if (!id.isValid() || requested.contains(id.value()) || position <= 0) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("moves"),
                            QStringLiteral("Keyframe IDs must be unique and positions positive"));
                    }
                    requested.insert(id.value(), position);
                }
                QSet<int> found;
                bool changed = false;
                for (auto &keyframe : data.dynamicKeyframes) {
                    const auto it = requested.constFind(keyframe.id);
                    if (it == requested.cend())
                        continue;
                    if (keyframe.tick == 0) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("moves"),
                            QStringLiteral("The initial keyframe cannot be moved"));
                    }
                    found.insert(keyframe.id);
                    changed |= keyframe.tick != it.value();
                    keyframe.tick = it.value();
                }
                if (found.size() != requested.size()) {
                    return AutomationError::notFound(
                        {ObjectKind::SpeakerMixKeyframe, -1},
                        QStringLiteral("Speaker mix keyframe was not found"));
                }
                QSet<int> positions;
                for (const auto &keyframe : data.dynamicKeyframes) {
                    if (positions.contains(keyframe.tick)) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("moves"),
                            QStringLiteral("Moved keyframes would overlap"));
                    }
                    positions.insert(keyframe.tick);
                }
                std::sort(
                    data.dynamicKeyframes.begin(), data.dynamicKeyframes.end(),
                    [](const auto &left, const auto &right) { return left.tick < right.tick; });
                return changed;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::setSpeakerMixKeyframeWeights(
        const CommandContext &context, const ClipId clipId, const SpeakerMixKeyframeId keyframeId,
        QVector<double> weights) {
        return mutateClipSpeakerMix(
            OperationIds::speaker_mix::keyframes::set_weights, context, clipId,
            [keyframeId, weights = std::move(weights)](
                SpeakerMixModel::SpeakerMixData &data) -> AutomationResult<bool> {
                auto converted = storedSpeakerMixWeights(weights, data.sources.size());
                if (!converted)
                    return AutomationResult<bool>(converted.getError());
                for (auto &keyframe : data.dynamicKeyframes) {
                    if (keyframe.id != keyframeId.value())
                        continue;
                    const bool changed = keyframe.weights != converted.get();
                    keyframe.weights = converted.get();
                    return changed;
                }
                return AutomationError::notFound(
                    {ObjectKind::SpeakerMixKeyframe, keyframeId.value()},
                    QStringLiteral("Speaker mix keyframe was not found"));
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::removeSpeakerMixKeyframes(
        const CommandContext &context, const ClipId clipId,
        QList<SpeakerMixKeyframeId> keyframeIds) {
        QList<qint64> scalars{keyframeIds.size()};
        for (const auto id : keyframeIds)
            scalars.append(id.value());
        return mutateClipSpeakerMix(
            OperationIds::speaker_mix::keyframes::remove, context, clipId,
            [keyframeIds = std::move(keyframeIds)](
                SpeakerMixModel::SpeakerMixData &data) -> AutomationResult<bool> {
                QSet<int> remaining;
                for (const auto id : keyframeIds) {
                    if (!id.isValid() || remaining.contains(id.value())) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("keyframe_ids"),
                            QStringLiteral("Keyframe IDs must be valid and unique"));
                    }
                    remaining.insert(id.value());
                }
                for (const auto &keyframe : data.dynamicKeyframes) {
                    if (remaining.contains(keyframe.id) && keyframe.tick == 0) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("keyframe_ids"),
                            QStringLiteral("The initial keyframe cannot be removed"));
                    }
                }
                data.dynamicKeyframes.erase(std::remove_if(data.dynamicKeyframes.begin(),
                                                           data.dynamicKeyframes.end(),
                                                           [&remaining](const auto &keyframe) {
                                                               return remaining.remove(keyframe.id);
                                                           }),
                                            data.dynamicKeyframes.end());
                if (!remaining.isEmpty()) {
                    return AutomationError::notFound(
                        {ObjectKind::SpeakerMixKeyframe, *remaining.cbegin()},
                        QStringLiteral("Speaker mix keyframe was not found"));
                }
                return !keyframeIds.isEmpty();
            });
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::clearTrackVoice(const CommandContext &context,
                                                   const TrackId trackId) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::tracks::clear_voice, context,
            [this, trackId](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const bool changed = track->voiceContext() != EffectiveVoiceContext{};
                const QList<ObjectRef> affected{
                    {ObjectKind::Track, trackId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                actions->selectTrackSingleSpeaker({}, {}, track);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

    AutomationResult<MutationResult>
        ParameterAutomationFacade::clearClipVoice(const CommandContext &context,
                                                  const ClipId clipId) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::clips::clear_voice, context,
            [this, clipId](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const bool changed = clip->effectiveVoiceContext() != EffectiveVoiceContext{} ||
                                     clip->usesTrackVoiceContext();
                const QList<ObjectRef> affected{
                    {ObjectKind::Clip, clipId.value()}
                };
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, changed, affected));
                if (!changed)
                    return AutomationResult<MutationResult>(m_committer.unchanged(session));
                auto actions = std::make_unique<SpeakerMixActions>();
                actions->selectClipSingleSpeaker({}, {}, clip);
                return m_committer.commit(session, std::move(actions), affected);
            });
    }

} // namespace Automation
