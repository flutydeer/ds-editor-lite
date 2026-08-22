#include "EditorAutomationFacade.h"

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
                   state.pianoRoll.centerKeyIndex >= 0.0 && state.pianoRoll.centerKeyIndex <= 127.0 &&
                   std::isfinite(state.pianoRoll.horizontalScale) &&
                   state.pianoRoll.horizontalScale > 0.0 &&
                   std::isfinite(state.pianoRoll.verticalScale) &&
                   state.pianoRoll.verticalScale > 0.0 &&
                   state.pianoRoll.editMode >= EditorViewGlobal::Select &&
                   state.pianoRoll.editMode <= EditorViewGlobal::BakePitch;
        }
    }

    EditorAutomationFacade::EditorAutomationFacade(DocumentSession &session,
                                                   SingleWindowContext &windowContext,
                                                   OperationCatalog &catalog,
                                                   AutomationDispatcher &dispatcher,
                                                   EditorRuntimeServices services)
        : m_session(session), m_windowContext(windowContext), m_catalog(catalog),
          m_dispatcher(dispatcher), m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<EditorStateDto>
    EditorAutomationFacade::getEditorState(const WindowId &windowId) {
        return m_dispatcher.dispatchGuiQuery<EditorStateDto>(
            QStringLiteral("editor.get_state"), windowId, [this] {
                EditorStateDto result;
                result.document = m_session.version();
                result.windowId = m_windowContext.windowId();
                result.projectPath = m_session.path();
                result.projectName = m_session.projectName();
                result.documentBusy = m_session.isBusy();
                if (m_services.captureView)
                    result.view = m_services.captureView();
                return AutomationResult<EditorStateDto>(std::move(result));
            });
    }

    AutomationResult<GuiMutationResult>
    EditorAutomationFacade::restoreView(const GuiCommandContext &context,
                                        const EditorViewState &state) {
        if (!isValidViewState(state)) {
            return AutomationError::invalidArgument(
                QStringLiteral("state"), QStringLiteral("Editor view state is invalid"));
        }
        return mutateView(QStringLiteral("editor.restore_view"), context,
                          [state](EditorViewState &target) { target = state; },
                          [this, state] {
                              return m_services.restoreView && m_services.restoreView(state);
                          });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::centerTrackPanel(
        const GuiCommandContext &context, const double tick, const double trackIndex) {
        if (!std::isfinite(tick) || tick < 0.0 || !std::isfinite(trackIndex) || trackIndex < 0.0) {
            return AutomationError::invalidArgument(
                QStringLiteral("center"), QStringLiteral("Track panel center is invalid"));
        }
        return mutateView(
            QStringLiteral("editor.center_track_panel"), context,
            [tick, trackIndex](EditorViewState &target) {
                target.trackPanel.centerTick = tick;
                target.trackPanel.centerTrackIndex = trackIndex;
            },
            [this, tick, trackIndex] {
                return m_services.centerTrackPanel &&
                       m_services.centerTrackPanel(tick, trackIndex);
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setTrackPanelScale(
        const GuiCommandContext &context, const double horizontal, const double vertical) {
        if (!std::isfinite(horizontal) || horizontal <= 0.0 || !std::isfinite(vertical) ||
            vertical <= 0.0) {
            return AutomationError::invalidArgument(
                QStringLiteral("scale"), QStringLiteral("Track panel scale is invalid"));
        }
        return mutateView(
            QStringLiteral("editor.set_track_panel_scale"), context,
            [horizontal, vertical](EditorViewState &target) {
                target.trackPanel.horizontalScale = horizontal;
                target.trackPanel.verticalScale = vertical;
            },
            [this, horizontal, vertical] {
                return m_services.setTrackPanelScale &&
                       m_services.setTrackPanelScale(horizontal, vertical);
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setPanelVisibility(
        const GuiCommandContext &context, const bool trackVisible, const bool bottomVisible) {
        if (!trackVisible && !bottomVisible) {
            return AutomationError::invalidArgument(
                QStringLiteral("visibility"), QStringLiteral("At least one panel must remain visible"));
        }
        return mutateView(
            QStringLiteral("editor.set_panel_visibility"), context,
            [trackVisible, bottomVisible](EditorViewState &target) {
                target.layout.trackPanelVisible = trackVisible;
                target.layout.bottomPanelVisible = bottomVisible;
            },
            [this, trackVisible, bottomVisible] {
                return m_services.setPanelVisibility &&
                       m_services.setPanelVisibility(trackVisible, bottomVisible);
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::showBottomPanelPage(
        const GuiCommandContext &context, const QString &pageId) {
        if (pageId.trimmed().isEmpty()) {
            return AutomationError::invalidArgument(QStringLiteral("page_id"),
                                                    QStringLiteral("Page ID is empty"));
        }
        return mutateView(
            QStringLiteral("editor.show_bottom_panel_page"), context,
            [pageId](EditorViewState &target) { target.layout.bottomPanelPageId = pageId; },
            [this, pageId] {
                return m_services.showBottomPanelPage && m_services.showBottomPanelPage(pageId);
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::centerPianoRoll(
        const GuiCommandContext &context, const double tick, const double keyIndex) {
        if (!std::isfinite(tick) || tick < 0.0 || !std::isfinite(keyIndex) || keyIndex < 0.0 ||
            keyIndex > 127.0) {
            return AutomationError::invalidArgument(
                QStringLiteral("center"), QStringLiteral("Piano roll center is invalid"));
        }
        return mutateView(
            QStringLiteral("editor.center_piano_roll"), context,
            [tick, keyIndex](EditorViewState &target) {
                target.pianoRoll.centerTick = tick;
                target.pianoRoll.centerKeyIndex = keyIndex;
            },
            [this, tick, keyIndex] {
                return m_services.centerPianoRoll && m_services.centerPianoRoll(tick, keyIndex);
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setPianoRollScale(
        const GuiCommandContext &context, const double horizontal, const double vertical) {
        if (!std::isfinite(horizontal) || horizontal <= 0.0 || !std::isfinite(vertical) ||
            vertical <= 0.0) {
            return AutomationError::invalidArgument(
                QStringLiteral("scale"), QStringLiteral("Piano roll scale is invalid"));
        }
        return mutateView(
            QStringLiteral("editor.set_piano_roll_scale"), context,
            [horizontal, vertical](EditorViewState &target) {
                target.pianoRoll.horizontalScale = horizontal;
                target.pianoRoll.verticalScale = vertical;
            },
            [this, horizontal, vertical] {
                return m_services.setPianoRollScale &&
                       m_services.setPianoRollScale(horizontal, vertical);
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::setPianoRollEditMode(
        const GuiCommandContext &context, const EditorViewGlobal::PianoRollEditMode mode) {
        if (mode < EditorViewGlobal::Select || mode > EditorViewGlobal::BakePitch) {
            return AutomationError::invalidArgument(
                QStringLiteral("mode"), QStringLiteral("Piano roll edit mode is invalid"));
        }
        return mutateView(
            QStringLiteral("editor.set_piano_roll_edit_mode"), context,
            [mode](EditorViewState &target) { target.pianoRoll.editMode = mode; },
            [this, mode] {
                return m_services.setPianoRollEditMode && m_services.setPianoRollEditMode(mode);
            });
    }

    AutomationResult<GuiMutationResult> EditorAutomationFacade::mutateView(
        const OperationId &operationId, const GuiCommandContext &context, ViewMutation mutation,
        ViewApply apply) {
        return m_dispatcher.dispatchGuiCommand<GuiMutationResult>(
            operationId, context, [this, context, mutation = std::move(mutation),
                                   apply = std::move(apply)](const bool validateOnly) {
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

    AutomationResult<EditorCapabilitiesDto>
    EditorAutomationFacade::getEditorCapabilities() {
        return m_dispatcher.dispatchApplicationQuery<EditorCapabilitiesDto>(
            QStringLiteral("editor.get_capabilities"), [this] {
                EditorCapabilitiesDto result;
                result.operationIds = m_catalog.operationIds();
                return AutomationResult<EditorCapabilitiesDto>(std::move(result));
            });
    }

    void EditorAutomationFacade::registerOperations() {
        m_catalog.add({
            .id = QStringLiteral("editor.get_capabilities"),
            .category = QStringLiteral("editor"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.Empty.v1"),
            .outputContract = QStringLiteral("automation.EditorCapabilities.v1"),
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
            .id = QStringLiteral("editor.get_state"),
            .category = QStringLiteral("editor"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.WindowRef.v1"),
            .outputContract = QStringLiteral("automation.EditorState.v1"),
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::GuiOnly,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        const auto addGuiCommand = [this](const QString &id, const QString &contract) {
            const auto result = m_catalog.add({
                .id = id,
                .category = QStringLiteral("editor"),
                .kind = OperationKind::Command,
                .syncMode = SyncMode::Synchronous,
                .inputContract = contract,
                .outputContract = QStringLiteral("automation.GuiMutationResult.v1"),
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
        addGuiCommand(QStringLiteral("editor.center_piano_roll"),
                      QStringLiteral("automation.EditorCenterCommand.v1"));
        addGuiCommand(QStringLiteral("editor.center_track_panel"),
                      QStringLiteral("automation.EditorCenterCommand.v1"));
        addGuiCommand(QStringLiteral("editor.restore_view"),
                      QStringLiteral("automation.EditorViewStateCommand.v1"));
        addGuiCommand(QStringLiteral("editor.set_panel_visibility"),
                      QStringLiteral("automation.EditorPanelVisibilityCommand.v1"));
        addGuiCommand(QStringLiteral("editor.set_piano_roll_edit_mode"),
                      QStringLiteral("automation.EditorEditModeCommand.v1"));
        addGuiCommand(QStringLiteral("editor.set_piano_roll_scale"),
                      QStringLiteral("automation.EditorScaleCommand.v1"));
        addGuiCommand(QStringLiteral("editor.set_track_panel_scale"),
                      QStringLiteral("automation.EditorScaleCommand.v1"));
        addGuiCommand(QStringLiteral("editor.show_bottom_panel_page"),
                      QStringLiteral("automation.EditorPageCommand.v1"));
    }

} // namespace Automation
