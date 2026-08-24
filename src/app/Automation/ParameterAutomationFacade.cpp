#include "ParameterAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/Param/ParamsActions.h"
#include "Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.h"

#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>

#include <QCryptographicHash>
#include <QJsonDocument>

#include <memory>
#include <algorithm>
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

        QByteArray parameterFingerprint(const ClipId clipId, const ParamInfo::Name name,
                                        const Param::Type type,
                                        const QList<CurveDraftDto> &curves) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hashInteger(hash, clipId.value());
            hashInteger(hash, name);
            hashInteger(hash, type);
            hashCurves(hash, curves);
            return hash.result();
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

        void retainDrawRange(QList<DrawCurve *> &curves, const int localStart,
                             const int localEnd) {
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

        QByteArray parameterMutationFingerprint(const QByteArray &operationTag,
                                                const ClipId clipId,
                                                const ParamInfo::Name name,
                                                const Param::Type type,
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

        QByteArray voiceFingerprint(const int objectId, const SingerInfo &singerInfo,
                                    const SpeakerInfo &speakerInfo,
                                    const SpeakerMixModel::SpeakerMixData &data) {
            QCryptographicHash hash(QCryptographicHash::Sha256);
            hashInteger(hash, objectId);
            hash.addData(fingerprint(singerInfo));
            hash.addData(fingerprint(speakerInfo));
            hash.addData(fingerprint(data));
            return hash.result();
        }

        bool supportedParameter(const ParamInfo::Name name, const Param::Type type) {
            return name >= ParamInfo::Pitch && name <= ParamInfo::ToneShift &&
                   (type == Param::Original || type == Param::Edited ||
                    type == Param::Envelope);
        }

        bool validParameterValue(const ParamInfo::Name name, const int value) {
            const auto spec = ParamInfo::valueSpec(name);
            return value >= spec.minimum && value <= spec.maximum &&
                   (value - spec.minimum) % spec.step == 0;
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

    ParameterAutomationFacade::ParameterAutomationFacade(OperationCatalog &catalog,
                                                         AutomationDispatcher &dispatcher,
                                                         CommandCommitter &committer,
                                                         DocumentObjectResolver &objects)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_committer(committer),
          m_objects(objects) {
        registerOperations();
    }

    AutomationResult<ParameterSnapshotDto> ParameterAutomationFacade::getParameter(
        const DocumentId &documentId, const ClipId clipId, const ParamInfo::Name name,
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
            parameterFingerprint(clipId, name, type, curves),
            [this, clipId, name, type, curves](DocumentSession &session,
                                             const bool validateOnly) {
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
                const auto affected = QList<ObjectRef>{{ObjectKind::Clip, clipId.value()}};
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
            OperationIds::parameters::get, documentId,
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
                    capability.interpolations = {
                        AnchorNode::Hermite, AnchorNode::Linear, AnchorNode::None};
                    capability.editable = true;
                    capability.valueSpec = ParamInfo::valueSpec(name);
                    result.parameters.append(std::move(capability));
                }
                return AutomationResult<ParameterCapabilitiesDto>(std::move(result));
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::mutateParameter(
        const CommandContext &context, const QByteArray &operationTag,
        const QByteArray &requestFingerprint, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, CurveMutation mutation) {
        auto isolatedContext = context;
        if (!isolatedContext.idempotencyKey.isEmpty()) {
            isolatedContext.idempotencyKey =
                QString::fromLatin1(operationTag) + QLatin1Char(':') + isolatedContext.idempotencyKey;
        }
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::parameters::replace, isolatedContext, requestFingerprint,
            [this, clipId, name, type,
             mutation = std::move(mutation)](DocumentSession &session, const bool validateOnly) {
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
                const auto affected = QList<ObjectRef>{{ObjectKind::Clip, clipId.value()}};
                if (validateOnly)
                    return AutomationResult<MutationResult>(
                        m_committer.preview(session, mutated.get(), affected));
                if (!mutated.get())
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

    AutomationResult<MutationResult> ParameterAutomationFacade::drawParameter(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const int localStart, const int step, QList<int> values,
        const bool overlay) {
        const auto requestFingerprint = parameterMutationFingerprint(
            QByteArrayLiteral("draw"), clipId, name, type,
            {localStart, step, static_cast<qint64>(overlay)}, values);
        return mutateParameter(
            context, QByteArrayLiteral("draw"), requestFingerprint, clipId, name, type,
            [localStart, step, values = std::move(values), overlay](QList<CurveDraftDto> &curves)
                -> AutomationResult<bool> {
                if (localStart < 0 || step <= 0 || values.size() < 2) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("values"),
                        QStringLiteral("Draw curves require a non-negative start, positive step, "
                                       "and at least two values"));
                }
                const int localEnd = localStart + step * values.size();
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
                    std::sort(replacement.begin(), replacement.end(), [](const auto *left,
                                                                         const auto *right) {
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
                return true;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::eraseParameter(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const int localStart, const int localEnd) {
        const auto requestFingerprint = parameterMutationFingerprint(
            QByteArrayLiteral("erase"), clipId, name, type, {localStart, localEnd});
        return mutateParameter(
            context, QByteArrayLiteral("erase"), requestFingerprint, clipId, name, type,
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
        const Param::Type type, const std::optional<CurveId> curveId, const int position,
        const int value, const AnchorNode::InterpMode interpolation) {
        const auto requestFingerprint = parameterMutationFingerprint(
            QByteArrayLiteral("insert_anchor"), clipId, name, type,
            {curveId ? curveId->value() : -1, position, value,
             static_cast<qint64>(interpolation)});
        return mutateParameter(
            context, QByteArrayLiteral("insert_anchor"), requestFingerprint, clipId, name, type,
            [curveId, position, value,
             interpolation](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                if (position < 0) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("position"),
                        QStringLiteral("Anchor position must be non-negative"));
                }
                CurveDraftDto *target = nullptr;
                for (auto &curve : curves) {
                    if (curve.type != CurveDraftDto::Type::Anchor)
                        continue;
                    if (!curveId || curve.id == *curveId) {
                        target = &curve;
                        break;
                    }
                }
                if (curveId && !target) {
                    return AutomationError::notFound(
                        {ObjectKind::Curve, curveId->value()},
                        QStringLiteral("Anchor curve was not found"));
                }
                if (!target) {
                    CurveDraftDto created;
                    created.type = CurveDraftDto::Type::Anchor;
                    curves.append(std::move(created));
                    target = &curves.last();
                }
                const auto duplicate = std::find_if(target->nodes.cbegin(), target->nodes.cend(),
                                                    [position](const auto &node) {
                                                        return node.position == position;
                                                    });
                if (duplicate != target->nodes.cend()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("position"),
                        QStringLiteral("An anchor already exists at this position"));
                }
                target->nodes.append({position, value, interpolation});
                std::sort(target->nodes.begin(), target->nodes.end(), [](const auto &left,
                                                                        const auto &right) {
                    return left.position < right.position;
                });
                return true;
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::moveAnchor(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const AnchorId anchorId, const int position, const int value) {
        const auto requestFingerprint = parameterMutationFingerprint(
            QByteArrayLiteral("move_anchor"), clipId, name, type,
            {anchorId.value(), position, value});
        return mutateParameter(
            context, QByteArrayLiteral("move_anchor"), requestFingerprint, clipId, name, type,
            [anchorId, position, value](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                if (position < 0) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("position"),
                        QStringLiteral("Anchor position must be non-negative"));
                }
                for (auto &curve : curves) {
                    for (auto &node : curve.nodes) {
                        if (node.id != anchorId)
                            continue;
                        const auto duplicate =
                            std::find_if(curve.nodes.cbegin(), curve.nodes.cend(),
                                         [anchorId, position](const auto &candidate) {
                                             return candidate.id != anchorId &&
                                                    candidate.position == position;
                                         });
                        if (duplicate != curve.nodes.cend()) {
                            return AutomationError::invalidArgument(
                                QStringLiteral("position"),
                                QStringLiteral("An anchor already exists at this position"));
                        }
                        const bool changed = node.position != position || node.value != value;
                        node.position = position;
                        node.value = value;
                        std::sort(curve.nodes.begin(), curve.nodes.end(), [](const auto &left,
                                                                           const auto &right) {
                            return left.position < right.position;
                        });
                        return changed;
                    }
                }
                return AutomationError::notFound({ObjectKind::Anchor, anchorId.value()},
                                                 QStringLiteral("Anchor was not found"));
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::removeAnchor(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const AnchorId anchorId) {
        const auto requestFingerprint = parameterMutationFingerprint(
            QByteArrayLiteral("remove_anchor"), clipId, name, type, {anchorId.value()});
        return mutateParameter(
            context, QByteArrayLiteral("remove_anchor"), requestFingerprint, clipId, name, type,
            [anchorId](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                for (auto curveIt = curves.begin(); curveIt != curves.end(); ++curveIt) {
                    for (auto nodeIt = curveIt->nodes.begin(); nodeIt != curveIt->nodes.end();
                         ++nodeIt) {
                        if (nodeIt->id != anchorId)
                            continue;
                        curveIt->nodes.erase(nodeIt);
                        if (curveIt->nodes.isEmpty())
                            curves.erase(curveIt);
                        return true;
                    }
                }
                return AutomationError::notFound({ObjectKind::Anchor, anchorId.value()},
                                                 QStringLiteral("Anchor was not found"));
            });
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::setAnchorInterpolation(
        const CommandContext &context, const ClipId clipId, const ParamInfo::Name name,
        const Param::Type type, const AnchorId anchorId,
        const AnchorNode::InterpMode interpolation) {
        const auto requestFingerprint = parameterMutationFingerprint(
            QByteArrayLiteral("set_anchor_interpolation"), clipId, name, type,
            {anchorId.value(), static_cast<qint64>(interpolation)});
        return mutateParameter(
            context, QByteArrayLiteral("set_anchor_interpolation"), requestFingerprint, clipId,
            name, type,
            [anchorId,
             interpolation](QList<CurveDraftDto> &curves) -> AutomationResult<bool> {
                for (auto &curve : curves) {
                    for (auto &node : curve.nodes) {
                        if (node.id == anchorId) {
                            const bool changed = node.interpolation != interpolation;
                            node.interpolation = interpolation;
                            return changed;
                        }
                    }
                }
                return AutomationError::notFound({ObjectKind::Anchor, anchorId.value()},
                                                 QStringLiteral("Anchor was not found"));
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
            OperationIds::parameters::replace, isolatedContext,
            parameterMutationFingerprint(QByteArrayLiteral("bake"), clipId, name, Param::Edited,
                                         {rangeStart, rangeEnd}),
            [this, clipId, name, localStart,
             localEnd](DocumentSession &session, const bool validateOnly) {
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
                        QStringLiteral("Bake range must provide an ordered non-negative interval")));
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
                const auto affected = QList<ObjectRef>{{ObjectKind::Clip, clipId.value()}};
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
            voiceFingerprint(clipId.value(), {}, {}, data),
            [this, clipId, data](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const auto previous = clip->effectiveVoiceContext();
                const EffectiveVoiceContext next{
                    previous.singer, previous.speaker,
                    SpeakerMixModel::preservePresetSourceAsDirty(previous.speakerMix, data), false};
                const bool changed = previous != next;
                const auto affected = QList<ObjectRef>{{ObjectKind::Clip, clipId.value()}};
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

    AutomationResult<MutationResult> ParameterAutomationFacade::useTrackVoiceContext(
        const CommandContext &context, const ClipId clipId) {
        return m_dispatcher.dispatchDocumentCommand(
            OperationIds::speaker_mix::clip::use_track, context,
            voiceFingerprint(clipId.value(), {}, {}, {}),
            [this, clipId](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const bool changed = !clip->usesTrackVoiceContext();
                const auto affected = QList<ObjectRef>{{ObjectKind::Clip, clipId.value()}};
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
        return setClipVoiceContext(OperationIds::speaker_mix::clip::select_single,
                                   ClipVoiceAction::SelectSingle, context, clipId, singerInfo,
                                   speakerInfo, {});
    }

    AutomationResult<MutationResult> ParameterAutomationFacade::enableClipDynamicSpeakerMix(
        const CommandContext &context, const ClipId clipId, const SingerInfo &singerInfo,
        const SpeakerInfo &speakerInfo, const SpeakerMixModel::SpeakerMixData &data) {
        return setClipVoiceContext(OperationIds::speaker_mix::clip::enable_dynamic,
                                   ClipVoiceAction::EnableDynamic, context, clipId, singerInfo,
                                   speakerInfo, data);
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
            operationId, context, voiceFingerprint(clipId.value(), singerInfo, speakerInfo, data),
            [this, clipId, singerInfo, speakerInfo, data,
             action](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const EffectiveVoiceContext next{singerInfo, speakerInfo,
                                                 SpeakerMixModel::normalizeSpeakerMixData(data),
                                                 false};
                const bool changed = clip->effectiveVoiceContext() != next;
                const auto affected = QList<ObjectRef>{{ObjectKind::Clip, clipId.value()}};
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
            OperationIds::speaker_mix::track::select_single, context,
            voiceFingerprint(trackId.value(), singerInfo, speakerInfo, {}),
            [this, trackId, singerInfo, speakerInfo](DocumentSession &session,
                                                   const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const EffectiveVoiceContext next{singerInfo, speakerInfo, {}, false};
                const bool changed = track->voiceContext() != next;
                const auto affected = QList<ObjectRef>{{ObjectKind::Track, trackId.value()}};
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
            voiceFingerprint(trackId.value(), singerInfo, speakerInfo, data),
            [this, trackId, singerInfo, speakerInfo,
             data](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const EffectiveVoiceContext next{singerInfo, speakerInfo,
                                                 SpeakerMixModel::normalizeSpeakerMixData(data),
                                                 false};
                const bool changed = track->voiceContext() != next;
                const auto affected = QList<ObjectRef>{{ObjectKind::Track, trackId.value()}};
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
            voiceFingerprint(trackId.value(), {}, {}, data),
            [this, trackId, data](DocumentSession &session, const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const auto previous = track->voiceContext();
                const EffectiveVoiceContext next{
                    previous.singer, previous.speaker,
                    SpeakerMixModel::preservePresetSourceAsDirty(previous.speakerMix, data), false};
                const bool changed = previous != next;
                const auto affected = QList<ObjectRef>{{ObjectKind::Track, trackId.value()}};
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

    void ParameterAutomationFacade::registerOperations() {
        const auto add = [this](OperationDescriptor descriptor) {
            const auto result = m_catalog.add(std::move(descriptor));
            Q_ASSERT(result);
        };
        add({
            .id = OperationIds::parameters::get,
            .category = QStringLiteral("parameters"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        const auto addMutation = [&add](const OperationId &id) {
            add({
                .id = id,
                .category = id.section('.', 0, 0),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::Write,
                .revisionPolicy = RevisionPolicy::Increment,
                .historyPolicy = HistoryPolicy::Record,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::Core,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::DocumentGeneration,
            });
        };
        addMutation(OperationIds::parameters::replace);
        addMutation(OperationIds::speaker_mix::clip::apply);
        addMutation(OperationIds::speaker_mix::clip::enable_dynamic);
        addMutation(OperationIds::speaker_mix::clip::replace);
        addMutation(OperationIds::speaker_mix::clip::select_single);
        addMutation(OperationIds::speaker_mix::clip::use_track);
        addMutation(OperationIds::speaker_mix::track::apply);
        addMutation(OperationIds::speaker_mix::track::replace);
        addMutation(OperationIds::speaker_mix::track::select_single);
    }

} // namespace Automation
