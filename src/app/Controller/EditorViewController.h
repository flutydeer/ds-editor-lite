#ifndef EDITORVIEWCONTROLLER_H
#define EDITORVIEWCONTROLLER_H

#define editorViewController EditorViewController::instance()

#include "Global/AppGlobal.h"
#include "Interface/EditorViewState.h"
#include "Modules/History/HistoryFocus.h"
#include "Utils/Singleton.h"

#include <QObject>
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

signals:
    void activePanelChanged(AppGlobal::PanelType panel);

private:
    IEditorView *m_view = nullptr;
    QList<IPanel *> m_panels;
    AppGlobal::PanelType m_activePanel = AppGlobal::TracksEditor;
};

#endif // EDITORVIEWCONTROLLER_H
