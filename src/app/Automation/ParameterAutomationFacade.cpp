#include "ParameterAutomationFacade.h"
#include "OperationIds.h"

#include "Controller/Actions/AppModel/Param/ParamsActions.h"
#include "Controller/Actions/AppModel/SpeakerMix/SpeakerMixActions.h"

#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCryptographicHash>
#include <QJsonDocument>

#include <memory>
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
                   (type == Param::Original || type == Param::Edited || type == Param::Envelope);
        }
    }

    ParameterAutomationFacade::ParameterAutomationFacade(OperationCatalog &catalog,
                                                         AutomationDispatcher &dispatcher,
                                                         CommandCommitter &committer,
                                                         DocumentObjectResolver &objects)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_committer(committer), m_objects(objects) {
        registerOperations();
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
            parameterFingerprint(clipId, name, type, curves),
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
                }
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                QList<CurveDraftDto> existing;
                for (const auto *curve : clip->params.getParamByName(name)->curves(type)) {
                    if (curve && (curve->type() == Curve::Draw || curve->type() == Curve::Anchor))
                        existing.append(curveDraftDto(*curve));
                }
                QCryptographicHash oldHash(QCryptographicHash::Sha256);
                QCryptographicHash newHash(QCryptographicHash::Sha256);
                hashCurves(oldHash, existing);
                hashCurves(newHash, curves);
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
            OperationIds::speaker_mix::clip::use_track, context,
            voiceFingerprint(clipId.value(), {}, {}, {}),
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
            [this, clipId, singerInfo, speakerInfo, data, action](DocumentSession &session,
                                                                  const bool validateOnly) {
                auto resolved = m_objects.singingClip(session, clipId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *clip = static_cast<SingingClip *>(resolved.get().clip);
                const EffectiveVoiceContext next{
                    singerInfo, speakerInfo, SpeakerMixModel::normalizeSpeakerMixData(data), false};
                const bool changed = clip->effectiveVoiceContext() != next;
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
            voiceFingerprint(trackId.value(), singerInfo, speakerInfo, data),
            [this, trackId, singerInfo, speakerInfo, data](DocumentSession &session,
                                                           const bool validateOnly) {
                auto resolved = m_objects.track(session, trackId);
                if (!resolved)
                    return AutomationResult<MutationResult>(resolved.getError());
                auto *track = resolved.get();
                const EffectiveVoiceContext next{
                    singerInfo, speakerInfo, SpeakerMixModel::normalizeSpeakerMixData(data), false};
                const bool changed = track->voiceContext() != next;
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
