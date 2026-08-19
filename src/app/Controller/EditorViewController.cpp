#include "EditorViewController.h"

#include "Interface/IEditorView.h"
#include "Interface/IPanel.h"

#include <QCoreApplication>
#include <QEvent>

#include <algorithm>
#include <utility>

EditorViewController::EditorViewController(QObject *parent) : QObject(parent) {
    if (qApp)
        qApp->installEventFilter(this);
}

EditorViewController::~EditorViewController() {
    if (qApp)
        qApp->removeEventFilter(this);
}

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
    setActiveContext(panel, EditorInteraction::defaultTargetForPanel(panel));
}

void EditorViewController::activatePanelContext(const AppGlobal::PanelType panel) {
    const auto target = panel == AppGlobal::ClipEditor
                            ? m_lastClipEditTarget
                            : EditorInteraction::defaultTargetForPanel(panel);
    setActiveContext(panel, target);
}

void EditorViewController::syncPanelVisibility(const bool trackPanelVisible,
                                               const bool bottomPanelVisible,
                                               const AppGlobal::PanelType bottomPanelType) {
    if (!trackPanelVisible && !bottomPanelVisible)
        return;
    if (!bottomPanelVisible) {
        setActivePanel(AppGlobal::TracksEditor);
        return;
    }
    if (!trackPanelVisible && m_activePanel != bottomPanelType)
        activatePanelContext(bottomPanelType);
}

void EditorViewController::syncEditTargetVisibility(const EditorInteraction::Target target,
                                                    const bool visible,
                                                    const AppGlobal::PanelType fallbackPanel) {
    if (!visible && m_activeEditTarget == target)
        setActivePanel(fallbackPanel);
}

AppGlobal::PanelType EditorViewController::activePanel() const {
    return m_activePanel;
}

void EditorViewController::registerInteractionArea(QObject *area, const AppGlobal::PanelType panel,
                                                   const EditorInteraction::Target target) {
    if (!area)
        return;
    updateInteractionArea(area, panel, target);
}

void EditorViewController::updateInteractionArea(QObject *area, const AppGlobal::PanelType panel,
                                                 const EditorInteraction::Target target) {
    if (!area)
        return;
    m_interactionAreas.erase(
        std::remove_if(m_interactionAreas.begin(), m_interactionAreas.end(),
                       [](const InteractionArea &entry) { return entry.object.isNull(); }),
        m_interactionAreas.end());
    for (auto &entry : m_interactionAreas) {
        if (entry.object == area) {
            entry.panel = panel;
            entry.target = target;
            return;
        }
    }
    m_interactionAreas.append({area, panel, target});
}

void EditorViewController::unregisterInteractionArea(QObject *area) {
    m_interactionAreas.erase(std::remove_if(m_interactionAreas.begin(), m_interactionAreas.end(),
                                            [area](const InteractionArea &entry) {
                                                return entry.object.isNull() ||
                                                       entry.object == area;
                                            }),
                             m_interactionAreas.end());
}

EditorInteraction::Target EditorViewController::activeEditTarget() const {
    return m_activeEditTarget;
}

void EditorViewController::requestEditCommand(const EditorInteraction::Command command) {
    emit editCommandRequested(m_activeEditTarget, command);
}

bool EditorViewController::eventFilter(QObject *watched, QEvent *event) {
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::FocusIn)
        return QObject::eventFilter(watched, event);

    for (auto *object = watched; object; object = object->parent()) {
        for (const auto &entry : std::as_const(m_interactionAreas)) {
            if (entry.object == object) {
                setActiveContext(entry.panel, entry.target);
                return QObject::eventFilter(watched, event);
            }
        }
    }
    return QObject::eventFilter(watched, event);
}

void EditorViewController::setActiveContext(const AppGlobal::PanelType panel,
                                            const EditorInteraction::Target target) {
    if (panel == AppGlobal::ClipEditor &&
        (target == EditorInteraction::Target::PianoRoll ||
         target == EditorInteraction::Target::Parameters)) {
        m_lastClipEditTarget = target;
    }
    if (m_activePanel != panel) {
        m_activePanel = panel;
        for (const auto registeredPanel : std::as_const(m_panels))
            registeredPanel->setPanelActive(registeredPanel->panelType() == panel);
        emit activePanelChanged(panel);
    }
    setActiveEditTarget(target);
}

void EditorViewController::setActiveEditTarget(const EditorInteraction::Target target) {
    if (m_activeEditTarget == target)
        return;
    m_activeEditTarget = target;
    emit activeEditTargetChanged(target);
}
