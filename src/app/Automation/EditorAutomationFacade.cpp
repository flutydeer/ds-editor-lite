#include "EditorAutomationFacade.h"
#include "OperationIds.h"

#include "Global/AppGlobal.h"

#include <lite/History/HistoryFocus.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QSet>

#include <cmath>

namespace Automation {
    namespace {
        bool isValidViewState(const EditorViewState &state) {
            return std::isfinite(state.trackPanel.centerTick) &&
                   state.trackPanel.centerTick >= 0.0 &&
                   std::isfinite(state.trackPanel.centerTrackIndex) &&
                   state.trackPanel.centerTrackIndex >= 0.0 &&
                   std::isfinite(state.trackPanel.horizontalScale) &&
                   state.trackPanel.horizontalScale > 0.0 &&
                   std::isfinite(state.trackPanel.verticalScale) &&
                   state.trackPanel.verticalScale > 0.0 &&
                   (state.layout.trackPanelVisible || state.layout.bottomPanelVisible) &&
                   !state.layout.bottomPanelPageId.trimmed().isEmpty() &&
                   std::isfinite(state.pianoRoll.centerTick) && state.pianoRoll.centerTick >= 0.0 &&
                   std::isfinite(state.pianoRoll.centerKeyIndex) &&
                   state.pianoRoll.centerKeyIndex >= 0.0 &&
                   state.pianoRoll.centerKeyIndex <= 127.0 &&
                   std::isfinite(state.pianoRoll.horizontalScale) &&
                   state.pianoRoll.horizontalScale > 0.0 &&
                   std::isfinite(state.pianoRoll.verticalScale) &&
                   state.pianoRoll.verticalScale > 0.0 &&
                   state.pianoRoll.editMode >= EditorViewGlobal::Select &&
                   state.pianoRoll.editMode <= EditorViewGlobal::ModulatePitch;
        }

        AutomationError editorStateUnavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::HostCapabilityUnavailable;
            error.message = QStringLiteral("Editor state is unavailable");
            return error;
        }

        GuiMutationResult guiMutation(const WindowId &windowId, const bool changed,
                                      const bool validateOnly) {
            return {
                .windowId = windowId,
                .changed = changed,
                .validatedOnly = validateOnly,
            };
        }

