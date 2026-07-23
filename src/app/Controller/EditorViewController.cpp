#include "EditorViewController.h"

#include "Interface/IEditorView.h"
#include "Interface/IPanel.h"

#include <utility>

EditorViewController::EditorViewController(QObject *parent) : QObject(parent) {
}

EditorViewController::~EditorViewController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(EditorViewController)

void EditorViewController::setView(IEditorView *view) {
    m_view = view;
}

std::optional<EditorViewState> EditorViewController::captureState() const {
    if (!m_view)
        return std::nullopt;
    return m_view->captureEditorViewState();
}

bool EditorViewController::restoreState(const EditorViewState &state) const {
    return m_view && m_view->restoreEditorViewState(state);
}

bool EditorViewController::centerTrackPanelAt(double tick, double trackIndex) const {
    return m_view && m_view->centerTrackPanelAt(tick, trackIndex);
}

bool EditorViewController::setTrackPanelScale(double horizontalScale, double verticalScale) const {
    return m_view && m_view->setTrackPanelScale(horizontalScale, verticalScale);
}

bool EditorViewController::setPanelVisibility(bool trackPanelVisible,
                                              bool bottomPanelVisible) const {
    if (!trackPanelVisible && !bottomPanelVisible)
        return false;
    return m_view && m_view->setEditorPanelVisibility(trackPanelVisible, bottomPanelVisible);
}

bool EditorViewController::showBottomPanelPage(const QString &pageId) const {
    return m_view && m_view->showBottomPanelPage(pageId);
}

bool EditorViewController::centerPianoRollAt(double tick, double keyIndex) const {
    return m_view && m_view->centerPianoRollAt(tick, keyIndex);
}

bool EditorViewController::setPianoRollScale(double horizontalScale, double verticalScale) const {
    return m_view && m_view->setPianoRollScale(horizontalScale, verticalScale);
}

bool EditorViewController::setPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode) const {
    return m_view && m_view->setPianoRollEditMode(mode);
}

void EditorViewController::refreshActiveClipTrackPresentation() const {
    if (m_view)
        m_view->refreshActiveClipTrackPresentation();
}

void EditorViewController::previewActiveClipTrackColor(int colorIndex) const {
    if (m_view)
        m_view->previewActiveClipTrackColor(colorIndex);
}

HistoryFocusVisibility EditorViewController::focusVisibility(const HistoryFocus &focus) const {
    return m_view ? m_view->focusVisibility(focus) : HistoryFocusVisibility::Unavailable;
}

bool EditorViewController::revealFocus(const HistoryFocus &focus) const {
    return m_view && m_view->revealFocus(focus);
}

bool EditorViewController::finalizeFocus(const HistoryFocus &focus) const {
    return m_view && m_view->finalizeFocus(focus);
}

void EditorViewController::clearFocusPreview() const {
    if (m_view)
        m_view->clearFocusPreview();
}

void EditorViewController::registerPanel(IPanel *panel) {
    if (!panel || m_panels.contains(panel))
        return;
    m_panels.append(panel);
    panel->setPanelActive(panel->panelType() == m_activePanel);
}

void EditorViewController::unregisterPanel(IPanel *panel) {
    m_panels.removeAll(panel);
}

void EditorViewController::setActivePanel(AppGlobal::PanelType panel) {
    for (const auto registeredPanel : std::as_const(m_panels))
        registeredPanel->setPanelActive(registeredPanel->panelType() == panel);
    m_activePanel = panel;
    emit activePanelChanged(panel);
}
