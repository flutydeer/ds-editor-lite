#include "EditorViewController.h"

#include "AppContext.h"
#include "Automation/CoreRuntime.h"
#include "Interface/IEditorView.h"
#include "Interface/IPanel.h"

#include <QCoreApplication>
#include <QEvent>

#include <algorithm>
#include <utility>

namespace {
    std::optional<Automation::EditorRevealDto> revealDto(const HistoryFocus &focus) {
        Automation::EditorRevealKind kind;
        if (focus.kind == HistoryFocusKind::TrackClips)
            kind = Automation::EditorRevealKind::TrackClips;
        else if (focus.kind == HistoryFocusKind::PianoRollNotes)
            kind = Automation::EditorRevealKind::PianoRollNotes;
        else
            return std::nullopt;
        return Automation::EditorRevealDto{
            .kind = kind,
            .objectIds = focus.objectIds,
            .containerId = focus.containerId,
            .trackId = focus.trackId,
            .trackIndex = focus.trackIndex,
            .tickStart = focus.tickStart,
            .tickEnd = focus.tickEnd,
            .valueStart = focus.valueStart,
            .valueEnd = focus.valueEnd,
            .ticksAreLocal = focus.ticksAreLocal,
            .allowRangeFallback = true,
        };
    }
}

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
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime &&
           runtime->facade().restoreView({.windowId = runtime->windowId(),
                                          .source = Automation::InvocationSource::TrustedGui},
                                         state);
}

bool EditorViewController::centerTrackPanelAt(double tick, double trackIndex) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime &&
           runtime->facade().centerTrackPanel({.windowId = runtime->windowId(),
                                               .source = Automation::InvocationSource::TrustedGui},
                                              tick, trackIndex);
}

bool EditorViewController::setTrackPanelScale(double horizontalScale, double verticalScale) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime && runtime->facade().setTrackPanelScale(
                          {.windowId = runtime->windowId(),
                           .source = Automation::InvocationSource::TrustedGui},
                          horizontalScale, verticalScale);
}

bool EditorViewController::setPanelVisibility(bool trackPanelVisible,
                                              bool bottomPanelVisible) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime && runtime->facade().setPanelVisibility(
                          {.windowId = runtime->windowId(),
                           .source = Automation::InvocationSource::TrustedGui},
                          trackPanelVisible, bottomPanelVisible);
}

bool EditorViewController::showBottomPanelPage(const QString &pageId) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime && runtime->facade().showBottomPanelPage(
                          {.windowId = runtime->windowId(),
                           .source = Automation::InvocationSource::TrustedGui},
                          pageId);
}

bool EditorViewController::centerPianoRollAt(double tick, double keyIndex) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime &&
           runtime->facade().centerPianoRoll({.windowId = runtime->windowId(),
                                              .source = Automation::InvocationSource::TrustedGui},
                                             tick, keyIndex);
}

bool EditorViewController::setPianoRollScale(double horizontalScale, double verticalScale) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime &&
           runtime->facade().setPianoRollScale({.windowId = runtime->windowId(),
                                                .source = Automation::InvocationSource::TrustedGui},
                                               horizontalScale, verticalScale);
}

bool EditorViewController::setPianoRollEditMode(EditorViewGlobal::PianoRollEditMode mode) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime && runtime->facade().setPianoRollEditMode(
                          {.windowId = runtime->windowId(),
                           .source = Automation::InvocationSource::TrustedGui},
                          mode);
}

bool EditorViewController::setPianoRollQuantize(const int quantize, const bool enabled) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime && runtime->facade().setPianoRollQuantize(
                          {.windowId = runtime->windowId(),
                           .source = Automation::InvocationSource::TrustedGui},
                          quantize, enabled);
}

bool EditorViewController::setTrackAutoPageTurn(const bool enabled) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime &&
           runtime->facade().setAutoPageTurn({.windowId = runtime->windowId(),
                                              .source = Automation::InvocationSource::TrustedGui},
                                             Automation::EditorAutoPageTarget::TrackPanel, enabled);
}

bool EditorViewController::setPianoRollAutoPageTurn(const bool enabled) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    return runtime &&
           runtime->facade().setAutoPageTurn({.windowId = runtime->windowId(),
                                              .source = Automation::InvocationSource::TrustedGui},
                                             Automation::EditorAutoPageTarget::PianoRoll, enabled);
}

bool EditorViewController::applyRestoreState(const EditorViewState &state) const {
    return m_view && m_view->restoreEditorViewState(state);
}

bool EditorViewController::applyCenterTrackPanelAt(const double tick,
                                                   const double trackIndex) const {
    return m_view && m_view->centerTrackPanelAt(tick, trackIndex);
}

bool EditorViewController::applyTrackPanelScale(const double horizontalScale,
                                                const double verticalScale) const {
    return m_view && m_view->setTrackPanelScale(horizontalScale, verticalScale);
}

bool EditorViewController::applyPanelVisibility(const bool trackPanelVisible,
                                                const bool bottomPanelVisible) const {
    return m_view && m_view->setEditorPanelVisibility(trackPanelVisible, bottomPanelVisible);
}

bool EditorViewController::applyBottomPanelPage(const QString &pageId) const {
    return m_view && m_view->showBottomPanelPage(pageId);
}

bool EditorViewController::applyCenterPianoRollAt(const double tick, const double keyIndex) const {
    return m_view && m_view->centerPianoRollAt(tick, keyIndex);
}

bool EditorViewController::applyPianoRollScale(const double horizontalScale,
                                               const double verticalScale) const {
    return m_view && m_view->setPianoRollScale(horizontalScale, verticalScale);
}

bool EditorViewController::applyPianoRollEditMode(
    const EditorViewGlobal::PianoRollEditMode mode) const {
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
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    const auto target = revealDto(focus);
    return runtime && target &&
           runtime->facade().reveal({.expected = runtime->documentVersion(),
                                     .windowId = runtime->windowId(),
                                     .source = Automation::InvocationSource::TrustedGui},
                                    *target, false);
}

bool EditorViewController::finalizeFocus(const HistoryFocus &focus) const {
    auto *runtime = AppContext::instance<Automation::CoreRuntime>();
    const auto target = revealDto(focus);
    return runtime && target &&
           runtime->facade().reveal({.expected = runtime->documentVersion(),
                                     .windowId = runtime->windowId(),
                                     .source = Automation::InvocationSource::TrustedGui},
                                    *target, true);
}

bool EditorViewController::applyRevealFocus(const HistoryFocus &focus, const bool finalize) const {
    return m_view && (finalize ? m_view->finalizeFocus(focus) : m_view->revealFocus(focus));
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

void EditorViewController::syncPianoRollEditMode(
    const EditorViewGlobal::PianoRollEditMode mode) {
    if (m_pianoRollEditMode == mode)
        return;
    m_pianoRollEditMode = mode;
    emit editCommandCapabilitiesChanged();
}

bool EditorViewController::supportsEditCommand(const EditorInteraction::Command command) const {
    if (m_activeEditTarget == EditorInteraction::Target::PianoRoll)
        return EditorInteraction::supportsCommand(m_activeEditTarget, command, m_pianoRollEditMode);
    return EditorInteraction::supportsCommand(m_activeEditTarget, command);
}

void EditorViewController::requestEditCommand(const EditorInteraction::Command command) {
    if (!supportsEditCommand(command))
        return;
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
    if (panel == AppGlobal::ClipEditor && (target == EditorInteraction::Target::PianoRoll ||
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
