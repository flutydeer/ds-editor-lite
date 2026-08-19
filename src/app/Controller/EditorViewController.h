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
#include <optional>

class IEditorView;
class IPanel;

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

    bool centerTrackPanelAt(double tick, double trackIndex) const;
    bool setTrackPanelScale(double horizontalScale, double verticalScale) const;
    bool setPanelVisibility(bool trackPanelVisible, bool bottomPanelVisible) const;
    bool showBottomPanelPage(const QString &pageId) const;

    bool centerPianoRollAt(double tick, double keyIndex) const;
    bool setPianoRollScale(double horizontalScale, double verticalScale) const;
    bool setPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode) const;

    void refreshActiveClipTrackPresentation() const;
    void previewActiveClipTrackColor(int colorIndex) const;
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const;
    bool revealFocus(const HistoryFocus &focus) const;
    bool finalizeFocus(const HistoryFocus &focus) const;
    void clearFocusPreview() const;

    void registerPanel(IPanel *panel);
    void unregisterPanel(IPanel *panel);
    void setActivePanel(AppGlobal::PanelType panel);
    void syncPanelVisibility(bool trackPanelVisible, bool bottomPanelVisible,
                             AppGlobal::PanelType bottomPanelType);
    void syncEditTargetVisibility(EditorInteraction::Target target, bool visible,
                                  AppGlobal::PanelType fallbackPanel);
    [[nodiscard]] AppGlobal::PanelType activePanel() const;

    void registerInteractionArea(QObject *area, AppGlobal::PanelType panel,
                                 EditorInteraction::Target target);
    void updateInteractionArea(QObject *area, AppGlobal::PanelType panel,
                               EditorInteraction::Target target);
    void unregisterInteractionArea(QObject *area);
    [[nodiscard]] EditorInteraction::Target activeEditTarget() const;
    void requestEditCommand(EditorInteraction::Command command);

signals:
    void activePanelChanged(AppGlobal::PanelType panel);
    void activeEditTargetChanged(EditorInteraction::Target target);
    void editCommandRequested(EditorInteraction::Target target, EditorInteraction::Command command);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void setActiveContext(AppGlobal::PanelType panel, EditorInteraction::Target target);
    void setActiveEditTarget(EditorInteraction::Target target);

    struct InteractionArea {
        QPointer<QObject> object;
        AppGlobal::PanelType panel = AppGlobal::Generic;
        EditorInteraction::Target target = EditorInteraction::Target::None;
    };

    IEditorView *m_view = nullptr;
    QList<IPanel *> m_panels;
    QList<InteractionArea> m_interactionAreas;
    AppGlobal::PanelType m_activePanel = AppGlobal::TracksEditor;
    EditorInteraction::Target m_activeEditTarget = EditorInteraction::Target::Tracks;
};

#endif // EDITORVIEWCONTROLLER_H
