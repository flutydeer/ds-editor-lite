#ifndef EDITORAUTOMATIONFACADE_H
#define EDITORAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "DocumentObjectResolver.h"
#include "Interface/EditorViewState.h"

#include <functional>
#include <optional>

struct HistoryFocus;

namespace Automation {

    enum class EditorAutoPageTarget {
        TrackPanel,
        PianoRoll,
    };

    enum class EditorRevealKind {
        TrackClips,
        PianoRollNotes,
    };

    struct EditorSelectionDto {
        std::optional<TrackId> selectedTrackId;
        std::optional<ClipId> activeClipId;
        QList<ClipId> selectedClipIds;
        QList<NoteId> selectedNoteIds;

        friend bool operator==(const EditorSelectionDto &, const EditorSelectionDto &) = default;
    };

    struct EditorStableState {
        int selectedTrackIndex = -1;
        int activeClipId = -1;
        QList<int> selectedClipIds;
        QList<int> selectedNoteIds;
        int pianoRollQuantize = 16;
        bool pianoRollQuantizeEnabled = true;
        bool trackAutoPageTurnEnabled = true;
        bool pianoRollAutoPageTurnEnabled = true;

        friend bool operator==(const EditorStableState &, const EditorStableState &) = default;
    };

    struct EditorRevealDto {
        EditorRevealKind kind = EditorRevealKind::TrackClips;
        QList<int> objectIds;
        int containerId = -1;
        int trackId = -1;
        int trackIndex = -1;
        double tickStart = 0.0;
        double tickEnd = 0.0;
        double valueStart = 0.0;
        double valueEnd = 0.0;
        bool ticksAreLocal = false;
        bool allowRangeFallback = false;

        friend bool operator==(const EditorRevealDto &, const EditorRevealDto &) = default;
    };

    struct EditorStateDto {
        DocumentVersion document;
        WindowId windowId;
        QString projectPath;
        QString projectName;
        bool documentBusy = false;
        std::optional<EditorViewState> view;
        EditorSelectionDto selection;
        int pianoRollQuantize = 16;
        bool pianoRollQuantizeEnabled = true;
        bool trackAutoPageTurnEnabled = true;
        bool pianoRollAutoPageTurnEnabled = true;
    };

    struct EditorCapabilitiesDto {
        int maxConcurrentDocuments = 1;
        int maxConcurrentWindows = 1;
        QStringList operationIds;
    };

    struct EditorRuntimeServices {
        std::function<std::optional<EditorViewState>()> captureView;
        std::function<EditorStableState()> captureStableState;
        std::function<bool(const EditorViewState &)> restoreView;
        std::function<bool(double, double)> centerTrackPanel;
        std::function<bool(double, double)> setTrackPanelScale;
        std::function<bool(bool, bool)> setPanelVisibility;
        std::function<bool(const QString &)> showBottomPanelPage;
        std::function<bool(double, double)> centerPianoRoll;
        std::function<bool(double, double)> setPianoRollScale;
        std::function<bool(EditorViewGlobal::PianoRollEditMode)> setPianoRollEditMode;
        std::function<void(int)> setActiveClip;
        std::function<void(int)> setSelectedTrackIndex;
        std::function<void(const QList<int> &)> setSelectedClips;
        std::function<void(int, const QList<int> &)> setSelectedNotes;
        std::function<void(int, bool)> setPianoRollQuantize;
        std::function<void(EditorAutoPageTarget, bool)> setAutoPageTurn;
        std::function<bool(const HistoryFocus &, bool)> revealFocus;
    };

    class EditorAutomationFacade final {
    public:
        EditorAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                               DocumentObjectResolver &objectResolver,
                               EditorRuntimeServices services = {});

        AutomationResult<EditorStateDto> getEditorState(const DocumentId &documentId,
                                                        const WindowId &windowId);
        AutomationResult<EditorCapabilitiesDto> getEditorCapabilities();
        AutomationResult<GuiMutationResult> restoreView(const GuiCommandContext &context,
                                                        const EditorViewState &state);
        AutomationResult<GuiMutationResult> centerTrackPanel(const GuiCommandContext &context,
                                                             double tick, double trackIndex);
        AutomationResult<GuiMutationResult> setTrackPanelScale(const GuiCommandContext &context,
                                                               double horizontal, double vertical);
        AutomationResult<GuiMutationResult> setPanelVisibility(const GuiCommandContext &context,
                                                               bool trackVisible,
                                                               bool bottomVisible);
        AutomationResult<GuiMutationResult> showBottomPanelPage(const GuiCommandContext &context,
                                                                const QString &pageId);
        AutomationResult<GuiMutationResult> centerPianoRoll(const GuiCommandContext &context,
                                                            double tick, double keyIndex);
        AutomationResult<GuiMutationResult> setPianoRollScale(const GuiCommandContext &context,
                                                              double horizontal, double vertical);
        AutomationResult<GuiMutationResult>
            setPianoRollEditMode(const GuiCommandContext &context,
                                 EditorViewGlobal::PianoRollEditMode mode);
        AutomationResult<GuiMutationResult> setActiveClip(const GuiDocumentCommandContext &context,
                                                          std::optional<ClipId> clipId);
        AutomationResult<GuiMutationResult>
            setSelectedTrack(const GuiDocumentCommandContext &context,
                             std::optional<TrackId> trackId);
        AutomationResult<GuiMutationResult>
            setSelectedClips(const GuiDocumentCommandContext &context,
                             const QList<ClipId> &clipIds);
        AutomationResult<GuiMutationResult>
            setSelectedNotes(const GuiDocumentCommandContext &context, ClipId clipId,
                             const QList<NoteId> &noteIds);
        AutomationResult<GuiMutationResult> setPianoRollQuantize(const GuiCommandContext &context,
                                                                 int quantize, bool enabled);
        AutomationResult<GuiMutationResult> setAutoPageTurn(const GuiCommandContext &context,
                                                            EditorAutoPageTarget target,
                                                            bool enabled);
        AutomationResult<GuiMutationResult> reveal(const GuiDocumentCommandContext &context,
                                                   const EditorRevealDto &target,
                                                   bool finalize = true);

    private:
        using ViewMutation = std::function<void(EditorViewState &)>;
        using ViewApply = std::function<bool()>;
        AutomationResult<GuiMutationResult>
            mutateView(const OperationId &operationId, const GuiCommandContext &context,
                       ViewMutation mutation, ViewApply apply,
                       std::optional<AutomationError> validationError = std::nullopt);
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        DocumentObjectResolver &m_objectResolver;
        EditorRuntimeServices m_services;
    };

} // namespace Automation

#endif // EDITORAUTOMATIONFACADE_H