        template <typename Id>
        QList<int> normalizedObjectIds(const QList<Id> &ids) {
            QList<int> result;
            QSet<int> seen;
            result.reserve(ids.size());
            for (const auto id : ids) {
                if (!seen.contains(id.value())) {
                    seen.insert(id.value());
                    result.append(id.value());
                }
            }
            return result;
        }
    }

    EditorAutomationFacade::EditorAutomationFacade(OperationCatalog &catalog,
                                                   AutomationDispatcher &dispatcher,
                                                   DocumentObjectResolver &objectResolver,
                                                   EditorRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_objectResolver(objectResolver),
          m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<EditorStateDto>
        EditorAutomationFacade::getEditorState(const DocumentId &documentId,
                                               const WindowId &windowId) {
        return m_dispatcher.dispatchGuiDocumentQuery<EditorStateDto>(
            OperationIds::editor::get_state, documentId, windowId,
            [this, windowId](DocumentSession &session) {
                EditorStateDto result;
                result.document = session.version();
                result.windowId = windowId;
                result.projectPath = session.path();
                result.projectName = session.projectName();
                result.documentBusy = session.isBusy();
                if (m_services.captureView)
                    result.view = m_services.captureView();
                if (m_services.captureStableState) {
                    const auto stable = m_services.captureStableState();
                    result.pianoRollQuantize = stable.pianoRollQuantize;
                    result.pianoRollQuantizeEnabled = stable.pianoRollQuantizeEnabled;
                    result.trackAutoPageTurnEnabled = stable.trackAutoPageTurnEnabled;
                    result.pianoRollAutoPageTurnEnabled = stable.pianoRollAutoPageTurnEnabled;
                    if (stable.selectedTrackIndex >= 0 && session.model() &&
                        stable.selectedTrackIndex < session.model()->tracks().size()) {
                        result.selection.selectedTrackId =
                            TrackId(session.model()->tracks().at(stable.selectedTrackIndex)->id());
                    }
                    if (stable.activeClipId >= 0)
                        result.selection.activeClipId = ClipId(stable.activeClipId);
                    for (const auto id : stable.selectedClipIds)
                        result.selection.selectedClipIds.append(ClipId(id));
                    for (const auto id : stable.selectedNoteIds)
                        result.selection.selectedNoteIds.append(NoteId(id));
                }
                return AutomationResult<EditorStateDto>(std::move(result));
            });
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::restoreView(const GuiCommandContext &context,
                                            const EditorViewState &state) {
        std::optional<AutomationError> validationError;
        if (!isValidViewState(state))
            validationError = AutomationError::invalidArgument(
                QStringLiteral("state"), QStringLiteral("Editor view state is invalid"));
        return mutateView(
            OperationIds::editor::restore_view, context,
            [state](EditorViewState &target) { target = state; },
            [this, state] { return m_services.restoreView && m_services.restoreView(state); },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::centerTrackPanel(const GuiCommandContext &context,
                                                 const double tick, const double trackIndex) {
        std::optional<AutomationError> validationError;
        if (!std::isfinite(tick) || tick < 0.0 || !std::isfinite(trackIndex) || trackIndex < 0.0)
            validationError = AutomationError::invalidArgument(
                QStringLiteral("center"), QStringLiteral("Track panel center is invalid"));
        return mutateView(
            OperationIds::editor::center_track_panel, context,
            [tick, trackIndex](EditorViewState &target) {
                target.trackPanel.centerTick = tick;
                target.trackPanel.centerTrackIndex = trackIndex;
            },
            [this, tick, trackIndex] {
                return m_services.centerTrackPanel && m_services.centerTrackPanel(tick, trackIndex);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setTrackPanelScale(const GuiCommandContext &context,
                                                   const double horizontal, const double vertical) {
        std::optional<AutomationError> validationError;
        if (!std::isfinite(horizontal) || horizontal <= 0.0 || !std::isfinite(vertical) ||
            vertical <= 0.0) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("scale"), QStringLiteral("Track panel scale is invalid"));
        }
        return mutateView(
            OperationIds::editor::set_track_panel_scale, context,
            [horizontal, vertical](EditorViewState &target) {
                target.trackPanel.horizontalScale = horizontal;
                target.trackPanel.verticalScale = vertical;
            },
            [this, horizontal, vertical] {
                return m_services.setTrackPanelScale &&
                       m_services.setTrackPanelScale(horizontal, vertical);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setPanelVisibility(
        const GuiCommandContext &context, const bool trackVisible, const bool bottomVisible) {
        std::optional<AutomationError> validationError;
        if (!trackVisible && !bottomVisible)
            validationError = AutomationError::invalidArgument(
                QStringLiteral("visibility"),
                QStringLiteral("At least one panel must remain visible"));
        return mutateView(
            OperationIds::editor::set_panel_visibility, context,
            [trackVisible, bottomVisible](EditorViewState &target) {
                target.layout.trackPanelVisible = trackVisible;
                target.layout.bottomPanelVisible = bottomVisible;
            },
            [this, trackVisible, bottomVisible] {
                return m_services.setPanelVisibility &&
                       m_services.setPanelVisibility(trackVisible, bottomVisible);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::showBottomPanelPage(const GuiCommandContext &context,
                                                    const QString &pageId) {
        std::optional<AutomationError> validationError;
        if (pageId.trimmed().isEmpty())
            validationError = AutomationError::invalidArgument(QStringLiteral("page_id"),
                                                               QStringLiteral("Page ID is empty"));
        return mutateView(
            OperationIds::editor::show_bottom_panel_page, context,
            [pageId](EditorViewState &target) { target.layout.bottomPanelPageId = pageId; },
            [this, pageId] {
                return m_services.showBottomPanelPage && m_services.showBottomPanelPage(pageId);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::centerPianoRoll(const GuiCommandContext &context, const double tick,
                                                const double keyIndex) {
        std::optional<AutomationError> validationError;
        if (!std::isfinite(tick) || tick < 0.0 || !std::isfinite(keyIndex) || keyIndex < 0.0 ||
            keyIndex > 127.0) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("center"), QStringLiteral("Piano roll center is invalid"));
        }
        return mutateView(
            OperationIds::editor::center_piano_roll, context,
            [tick, keyIndex](EditorViewState &target) {
                target.pianoRoll.centerTick = tick;
                target.pianoRoll.centerKeyIndex = keyIndex;
            },
            [this, tick, keyIndex] {
                return m_services.centerPianoRoll && m_services.centerPianoRoll(tick, keyIndex);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setPianoRollScale(const GuiCommandContext &context,
                                                  const double horizontal, const double vertical) {
        std::optional<AutomationError> validationError;
        if (!std::isfinite(horizontal) || horizontal <= 0.0 || !std::isfinite(vertical) ||
            vertical <= 0.0) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("scale"), QStringLiteral("Piano roll scale is invalid"));
        }
        return mutateView(
            OperationIds::editor::set_piano_roll_scale, context,
            [horizontal, vertical](EditorViewState &target) {
                target.pianoRoll.horizontalScale = horizontal;
                target.pianoRoll.verticalScale = vertical;
            },
            [this, horizontal, vertical] {
                return m_services.setPianoRollScale &&
                       m_services.setPianoRollScale(horizontal, vertical);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setPianoRollEditMode(
        const GuiCommandContext &context, const EditorViewGlobal::PianoRollEditMode mode) {
        std::optional<AutomationError> validationError;
        if (mode < EditorViewGlobal::Select || mode > EditorViewGlobal::ModulatePitch)
            validationError = AutomationError::invalidArgument(
                QStringLiteral("mode"), QStringLiteral("Piano roll edit mode is invalid"));
        return mutateView(
            OperationIds::editor::set_piano_roll_edit_mode, context,
            [mode](EditorViewState &target) { target.pianoRoll.editMode = mode; },
            [this, mode] {
                return m_services.setPianoRollEditMode && m_services.setPianoRollEditMode(mode);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setActiveClip(const GuiDocumentCommandContext &context,
                                              const std::optional<ClipId> clipId) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::set_active_clip, context,
            [this, context, clipId](DocumentSession &session, const bool validateOnly) {
                const int value = clipId ? clipId->value() : -1;
                if (clipId) {
                    const auto resolved = m_objectResolver.clip(session, *clipId);
                    if (!resolved)
                        return AutomationResult<GuiMutationResult>(resolved.getError());
                }
                if (!m_services.captureStableState || !m_services.setActiveClip)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                const auto current = m_services.captureStableState();
                const bool changed = current.activeClipId != value;
                if (!validateOnly && changed)
                    m_services.setActiveClip(value);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, changed, validateOnly));
            });
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setSelectedTrack(const GuiDocumentCommandContext &context,
                                                 const std::optional<TrackId> trackId) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::set_selection, context,
            [this, context, trackId](DocumentSession &session, const bool validateOnly) {
                int trackIndex = -1;
                if (trackId) {
                    const auto resolved = m_objectResolver.track(session, *trackId);
                    if (!resolved)
                        return AutomationResult<GuiMutationResult>(resolved.getError());
                    trackIndex = session.model()->tracks().indexOf(resolved.get());
                }
                if (!m_services.captureStableState || !m_services.setSelectedTrackIndex)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                const bool changed =
                    m_services.captureStableState().selectedTrackIndex != trackIndex;
                if (!validateOnly && changed)
                    m_services.setSelectedTrackIndex(trackIndex);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, changed, validateOnly));
            });
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setSelectedClips(const GuiDocumentCommandContext &context,
                                                 const QList<ClipId> &clipIds) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::set_selection, context,
            [this, context, clipIds](DocumentSession &session, const bool validateOnly) {
                for (const auto clipId : clipIds) {
                    const auto resolved = m_objectResolver.clip(session, clipId);
                    if (!resolved)
                        return AutomationResult<GuiMutationResult>(resolved.getError());
                }
                if (!m_services.captureStableState || !m_services.setSelectedClips)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                const auto normalized = normalizedObjectIds(clipIds);
                const bool changed = m_services.captureStableState().selectedClipIds != normalized;
                if (!validateOnly && changed)
                    m_services.setSelectedClips(normalized);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, changed, validateOnly));
            });
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setSelectedNotes(const GuiDocumentCommandContext &context,
                                                 const ClipId clipId,
                                                 const QList<NoteId> &noteIds) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::set_selection, context,
            [this, context, clipId, noteIds](DocumentSession &session, const bool validateOnly) {
                const auto clip = m_objectResolver.singingClip(session, clipId);
                if (!clip)
                    return AutomationResult<GuiMutationResult>(clip.getError());
                for (const auto noteId : noteIds) {
                    const auto note = m_objectResolver.note(session, clipId, noteId);
                    if (!note)
                        return AutomationResult<GuiMutationResult>(note.getError());
                }
                if (!m_services.captureStableState || !m_services.setSelectedNotes)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                const auto normalized = normalizedObjectIds(noteIds);
                const auto current = m_services.captureStableState();
                const bool changed =
                    current.activeClipId != clipId.value() || current.selectedNoteIds != normalized;
                if (!validateOnly && changed)
                    m_services.setSelectedNotes(clipId.value(), normalized);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, changed, validateOnly));
            });
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setPianoRollQuantize(const GuiCommandContext &context,
                                                     const int quantize, const bool enabled) {
        return m_dispatcher.dispatchGuiCommand<GuiMutationResult>(
            OperationIds::editor::set_quantize, context,
            [this, context, quantize, enabled](const bool validateOnly) {
                if (quantize <= 0 || AppGlobal::ticksPerWholeNote % quantize != 0) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("quantize"),
                        QStringLiteral(
                            "Quantize must divide the number of ticks in a whole note")));
                }
                if (!m_services.captureStableState || !m_services.setPianoRollQuantize)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                const auto current = m_services.captureStableState();
                const bool changed = current.pianoRollQuantize != quantize ||
                                     current.pianoRollQuantizeEnabled != enabled;
                if (!validateOnly && changed)
                    m_services.setPianoRollQuantize(quantize, enabled);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, changed, validateOnly));
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setAutoPageTurn(
        const GuiCommandContext &context, const EditorAutoPageTarget target, const bool enabled) {
        return m_dispatcher.dispatchGuiCommand<GuiMutationResult>(
            OperationIds::editor::set_auto_page_turn, context,
            [this, context, target, enabled](const bool validateOnly) {
                if (target != EditorAutoPageTarget::TrackPanel &&
                    target != EditorAutoPageTarget::PianoRoll) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("target"), QStringLiteral("Auto page target is invalid")));
                }
                if (!m_services.captureStableState || !m_services.setAutoPageTurn)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                const auto current = m_services.captureStableState();
                const bool previous = target == EditorAutoPageTarget::TrackPanel
                                          ? current.trackAutoPageTurnEnabled
                                          : current.pianoRollAutoPageTurnEnabled;
                const bool changed = previous != enabled;
                if (!validateOnly && changed)
                    m_services.setAutoPageTurn(target, enabled);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, changed, validateOnly));
            });
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::reveal(const GuiDocumentCommandContext &context,
                                       const EditorRevealDto &target, const bool finalize) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::reveal, context,
            [this, context, target, finalize](DocumentSession &session, const bool validateOnly) {
                if (!std::isfinite(target.tickStart) || !std::isfinite(target.tickEnd) ||
                    !std::isfinite(target.valueStart) || !std::isfinite(target.valueEnd) ||
                    target.tickStart > target.tickEnd || target.valueStart > target.valueEnd) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("target"), QStringLiteral("Reveal range is invalid")));
                }

                HistoryFocus focus;
                focus.objectIds = target.objectIds;
                focus.containerId = target.containerId;
                focus.trackId = target.trackId;
                focus.trackIndex = target.trackIndex;
                focus.tickStart = target.tickStart;
                focus.tickEnd = target.tickEnd;
                focus.valueStart = target.valueStart;
                focus.valueEnd = target.valueEnd;
                focus.ticksAreLocal = target.ticksAreLocal;

                if (target.kind == EditorRevealKind::TrackClips) {
                    focus.kind = HistoryFocusKind::TrackClips;
                    if (target.trackId >= 0) {
                        const auto track = m_objectResolver.track(session, TrackId(target.trackId));
                        if (!track)
                            return AutomationResult<GuiMutationResult>(track.getError());
                    } else if (target.trackIndex >= session.model()->tracks().size()) {
                        return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("track_index"),
                            QStringLiteral("Reveal track index is out of range")));
                    }
                    for (const auto id : target.objectIds) {
                        const auto clip = m_objectResolver.clip(session, ClipId(id));
                        if (!clip && !target.allowRangeFallback)
                            return AutomationResult<GuiMutationResult>(clip.getError());
                    }
                    if (target.objectIds.isEmpty() && target.trackId < 0 && target.trackIndex < 0) {
                        return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                            QStringLiteral("target"),
                            QStringLiteral("Reveal target does not identify a track or clip")));
                    }
                } else if (target.kind == EditorRevealKind::PianoRollNotes) {
                    focus.kind = HistoryFocusKind::PianoRollNotes;
                    const auto clip =
                        m_objectResolver.singingClip(session, ClipId(target.containerId));
                    if (!clip)
                        return AutomationResult<GuiMutationResult>(clip.getError());
                    for (const auto id : target.objectIds) {
                        const auto note =
                            m_objectResolver.note(session, ClipId(target.containerId), NoteId(id));
                        if (!note && !target.allowRangeFallback)
                            return AutomationResult<GuiMutationResult>(note.getError());
                    }
                } else {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("kind"), QStringLiteral("Reveal kind is unsupported")));
                }

                if (!m_services.revealFocus)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                if (!validateOnly && !m_services.revealFocus(focus, finalize)) {
                    AutomationError error;
                    error.code = AutomationErrorCode::HostCapabilityUnavailable;
                    error.message = QStringLiteral("Editor could not reveal the requested target");
                    return AutomationResult<GuiMutationResult>(std::move(error));
                }
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, true, validateOnly));
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::mutateView(
        const OperationId &operationId, const GuiCommandContext &context, ViewMutation mutation,
        ViewApply apply, std::optional<AutomationError> validationError) {
        return m_dispatcher.dispatchGuiCommand<GuiMutationResult>(
            operationId, context,
            [this, context, mutation = std::move(mutation), apply = std::move(apply),
             validationError = std::move(validationError)](const bool validateOnly) {
                if (validationError)
                    return AutomationResult<GuiMutationResult>(*validationError);
                if (!m_services.captureView) {
                    AutomationError error;
                    error.code = AutomationErrorCode::HostCapabilityUnavailable;
                    error.message = QStringLiteral("Editor view is unavailable");
                    return AutomationResult<GuiMutationResult>(std::move(error));
                }
                const auto current = m_services.captureView();
                if (!current) {
                    AutomationError error;
                    error.code = AutomationErrorCode::HostCapabilityUnavailable;
                    error.message = QStringLiteral("Editor view is unavailable");
                    return AutomationResult<GuiMutationResult>(std::move(error));
                }
                auto target = *current;
                mutation(target);
                const bool changed = target != *current;
                if (!validateOnly && changed && (!apply || !apply())) {
                    AutomationError error;
                    error.code = AutomationErrorCode::HostCapabilityUnavailable;
                    error.message = QStringLiteral("Editor view rejected the requested state");
                    return AutomationResult<GuiMutationResult>(std::move(error));
                }
                return AutomationResult<GuiMutationResult>({
                    .windowId = context.windowId,
                    .changed = changed,
                    .validatedOnly = validateOnly,
                });
            });
    }

    AutomationResult<EditorCapabilitiesDto> EditorAutomationFacade::getEditorCapabilities() {
        return m_dispatcher.dispatchApplicationQuery<EditorCapabilitiesDto>(
            OperationIds::editor::get_capabilities, [this] {
                EditorCapabilitiesDto result;
                result.operationIds = m_catalog.operationIds();
                return AutomationResult<EditorCapabilitiesDto>(std::move(result));
            });
    }

    void EditorAutomationFacade::registerOperations() {
        m_catalog.add({
            .id = OperationIds::editor::get_capabilities,
            .category = QStringLiteral("editor"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        m_catalog.add({
            .id = OperationIds::editor::get_state,
            .category = QStringLiteral("editor"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::GuiOnly,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        const auto addGuiCommand = [this](const OperationId &id) {
            const auto result = m_catalog.add({
                .id = id,
                .category = QStringLiteral("editor"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::None,
                .revisionPolicy = RevisionPolicy::None,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::GuiOnly,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
            Q_ASSERT(result);
        };
        const auto addGuiDocumentCommand = [this](const OperationId &id) {
            const auto result = m_catalog.add({
                .id = id,
                .category = QStringLiteral("editor"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .documentPolicy = DocumentPolicy::Read,
                .revisionPolicy = RevisionPolicy::Check,
                .historyPolicy = HistoryPolicy::None,
                .fileAccess = FileAccessPolicy::None,
                .hostAvailability = HostAvailability::GuiOnly,
                .safety = SafetyClass::Reversible,
                .exposure = ExposurePolicy::InternalOnly,
                .idempotency = IdempotencyPolicy::Unsupported,
            });
            Q_ASSERT(result);
        };
        addGuiCommand(OperationIds::editor::center_piano_roll);
        addGuiCommand(OperationIds::editor::center_track_panel);
        addGuiCommand(OperationIds::editor::restore_view);
        addGuiCommand(OperationIds::editor::set_panel_visibility);
        addGuiCommand(OperationIds::editor::set_piano_roll_edit_mode);
        addGuiCommand(OperationIds::editor::set_piano_roll_scale);
        addGuiCommand(OperationIds::editor::set_track_panel_scale);
        addGuiCommand(OperationIds::editor::show_bottom_panel_page);
        addGuiCommand(OperationIds::editor::set_auto_page_turn);
        addGuiCommand(OperationIds::editor::set_quantize);
        addGuiDocumentCommand(OperationIds::editor::reveal);
        addGuiDocumentCommand(OperationIds::editor::set_active_clip);
        addGuiDocumentCommand(OperationIds::editor::set_selection);
    }

} // namespace Automation
