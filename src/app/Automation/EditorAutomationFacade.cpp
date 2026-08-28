#include "EditorAutomationFacade.h"
#include "OperationIds.h"

#include "Global/AppGlobal.h"

#include <lite/History/HistoryFocus.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QSet>

#include <algorithm>
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
                   (state.layout.pianoRollVisible || state.layout.parametersVisible) &&
                   !state.layout.bottomPanelPageId.trimmed().isEmpty() &&
                   (state.layout.activeRegion == EditorViewGlobal::Region::TrackPanel ||
                    state.layout.activeRegion == EditorViewGlobal::Region::PianoRoll ||
                    state.layout.activeRegion == EditorViewGlobal::Region::Parameters) &&
                   (state.layout.focusedRegion == EditorViewGlobal::Region::None ||
                    state.layout.focusedRegion == EditorViewGlobal::Region::TrackPanel ||
                    state.layout.focusedRegion == EditorViewGlobal::Region::PianoRoll ||
                    state.layout.focusedRegion == EditorViewGlobal::Region::Parameters) &&
                   std::isfinite(state.pianoRoll.centerTick) && state.pianoRoll.centerTick >= 0.0 &&
                   std::isfinite(state.pianoRoll.centerKeyIndex) &&
                   state.pianoRoll.centerKeyIndex >= 0.0 &&
                   state.pianoRoll.centerKeyIndex <= 127.0 &&
                   std::isfinite(state.pianoRoll.horizontalScale) &&
                   state.pianoRoll.horizontalScale > 0.0 &&
                   std::isfinite(state.pianoRoll.verticalScale) &&
                   state.pianoRoll.verticalScale > 0.0 &&
                   state.pianoRoll.editMode >= EditorViewGlobal::Select &&
                   state.pianoRoll.editMode <= EditorViewGlobal::BakePitch &&
                   state.parameters.foreground > ParamInfo::Pitch &&
                   state.parameters.foreground < ParamInfo::Unknown &&
                   state.parameters.background >= ParamInfo::Expressiveness &&
                   state.parameters.background != ParamInfo::SpeakerMix &&
                   state.parameters.background <= ParamInfo::Unknown &&
                   state.parameters.editMode >= EditorViewGlobal::ParameterEditMode::Draw &&
                   state.parameters.editMode <= EditorViewGlobal::ParameterEditMode::Anchor &&
                   std::isfinite(state.parameters.centerRatio) &&
                   state.parameters.centerRatio >= 0.0 && state.parameters.centerRatio <= 1.0 &&
                   std::isfinite(state.parameters.verticalScale) &&
                   state.parameters.verticalScale >= 1.0;
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

        bool isValidRegion(const EditorViewGlobal::Region region) {
            return region == EditorViewGlobal::Region::TrackPanel ||
                   region == EditorViewGlobal::Region::PianoRoll ||
                   region == EditorViewGlobal::Region::Parameters;
        }

        ParameterEditorViewState effectiveParameterViewport(ParameterEditorViewState state) {
            const auto span = 1.0 / state.verticalScale;
            const auto minimum = std::clamp(state.centerRatio - span * 0.5, 0.0, 1.0 - span);
            state.centerRatio = minimum + span * 0.5;
            return state;
        }
    }

    EditorAutomationFacade::EditorAutomationFacade(AutomationDispatcher &dispatcher,
                                                   DocumentObjectResolver &objectResolver,
                                                   EditorRuntimeServices services)
        : m_dispatcher(dispatcher), m_objectResolver(objectResolver),
          m_services(std::move(services)) {
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
                    if (stable.primaryClipId >= 0 &&
                        stable.selectedClipIds.contains(stable.primaryClipId)) {
                        result.selection.primaryClipId = ClipId(stable.primaryClipId);
                    }
                    for (const auto id : stable.selectedNoteIds)
                        result.selection.selectedNoteIds.append(NoteId(id));
                    if (stable.primaryNoteId >= 0 &&
                        stable.selectedNoteIds.contains(stable.primaryNoteId)) {
                        result.selection.primaryNoteId = NoteId(stable.primaryNoteId);
                    }
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
            [this](const EditorViewState &target) {
                return m_services.restoreView && m_services.restoreView(target);
            },
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
            [this, tick, trackIndex](const EditorViewState &) {
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
            [this, horizontal, vertical](const EditorViewState &) {
                return m_services.setTrackPanelScale &&
                       m_services.setTrackPanelScale(horizontal, vertical);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setTrackPanelViewport(const GuiCommandContext &context,
                                                      const TrackPanelViewportPatch &patch) {
        std::optional<AutomationError> validationError;
        const auto nonNegative = [](const std::optional<double> value) {
            return !value || (std::isfinite(*value) && *value >= 0.0);
        };
        const auto positive = [](const std::optional<double> value) {
            return !value || (std::isfinite(*value) && *value > 0.0);
        };
        if (patch.empty()) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("viewport"), QStringLiteral("Viewport patch is empty"));
        } else if (!nonNegative(patch.centerTick) || !nonNegative(patch.centerTrackIndex) ||
                   !positive(patch.horizontalScale) || !positive(patch.verticalScale)) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("viewport"), QStringLiteral("Track panel viewport is invalid"));
        }
        return mutateView(
            OperationIds::editor::set_track_panel_viewport, context,
            [patch](EditorViewState &target) {
                if (patch.centerTick)
                    target.trackPanel.centerTick = *patch.centerTick;
                if (patch.centerTrackIndex)
                    target.trackPanel.centerTrackIndex = *patch.centerTrackIndex;
                if (patch.horizontalScale)
                    target.trackPanel.horizontalScale = *patch.horizontalScale;
                if (patch.verticalScale)
                    target.trackPanel.verticalScale = *patch.verticalScale;
            },
            [this](const EditorViewState &target) {
                return m_services.setTrackPanelViewport &&
                       m_services.setTrackPanelViewport(target.trackPanel);
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
            [this, trackVisible, bottomVisible](const EditorViewState &) {
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
            [this, pageId](const EditorViewState &) {
                return m_services.showBottomPanelPage && m_services.showBottomPanelPage(pageId);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::showRegion(const GuiCommandContext &context,
                                           const EditorViewGlobal::Region region) {
        std::optional<AutomationError> validationError;
        if (region != EditorViewGlobal::Region::PianoRoll &&
            region != EditorViewGlobal::Region::Parameters) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("region"), QStringLiteral("Clip editor region is invalid"));
        }
        return mutateView(
            OperationIds::editor::show_region, context,
            [region](EditorViewState &target) {
                target.layout.bottomPanelVisible = true;
                target.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                if (region == EditorViewGlobal::Region::PianoRoll)
                    target.layout.pianoRollVisible = true;
                else
                    target.layout.parametersVisible = true;
                target.layout.activeRegion = region;
            },
            [this, region](const EditorViewState &) {
                return m_services.showRegion && m_services.showRegion(region);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::focusRegion(const GuiCommandContext &context,
                                            const EditorViewGlobal::Region region) {
        std::optional<AutomationError> validationError;
        if (!isValidRegion(region)) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("region"), QStringLiteral("Editor region is invalid"));
        }
        return mutateView(
            OperationIds::editor::focus_region, context,
            [region](EditorViewState &target) {
                if (region == EditorViewGlobal::Region::TrackPanel) {
                    target.layout.trackPanelVisible = true;
                } else {
                    target.layout.bottomPanelVisible = true;
                    target.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                    if (region == EditorViewGlobal::Region::PianoRoll)
                        target.layout.pianoRollVisible = true;
                    else
                        target.layout.parametersVisible = true;
                }
                target.layout.activeRegion = region;
                target.layout.focusedRegion = region;
            },
            [this, region](const EditorViewState &) {
                return m_services.focusRegion && m_services.focusRegion(region);
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
            [this, tick, keyIndex](const EditorViewState &) {
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
            [this, horizontal, vertical](const EditorViewState &) {
                return m_services.setPianoRollScale &&
                       m_services.setPianoRollScale(horizontal, vertical);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setClipEditorTimeViewport(
        const GuiCommandContext &context, const ClipEditorTimeViewportPatch &patch) {
        std::optional<AutomationError> validationError;
        if (patch.empty()) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("viewport"), QStringLiteral("Viewport patch is empty"));
        } else if ((patch.centerTick &&
                    (!std::isfinite(*patch.centerTick) || *patch.centerTick < 0.0)) ||
                   (patch.horizontalScale &&
                    (!std::isfinite(*patch.horizontalScale) || *patch.horizontalScale <= 0.0))) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("viewport"), QStringLiteral("Clip editor time viewport is invalid"));
        }
        return mutateView(
            OperationIds::editor::set_clip_editor_time_viewport, context,
            [patch](EditorViewState &target) {
                if (patch.centerTick)
                    target.pianoRoll.centerTick = *patch.centerTick;
                if (patch.horizontalScale)
                    target.pianoRoll.horizontalScale = *patch.horizontalScale;
            },
            [this](const EditorViewState &target) {
                return m_services.setClipEditorTimeViewport &&
                       m_services.setClipEditorTimeViewport(target.pianoRoll.centerTick,
                                                            target.pianoRoll.horizontalScale);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setPianoRollPitchViewport(
        const GuiCommandContext &context, const PianoRollPitchViewportPatch &patch) {
        std::optional<AutomationError> validationError;
        if (patch.empty()) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("viewport"), QStringLiteral("Viewport patch is empty"));
        } else if ((patch.centerKeyIndex &&
                    (!std::isfinite(*patch.centerKeyIndex) || *patch.centerKeyIndex < 0.0 ||
                     *patch.centerKeyIndex > 127.0)) ||
                   (patch.verticalScale &&
                    (!std::isfinite(*patch.verticalScale) || *patch.verticalScale <= 0.0))) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("viewport"), QStringLiteral("Piano pitch viewport is invalid"));
        }
        return mutateView(
            OperationIds::editor::set_piano_roll_pitch_viewport, context,
            [patch](EditorViewState &target) {
                if (patch.centerKeyIndex)
                    target.pianoRoll.centerKeyIndex = *patch.centerKeyIndex;
                if (patch.verticalScale)
                    target.pianoRoll.verticalScale = *patch.verticalScale;
            },
            [this](const EditorViewState &target) {
                return m_services.setPianoRollPitchViewport &&
                       m_services.setPianoRollPitchViewport(target.pianoRoll.centerKeyIndex,
                                                            target.pianoRoll.verticalScale);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setPianoRollEditMode(
        const GuiCommandContext &context, const EditorViewGlobal::PianoRollEditMode mode) {
        std::optional<AutomationError> validationError;
        if (mode < EditorViewGlobal::Select || mode > EditorViewGlobal::BakePitch)
            validationError = AutomationError::invalidArgument(
                QStringLiteral("mode"), QStringLiteral("Piano roll edit mode is invalid"));
        return mutateView(
            OperationIds::editor::set_piano_roll_edit_mode, context,
            [mode](EditorViewState &target) { target.pianoRoll.editMode = mode; },
            [this, mode](const EditorViewState &) {
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
                    const auto resolved = m_objectResolver.singingClip(session, *clipId);
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
        return setSelectedTrack(context, trackId, false);
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setSelectedTrack(const GuiDocumentCommandContext &context,
                                                 const std::optional<TrackId> trackId,
                                                 const bool focusTrackPanel) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::set_selection, context,
            [this, context, trackId, focusTrackPanel](DocumentSession &session,
                                                      const bool validateOnly) {
                int trackIndex = -1;
                if (trackId) {
                    const auto resolved = m_objectResolver.track(session, *trackId);
                    if (!resolved)
                        return AutomationResult<GuiMutationResult>(resolved.getError());
                    trackIndex = session.model()->tracks().indexOf(resolved.get());
                }
                if (!m_services.captureStableState || !m_services.setSelectedTrackIndex ||
                    (focusTrackPanel && (!m_services.captureView || !m_services.showRegion))) {
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                }
                const bool selectionChanged =
                    m_services.captureStableState().selectedTrackIndex != trackIndex;
                bool presentationChanged = false;
                bool keyboardFocusRequested = false;
                if (focusTrackPanel) {
                    const auto view = m_services.captureView();
                    if (!view)
                        return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                    presentationChanged =
                        !view->layout.trackPanelVisible ||
                        view->layout.activeRegion != EditorViewGlobal::Region::TrackPanel;
                    keyboardFocusRequested =
                        view->layout.focusedRegion != EditorViewGlobal::Region::TrackPanel;
                }
                if (validateOnly) {
                    return AutomationResult<GuiMutationResult>(guiMutation(
                        context.windowId,
                        selectionChanged || presentationChanged || keyboardFocusRequested, true));
                }
                if (focusTrackPanel && presentationChanged &&
                    !m_services.showRegion(EditorViewGlobal::Region::TrackPanel))
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                if (selectionChanged)
                    m_services.setSelectedTrackIndex(trackIndex);
                const bool keyboardFocusChanged =
                    focusTrackPanel && keyboardFocusRequested && m_services.focusRegion &&
                    m_services.focusRegion(EditorViewGlobal::Region::TrackPanel);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId,
                                selectionChanged || presentationChanged || keyboardFocusChanged,
                                false));
            });
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setSelectedClips(const GuiDocumentCommandContext &context,
                                                 const QList<ClipId> &clipIds) {
        const auto primary =
            clipIds.isEmpty() ? std::nullopt : std::optional<ClipId>(clipIds.constLast());
        return setSelectedClips(context, clipIds, primary, false);
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setSelectedClips(
        const GuiDocumentCommandContext &context, const QList<ClipId> &clipIds,
        const std::optional<ClipId> primaryClipId, const bool focusTrackPanel) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::set_selection, context,
            [this, context, clipIds, primaryClipId, focusTrackPanel](DocumentSession &session,
                                                                     const bool validateOnly) {
                for (const auto clipId : clipIds) {
                    const auto resolved = m_objectResolver.clip(session, clipId);
                    if (!resolved)
                        return AutomationResult<GuiMutationResult>(resolved.getError());
                }
                if (primaryClipId && !clipIds.contains(*primaryClipId)) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("primary_clip_id"),
                        QStringLiteral("Primary clip must belong to the selected clip set")));
                }
                if (!m_services.captureStableState || !m_services.setSelectedClips ||
                    (focusTrackPanel && (!m_services.captureView || !m_services.showRegion))) {
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                }
                const auto normalized = normalizedObjectIds(clipIds);
                const auto primary = primaryClipId ? primaryClipId->value() : -1;
                const auto current = m_services.captureStableState();
                const bool selectionChanged =
                    current.selectedClipIds != normalized || current.primaryClipId != primary;
                bool presentationChanged = false;
                bool keyboardFocusRequested = false;
                if (focusTrackPanel) {
                    const auto view = m_services.captureView();
                    if (!view)
                        return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                    presentationChanged =
                        !view->layout.trackPanelVisible ||
                        view->layout.activeRegion != EditorViewGlobal::Region::TrackPanel;
                    keyboardFocusRequested =
                        view->layout.focusedRegion != EditorViewGlobal::Region::TrackPanel;
                }
                if (validateOnly) {
                    return AutomationResult<GuiMutationResult>(guiMutation(
                        context.windowId,
                        selectionChanged || presentationChanged || keyboardFocusRequested, true));
                }
                if (focusTrackPanel && presentationChanged &&
                    !m_services.showRegion(EditorViewGlobal::Region::TrackPanel))
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                if (selectionChanged)
                    m_services.setSelectedClips(normalized, primary);
                const bool keyboardFocusChanged =
                    focusTrackPanel && keyboardFocusRequested && m_services.focusRegion &&
                    m_services.focusRegion(EditorViewGlobal::Region::TrackPanel);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId,
                                selectionChanged || presentationChanged || keyboardFocusChanged,
                                false));
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::clearTrackPanelSelection(
        const GuiDocumentCommandContext &context, const bool clearTrack, const bool clearClips,
        const bool focusTrackPanel) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::set_selection, context,
            [this, context, clearTrack, clearClips, focusTrackPanel](DocumentSession &,
                                                                     const bool validateOnly) {
                if (!clearTrack && !clearClips) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("scope"),
                        QStringLiteral("At least one selection scope must be cleared")));
                }
                if (!m_services.captureStableState ||
                    (clearTrack && !m_services.setSelectedTrackIndex) ||
                    (clearClips && !m_services.setSelectedClips) ||
                    (focusTrackPanel && (!m_services.captureView || !m_services.showRegion))) {
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                }
                const auto current = m_services.captureStableState();
                const bool trackChanged = clearTrack && current.selectedTrackIndex >= 0;
                const bool clipsChanged = clearClips && !current.selectedClipIds.isEmpty();
                bool presentationChanged = false;
                bool keyboardFocusRequested = false;
                if (focusTrackPanel) {
                    const auto view = m_services.captureView();
                    if (!view)
                        return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                    presentationChanged =
                        !view->layout.trackPanelVisible ||
                        view->layout.activeRegion != EditorViewGlobal::Region::TrackPanel;
                    keyboardFocusRequested =
                        view->layout.focusedRegion != EditorViewGlobal::Region::TrackPanel;
                }
                if (validateOnly) {
                    return AutomationResult<GuiMutationResult>(guiMutation(
                        context.windowId, trackChanged || clipsChanged || presentationChanged ||
                                              keyboardFocusRequested,
                        true));
                }
                if (focusTrackPanel && presentationChanged &&
                    !m_services.showRegion(EditorViewGlobal::Region::TrackPanel))
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                if (trackChanged)
                    m_services.setSelectedTrackIndex(-1);
                if (clipsChanged)
                    m_services.setSelectedClips({}, -1);
                const bool keyboardFocusChanged =
                    focusTrackPanel && keyboardFocusRequested && m_services.focusRegion &&
                    m_services.focusRegion(EditorViewGlobal::Region::TrackPanel);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, trackChanged || clipsChanged ||
                                                      presentationChanged || keyboardFocusChanged,
                                false));
            });
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setSelectedNotes(const GuiDocumentCommandContext &context,
                                                 const ClipId clipId,
                                                 const QList<NoteId> &noteIds) {
        const auto primary =
            noteIds.isEmpty() ? std::nullopt : std::optional<NoteId>(noteIds.constLast());
        return setSelectedNotes(context, clipId, noteIds, primary, false);
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setSelectedNotes(
        const GuiDocumentCommandContext &context, const ClipId clipId, const QList<NoteId> &noteIds,
        const std::optional<NoteId> primaryNoteId, const bool focusPianoRoll) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            OperationIds::editor::set_selection, context,
            [this, context, clipId, noteIds, primaryNoteId,
             focusPianoRoll](DocumentSession &session, const bool validateOnly) {
                const auto clip = m_objectResolver.singingClip(session, clipId);
                if (!clip)
                    return AutomationResult<GuiMutationResult>(clip.getError());
                for (const auto noteId : noteIds) {
                    const auto note = m_objectResolver.note(session, clipId, noteId);
                    if (!note)
                        return AutomationResult<GuiMutationResult>(note.getError());
                }
                if (primaryNoteId && !noteIds.contains(*primaryNoteId)) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("primary_note_id"),
                        QStringLiteral("Primary note must belong to the selected note set")));
                }
                if (!m_services.captureStableState || !m_services.setSelectedNotes ||
                    (focusPianoRoll && (!m_services.captureView || !m_services.showRegion))) {
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                }
                const auto normalized = normalizedObjectIds(noteIds);
                const auto primary = primaryNoteId ? primaryNoteId->value() : -1;
                const auto current = m_services.captureStableState();
                const bool selectionChanged = current.activeClipId != clipId.value() ||
                                              current.selectedNoteIds != normalized ||
                                              current.primaryNoteId != primary;
                bool presentationChanged = false;
                bool keyboardFocusRequested = false;
                if (focusPianoRoll) {
                    const auto view = m_services.captureView();
                    if (!view)
                        return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                    presentationChanged =
                        !view->layout.bottomPanelVisible ||
                        view->layout.bottomPanelPageId != QStringLiteral("ClipEditor") ||
                        !view->layout.pianoRollVisible ||
                        view->layout.activeRegion != EditorViewGlobal::Region::PianoRoll;
                    keyboardFocusRequested =
                        view->layout.focusedRegion != EditorViewGlobal::Region::PianoRoll;
                }
                if (validateOnly) {
                    return AutomationResult<GuiMutationResult>(guiMutation(
                        context.windowId,
                        selectionChanged || presentationChanged || keyboardFocusRequested, true));
                }
                if (focusPianoRoll && presentationChanged &&
                    !m_services.showRegion(EditorViewGlobal::Region::PianoRoll))
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                if (selectionChanged)
                    m_services.setSelectedNotes(clipId.value(), normalized, primary);
                const bool keyboardFocusChanged =
                    focusPianoRoll && keyboardFocusRequested && m_services.focusRegion &&
                    m_services.focusRegion(EditorViewGlobal::Region::PianoRoll);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId,
                                selectionChanged || presentationChanged || keyboardFocusChanged,
                                false));
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
        EditorAutomationFacade::setParameterForeground(const GuiDocumentCommandContext &context,
                                                       const ParamInfo::Name name) {
        std::optional<AutomationError> validationError;
        if (name <= ParamInfo::Pitch || name >= ParamInfo::Unknown) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("parameter"), QStringLiteral("Foreground parameter is invalid"));
        }
        return mutateParameterView(
            OperationIds::editor::set_parameter_foreground, context,
            [name](EditorViewState &target) {
                target.parameters.foreground = name;
                target.layout.bottomPanelVisible = true;
                target.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                target.layout.parametersVisible = true;
                target.layout.activeRegion = EditorViewGlobal::Region::Parameters;
            },
            [this, name](const EditorViewState &) {
                return m_services.setParameterForeground && m_services.setParameterForeground(name);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::setParameterBackground(const GuiDocumentCommandContext &context,
                                                       const ParamInfo::Name name) {
        std::optional<AutomationError> validationError;
        if (name < ParamInfo::Expressiveness || name == ParamInfo::SpeakerMix ||
            name > ParamInfo::Unknown) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("parameter"), QStringLiteral("Background parameter is invalid"));
        }
        return mutateParameterView(
            OperationIds::editor::set_parameter_background, context,
            [name](EditorViewState &target) {
                target.parameters.background = name;
                target.layout.bottomPanelVisible = true;
                target.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                target.layout.parametersVisible = true;
                target.layout.activeRegion = EditorViewGlobal::Region::Parameters;
            },
            [this, name](const EditorViewState &) {
                return m_services.setParameterBackground && m_services.setParameterBackground(name);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult>
        EditorAutomationFacade::swapParameters(const GuiDocumentCommandContext &context) {
        return mutateParameterView(
            OperationIds::editor::swap_parameters, context,
            [](EditorViewState &target) {
                if (target.parameters.foreground == ParamInfo::SpeakerMix ||
                    target.parameters.background == ParamInfo::Unknown) {
                    return;
                }
                std::swap(target.parameters.foreground, target.parameters.background);
                target.layout.bottomPanelVisible = true;
                target.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                target.layout.parametersVisible = true;
                target.layout.activeRegion = EditorViewGlobal::Region::Parameters;
            },
            [this](const EditorViewState &target) {
                if (target.parameters.foreground == ParamInfo::SpeakerMix ||
                    target.parameters.background == ParamInfo::Unknown) {
                    return false;
                }
                return m_services.swapParameters && m_services.swapParameters();
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setParameterEditMode(
        const GuiDocumentCommandContext &context, const EditorViewGlobal::ParameterEditMode mode) {
        std::optional<AutomationError> validationError;
        if (mode < EditorViewGlobal::ParameterEditMode::Draw ||
            mode > EditorViewGlobal::ParameterEditMode::Anchor) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("tool"), QStringLiteral("Parameter edit mode is invalid"));
        }
        return mutateParameterView(
            OperationIds::editor::set_parameter_edit_mode, context,
            [mode](EditorViewState &target) {
                target.parameters.editMode = mode;
                target.layout.bottomPanelVisible = true;
                target.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                target.layout.parametersVisible = true;
                target.layout.activeRegion = EditorViewGlobal::Region::Parameters;
            },
            [this, mode](const EditorViewState &) {
                return m_services.setParameterEditMode && m_services.setParameterEditMode(mode);
            },
            std::move(validationError));
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setParameterValueViewport(
        const GuiDocumentCommandContext &context, const ParameterValueViewportPatch &patch) {
        std::optional<AutomationError> validationError;
        if (patch.empty()) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("viewport"), QStringLiteral("Viewport patch is empty"));
        } else if ((patch.centerRatio && (!std::isfinite(*patch.centerRatio) ||
                                          *patch.centerRatio < 0.0 || *patch.centerRatio > 1.0)) ||
                   (patch.verticalScale &&
                    (!std::isfinite(*patch.verticalScale) || *patch.verticalScale < 1.0))) {
            validationError = AutomationError::invalidArgument(
                QStringLiteral("viewport"), QStringLiteral("Parameter value viewport is invalid"));
        }
        return mutateParameterView(
            OperationIds::editor::set_parameter_value_viewport, context,
            [patch](EditorViewState &target) {
                if (patch.centerRatio)
                    target.parameters.centerRatio = *patch.centerRatio;
                if (patch.verticalScale)
                    target.parameters.verticalScale = *patch.verticalScale;
                target.parameters = effectiveParameterViewport(target.parameters);
                target.layout.bottomPanelVisible = true;
                target.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                target.layout.parametersVisible = true;
                target.layout.activeRegion = EditorViewGlobal::Region::Parameters;
            },
            [this](const EditorViewState &target) {
                return m_services.setParameterValueViewport &&
                       m_services.setParameterValueViewport(target.parameters.centerRatio,
                                                            target.parameters.verticalScale);
            },
            std::move(validationError));
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

                if (!m_services.captureView || !m_services.focusVisibility ||
                    !m_services.revealFocus) {
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                }
                const auto beforeView = m_services.captureView();
                if (!beforeView)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                std::optional<EditorStableState> beforeStable;
                if (m_services.captureStableState)
                    beforeStable = m_services.captureStableState();
                if (validateOnly) {
                    const auto visibility = m_services.focusVisibility(focus);
                    bool changed = visibility != HistoryFocusVisibility::Visible;
                    if (target.kind == EditorRevealKind::TrackClips) {
                        changed = changed || !beforeView->layout.trackPanelVisible ||
                                  beforeView->layout.activeRegion !=
                                      EditorViewGlobal::Region::TrackPanel;
                    } else {
                        changed =
                            changed || !beforeView->layout.bottomPanelVisible ||
                            beforeView->layout.bottomPanelPageId != QStringLiteral("ClipEditor") ||
                            !beforeView->layout.pianoRollVisible ||
                            beforeView->layout.activeRegion !=
                                EditorViewGlobal::Region::PianoRoll ||
                            (beforeStable && beforeStable->activeClipId != target.containerId);
                    }
                    return AutomationResult<GuiMutationResult>(
                        guiMutation(context.windowId, changed, true));
                }
                if (!m_services.revealFocus(focus, finalize)) {
                    AutomationError error;
                    error.code = AutomationErrorCode::HostCapabilityUnavailable;
                    error.message = QStringLiteral("Editor could not reveal the requested target");
                    return AutomationResult<GuiMutationResult>(std::move(error));
                }
                const auto afterView = m_services.captureView();
                if (!afterView)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                const auto changed =
                    *afterView != *beforeView || (beforeStable && m_services.captureStableState &&
                                                  m_services.captureStableState() != *beforeStable);
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, changed, false));
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
                if (!validateOnly && changed && (!apply || !apply(target))) {
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

    AutomationResult<GuiMutationResult> EditorAutomationFacade::mutateParameterView(
        const OperationId &operationId, const GuiDocumentCommandContext &context,
        ViewMutation mutation, ViewApply apply, std::optional<AutomationError> validationError) {
        return m_dispatcher.dispatchGuiDocumentCommand<GuiMutationResult>(
            operationId, context,
            [this, context, operationId, mutation = std::move(mutation), apply = std::move(apply),
             validationError = std::move(validationError)](DocumentSession &session,
                                                           const bool validateOnly) {
                if (validationError)
                    return AutomationResult<GuiMutationResult>(*validationError);
                if (!m_services.captureView || !m_services.captureStableState)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());

                const auto stable = m_services.captureStableState();
                if (stable.parameterEditInProgress) {
                    AutomationError error;
                    error.code = AutomationErrorCode::Busy;
                    error.message = QStringLiteral("A parameter edit is already in progress");
                    return AutomationResult<GuiMutationResult>(std::move(error));
                }
                if (stable.activeClipId < 0) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("active_clip_id"),
                        QStringLiteral("An active singing clip is required")));
                }
                const auto clip =
                    m_objectResolver.singingClip(session, ClipId(stable.activeClipId));
                if (!clip)
                    return AutomationResult<GuiMutationResult>(clip.getError());

                const auto current = m_services.captureView();
                if (!current)
                    return AutomationResult<GuiMutationResult>(editorStateUnavailable());
                if ((operationId == OperationIds::editor::set_parameter_edit_mode ||
                     operationId == OperationIds::editor::set_parameter_value_viewport) &&
                    current->parameters.foreground == ParamInfo::SpeakerMix) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("foreground"),
                        QStringLiteral("The operation is unavailable for speaker mix")));
                }
                auto target = *current;
                mutation(target);

                if (operationId == OperationIds::editor::swap_parameters &&
                    (current->parameters.foreground == ParamInfo::SpeakerMix ||
                     current->parameters.background == ParamInfo::Unknown)) {
                    return AutomationResult<GuiMutationResult>(AutomationError::invalidArgument(
                        QStringLiteral("parameters"),
                        QStringLiteral("Current parameter selection cannot be swapped")));
                }

                const bool changed = target != *current;
                if (!validateOnly && changed && (!apply || !apply(target))) {
                    AutomationError error;
                    error.code = AutomationErrorCode::HostCapabilityUnavailable;
                    error.message = QStringLiteral("Parameter editor rejected the requested state");
                    return AutomationResult<GuiMutationResult>(std::move(error));
                }
                return AutomationResult<GuiMutationResult>(
                    guiMutation(context.windowId, changed, validateOnly));
            });
    }

    AutomationResult<EditorCapabilitiesDto> EditorAutomationFacade::getEditorCapabilities() {
        return m_dispatcher.dispatchApplicationQuery<EditorCapabilitiesDto>(
            OperationIds::editor::get_capabilities, [this] {
                EditorCapabilitiesDto result;
                result.operationIds = OperationIds::all();
                return AutomationResult<EditorCapabilitiesDto>(std::move(result));
            });
    }

} // namespace Automation
