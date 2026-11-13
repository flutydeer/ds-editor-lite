#ifndef EDITORVIEWCONTROLLER_H
#define EDITORVIEWCONTROLLER_H

#define editorViewController EditorViewController::instance()

#include "Global/AppGlobal.h"
#include "Interface/EditorInteraction.h"
#include "Interface/EditorViewState.h"
#include <lite/History/HistoryFocus.h>
#include <lite/Core/Singleton.h>

#include <QObject>
#include <QPointer>
#include <QWidget>
#include <optional>

class IEditorView;
class IPanel;
class AppContext;

class EditorViewController final : public QObject {
    Q_OBJECT

private:
    explicit EditorViewController(QObject *parent = nullptr);
    ~EditorViewController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(EditorViewController)
    Q_DISABLE_COPY_MOVE(EditorViewController)

    void setView(IEditorView *view);
    [[nodiscard]] std::optional<EditorViewState> captureState() const;
    bool restoreState(const EditorViewState &state) const;

    // Document switch: resets viewports and editor tool state to their defaults.
    // Panel layout, the active panel page and the focused region are kept.
    void resetDocumentScopedState();

    bool centerTrackPanelAt(double tick, double trackIndex) const;
    bool setTrackPanelScale(double horizontalScale, double verticalScale) const;
    bool setPanelVisibility(bool trackPanelVisible, bool bottomPanelVisible) const;
    bool showBottomPanelPage(const QString &pageId) const;

    bool centerPianoRollAt(double tick, double keyIndex) const;
    bool setPianoRollScale(double horizontalScale, double verticalScale) const;
    bool setPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode) const;
    bool setPianoRollQuantize(int quantize, bool enabled) const;
    bool setTrackAutoPageTurn(bool enabled) const;
    bool setPianoRollAutoPageTurn(bool enabled) const;

    void previewActiveClipTrackColor(int colorIndex) const;
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus) const;
    bool finalizeFocus(const HistoryFocus &focus) const;
    void clearFocusPreview() const;

    void registerPanel(IPanel *panel);
    void unregisterPanel(IPanel *panel);

    // Bind a panel to the top-level window it lives in. While panels are spread
    // across more than one window, the active panel is derived from window
    // activation instead of the last clicked interaction area.
    void setPanelWindow(IPanel *panel, QWidget *window);
    void clearPanelWindows();
    [[nodiscard]] bool hasSeparatedPanelWindows() const;
    void activateWindowPanel(QWidget *window);
    // Brings the window hosting the given panel to the front: restores it from
    // minimized (keeping maximized/fullscreen state) and activates it. A panel
    // docked in the already-active window is a no-op.
    void bringPanelWindowToFront(AppGlobal::PanelType panel);

    void setActivePanel(AppGlobal::PanelType panel);
    void setActiveRegion(EditorViewGlobal::Region region);
    void activatePanelContext(AppGlobal::PanelType panel);
    void syncPanelVisibility(bool trackPanelVisible, bool bottomPanelVisible,
                             AppGlobal::PanelType bottomPanelType);
    void syncEditTargetVisibility(EditorInteraction::Target target, bool visible,
                                  AppGlobal::PanelType fallbackPanel);
    [[nodiscard]] AppGlobal::PanelType activePanel() const;
    [[nodiscard]] EditorViewGlobal::Region activeRegion() const;

    void registerInteractionArea(QObject *area, AppGlobal::PanelType panel,
                                 EditorInteraction::Target target);
    void updateInteractionArea(QObject *area, AppGlobal::PanelType panel,
                               EditorInteraction::Target target);
    void unregisterInteractionArea(QObject *area);
    [[nodiscard]] EditorInteraction::Target activeEditTarget() const;
    // The clip editor page shows a read-only view (an audio clip, or no clip at
    // all), so nothing on it can be cut, copied or deleted. While it is read-only
    // the page has no editable target, which is also what a later page activation
    // restores, so the commands cannot fall through to a hidden piano roll
    void setClipEditorReadOnly(bool readOnly);
    void syncPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode);
    [[nodiscard]] bool supportsEditCommand(EditorInteraction::Command command) const;
    void requestEditCommand(EditorInteraction::Command command);

signals:
    void activePanelChanged(AppGlobal::PanelType panel);
    void activeEditTargetChanged(EditorInteraction::Target target);
    void editCommandCapabilitiesChanged();
    void editCommandRequested(EditorInteraction::Target target, EditorInteraction::Command command);

private:
    friend class AppContext;

    bool applyRestoreState(const EditorViewState &state) const;
    bool applyCenterTrackPanelAt(double tick, double trackIndex) const;
    bool applyTrackPanelScale(double horizontalScale, double verticalScale) const;
    bool applyTrackPanelViewport(const TrackPanelViewState &state) const;
    bool applyPanelVisibility(bool trackPanelVisible, bool bottomPanelVisible) const;
    bool applyBottomPanelPage(const QString &pageId) const;
    bool applyShowRegion(EditorViewGlobal::Region region) const;
    bool applyFocusRegion(EditorViewGlobal::Region region) const;
    bool applyCenterPianoRollAt(double tick, double keyIndex) const;
    bool applyPianoRollScale(double horizontalScale, double verticalScale) const;
    bool applyClipEditorTimeViewport(double centerTick, double horizontalScale) const;
    bool applyPianoRollPitchViewport(double centerKeyIndex, double verticalScale) const;
    bool applyPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode) const;
    bool applyParameterForeground(ParamInfo::Name name) const;
    bool applyParameterBackground(ParamInfo::Name name) const;
    bool applySwapParameters() const;
    bool applyParameterEditMode(EditorViewGlobal::ParameterEditMode mode) const;
    bool applyParameterValueViewport(double centerRatio, double verticalScale) const;
    bool applyRevealFocus(const HistoryFocus &focus, bool finalize) const;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setActiveContext(AppGlobal::PanelType panel, EditorInteraction::Target target);
    void applyActiveContext(AppGlobal::PanelType panel, EditorInteraction::Target target);
    void setActiveEditTarget(EditorInteraction::Target target);
    [[nodiscard]] std::optional<AppGlobal::PanelType> panelTypeForWindow(
        const QWidget *window) const;

    struct InteractionArea {
        QPointer<QObject> object;
        AppGlobal::PanelType panel = AppGlobal::Generic;
        EditorInteraction::Target target = EditorInteraction::Target::None;
    };

    struct PanelWindowBinding {
        QPointer<QWidget> window;
        IPanel *panel = nullptr;
    };

    IEditorView *m_view = nullptr;
    QList<IPanel *> m_panels;
    QList<InteractionArea> m_interactionAreas;
    QList<PanelWindowBinding> m_panelWindows;
    AppGlobal::PanelType m_activePanel = AppGlobal::TracksEditor;
    EditorInteraction::Target m_activeEditTarget = EditorInteraction::Target::Tracks;
    EditorInteraction::Target m_lastClipEditTarget = EditorInteraction::Target::PianoRoll;
    bool m_clipEditorReadOnly = false;
    EditorViewGlobal::PianoRollEditMode m_pianoRollEditMode = EditorViewGlobal::Select;
};

#endif // EDITORVIEWCONTROLLER_H
