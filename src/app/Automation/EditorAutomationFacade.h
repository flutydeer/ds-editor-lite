#ifndef EDITORAUTOMATIONFACADE_H
#define EDITORAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "Interface/EditorViewState.h"

#include <functional>
#include <optional>

namespace Automation {

    struct EditorStateDto {
        DocumentVersion document;
        WindowId windowId;
        QString projectPath;
        QString projectName;
        bool documentBusy = false;
        std::optional<EditorViewState> view;
    };

    struct EditorCapabilitiesDto {
        int maxConcurrentDocuments = 1;
        int maxConcurrentWindows = 1;
        QStringList operationIds;
    };

    struct EditorRuntimeServices {
        std::function<std::optional<EditorViewState>()> captureView;
        std::function<bool(const EditorViewState &)> restoreView;
        std::function<bool(double, double)> centerTrackPanel;
        std::function<bool(double, double)> setTrackPanelScale;
        std::function<bool(bool, bool)> setPanelVisibility;
        std::function<bool(const QString &)> showBottomPanelPage;
        std::function<bool(double, double)> centerPianoRoll;
        std::function<bool(double, double)> setPianoRollScale;
        std::function<bool(EditorViewGlobal::PianoRollEditMode)> setPianoRollEditMode;
    };

    class EditorAutomationFacade final {
    public:
        EditorAutomationFacade(DocumentSession &session,
                               SingleWindowContext &windowContext,
                               OperationCatalog &catalog,
                               AutomationDispatcher &dispatcher,
                               EditorRuntimeServices services = {});

        AutomationResult<EditorStateDto> getEditorState(const WindowId &windowId);
        AutomationResult<EditorCapabilitiesDto> getEditorCapabilities();
        AutomationResult<GuiMutationResult> restoreView(const GuiCommandContext &context,
                                                        const EditorViewState &state);
        AutomationResult<GuiMutationResult> centerTrackPanel(const GuiCommandContext &context,
                                                            double tick,
                                                            double trackIndex);
        AutomationResult<GuiMutationResult> setTrackPanelScale(const GuiCommandContext &context,
                                                              double horizontal,
                                                              double vertical);
        AutomationResult<GuiMutationResult> setPanelVisibility(const GuiCommandContext &context,
                                                              bool trackVisible,
                                                              bool bottomVisible);
        AutomationResult<GuiMutationResult> showBottomPanelPage(
            const GuiCommandContext &context, const QString &pageId);
        AutomationResult<GuiMutationResult> centerPianoRoll(const GuiCommandContext &context,
                                                           double tick,
                                                           double keyIndex);
        AutomationResult<GuiMutationResult> setPianoRollScale(const GuiCommandContext &context,
                                                             double horizontal,
                                                             double vertical);
        AutomationResult<GuiMutationResult> setPianoRollEditMode(
            const GuiCommandContext &context, EditorViewGlobal::PianoRollEditMode mode);

    private:
        using ViewMutation = std::function<void(EditorViewState &)>;
        using ViewApply = std::function<bool()>;
        AutomationResult<GuiMutationResult> mutateView(const OperationId &operationId,
                                                       const GuiCommandContext &context,
                                                       ViewMutation mutation,
                                                       ViewApply apply);
        void registerOperations();

        DocumentSession &m_session;
        SingleWindowContext &m_windowContext;
        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        EditorRuntimeServices m_services;
    };

} // namespace Automation

#endif // EDITORAUTOMATIONFACADE_H
