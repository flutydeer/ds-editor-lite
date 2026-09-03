#include "Controller/EditorViewController.h"
#include "Interface/IEditorView.h"
#include "AppContext.h"
#include "Interface/IPanel.h"
#include "TestRuntime.h"

#include <QCoreApplication>
#include <QEvent>
#include <QTextStream>

#include <cmath>
#include <utility>

namespace {
    Automation::CoreRuntime *g_runtime = nullptr;
    IEditorView *g_editorHost = nullptr;
}

template <>
EditorViewController *AppContext::instance<EditorViewController>() {
    return nullptr;
}

template <>
Automation::CoreRuntime *AppContext::instance<Automation::CoreRuntime>() {
    return g_runtime;
}

namespace {

    int g_failures = 0;

    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++g_failures;
        return false;
    }

    bool validState(const EditorViewState &state) {
        const auto finite = [](const double value) { return std::isfinite(value); };
        return (state.layout.trackPanelVisible || state.layout.bottomPanelVisible) &&
               (state.layout.pianoRollVisible || state.layout.parametersVisible) &&
               (state.layout.bottomPanelPageId == QStringLiteral("ClipEditor") ||
                state.layout.bottomPanelPageId == QStringLiteral("MixConsole")) &&
               state.pianoRoll.editMode >= EditorViewGlobal::Select &&
               state.pianoRoll.editMode <= EditorViewGlobal::ModulatePitch &&
               finite(state.trackPanel.centerTick) && finite(state.trackPanel.centerTrackIndex) &&
               finite(state.trackPanel.horizontalScale) && finite(state.trackPanel.verticalScale) &&
               state.trackPanel.horizontalScale > 0 && state.trackPanel.verticalScale > 0 &&
               finite(state.pianoRoll.centerTick) && finite(state.pianoRoll.centerKeyIndex) &&
               finite(state.pianoRoll.horizontalScale) && finite(state.pianoRoll.verticalScale) &&
               state.pianoRoll.horizontalScale > 0 && state.pianoRoll.verticalScale > 0;
    }

    class FakeEditorView final : public IEditorView {
    public:
        EditorViewState state;
        mutable int captureCount = 0;
        int restoreCount = 0;
        int visibilityCallCount = 0;
        int refreshCount = 0;
        int previewCount = 0;
        int previewColorIndex = -1;
        HistoryFocusVisibility nextFocusVisibility = HistoryFocusVisibility::Visible;
        int focusVisibilityCount = 0;
        int revealFocusCount = 0;
        int finalizeFocusCount = 0;
        int clearFocusPreviewCount = 0;

        [[nodiscard]] EditorViewState captureEditorViewState() const override {
            ++captureCount;
            return state;
        }

        bool restoreEditorViewState(const EditorViewState &newState) override {
            ++restoreCount;
            if (!validState(newState))
                return false;
            state = newState;
            return true;
        }

        bool centerTrackPanelAt(const double tick, const double trackIndex) override {
            if (!std::isfinite(tick) || !std::isfinite(trackIndex))
                return false;
            state.trackPanel.centerTick = tick;
            state.trackPanel.centerTrackIndex = trackIndex;
            return true;
        }

        bool setTrackPanelScale(const double horizontalScale, const double verticalScale) override {
            if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale) ||
                horizontalScale <= 0 || verticalScale <= 0) {
                return false;
            }
            state.trackPanel.horizontalScale = horizontalScale;
            state.trackPanel.verticalScale = verticalScale;
            return true;
        }

        bool setTrackPanelViewport(const TrackPanelViewState &value) override {
            state.trackPanel = value;
            return true;
        }

        bool setEditorPanelVisibility(const bool trackPanelVisible,
                                      const bool bottomPanelVisible) override {
            ++visibilityCallCount;
            if (!trackPanelVisible && !bottomPanelVisible)
                return false;
            state.layout.trackPanelVisible = trackPanelVisible;
            state.layout.bottomPanelVisible = bottomPanelVisible;
            return true;
        }

        bool showBottomPanelPage(const QString &pageId) override {
            if (pageId != QStringLiteral("ClipEditor") && pageId != QStringLiteral("MixConsole")) {
                return false;
            }
            state.layout.bottomPanelVisible = true;
            state.layout.bottomPanelPageId = pageId;
            return true;
        }

        bool showEditorRegion(const EditorViewGlobal::Region region) override {
            if (region != EditorViewGlobal::Region::PianoRoll &&
                region != EditorViewGlobal::Region::Parameters) {
                return false;
            }
            state.layout.bottomPanelVisible = true;
            state.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
            if (region == EditorViewGlobal::Region::PianoRoll)
                state.layout.pianoRollVisible = true;
            else
                state.layout.parametersVisible = true;
            state.layout.activeRegion = region;
            state.layout.focusedRegion = region;
            return true;
        }

        bool focusEditorRegion(const EditorViewGlobal::Region region) override {
            if (region == EditorViewGlobal::Region::TrackPanel)
                state.layout.trackPanelVisible = true;
            else if (region == EditorViewGlobal::Region::PianoRoll ||
                     region == EditorViewGlobal::Region::Parameters) {
                state.layout.bottomPanelVisible = true;
                state.layout.bottomPanelPageId = QStringLiteral("ClipEditor");
                if (region == EditorViewGlobal::Region::PianoRoll)
                    state.layout.pianoRollVisible = true;
                else
                    state.layout.parametersVisible = true;
            } else {
                return false;
            }
            state.layout.activeRegion = region;
            state.layout.focusedRegion = region;
            return true;
        }

        bool centerPianoRollAt(const double tick, const double keyIndex) override {
            if (!std::isfinite(tick) || !std::isfinite(keyIndex))
                return false;
            state.pianoRoll.centerTick = tick;
            state.pianoRoll.centerKeyIndex = keyIndex;
            return true;
        }

        bool setPianoRollScale(const double horizontalScale, const double verticalScale) override {
            if (!std::isfinite(horizontalScale) || !std::isfinite(verticalScale) ||
                horizontalScale <= 0 || verticalScale <= 0) {
                return false;
            }
            state.pianoRoll.horizontalScale = horizontalScale;
            state.pianoRoll.verticalScale = verticalScale;
            return true;
        }

        bool setClipEditorTimeViewport(const double centerTick,
                                       const double horizontalScale) override {
            state.pianoRoll.centerTick = centerTick;
            state.pianoRoll.horizontalScale = horizontalScale;
            return true;
        }

        bool setPianoRollPitchViewport(const double centerKeyIndex,
                                       const double verticalScale) override {
            state.pianoRoll.centerKeyIndex = centerKeyIndex;
            state.pianoRoll.verticalScale = verticalScale;
            return true;
        }

        bool setPianoRollEditMode(const EditorViewGlobal::PianoRollEditMode mode) override {
            if (mode < EditorViewGlobal::Select || mode > EditorViewGlobal::ModulatePitch)
                return false;
            state.pianoRoll.editMode = mode;
            return true;
        }

        bool setParameterForeground(const ParamInfo::Name name) override {
            state.parameters.foreground = name;
            return true;
        }

        bool setParameterBackground(const ParamInfo::Name name) override {
            state.parameters.background = name;
            return true;
        }

        bool swapParameters() override {
            std::swap(state.parameters.foreground, state.parameters.background);
            return true;
        }

        bool setParameterEditMode(const EditorViewGlobal::ParameterEditMode mode) override {
            state.parameters.editMode = mode;
            return true;
        }

        bool setParameterValueViewport(const double centerRatio,
                                       const double verticalScale) override {
            state.parameters.centerRatio = centerRatio;
            state.parameters.verticalScale = verticalScale;
            return true;
        }

        void refreshActiveClipTrackPresentation() override {
            ++refreshCount;
        }

        void previewActiveClipTrackColor(const int colorIndex) override {
            ++previewCount;
            previewColorIndex = colorIndex;
        }

        HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const override {
            Q_UNUSED(focus);
            ++const_cast<FakeEditorView *>(this)->focusVisibilityCount;
            return nextFocusVisibility;
        }

        bool revealFocus(const HistoryFocus &focus) override {
            Q_UNUSED(focus);
            ++revealFocusCount;
            return true;
        }

        bool finalizeFocus(const HistoryFocus &focus) override {
            Q_UNUSED(focus);
            ++finalizeFocusCount;
            return true;
        }

        void clearFocusPreview() override {
            ++clearFocusPreviewCount;
        }
    };

    class FakePanel final : public IPanel {
    public:
        explicit FakePanel(const AppGlobal::PanelType type) : IPanel(type) {
        }

        int updateCount = 0;

    private:
        void afterSetActive() override {
            ++updateCount;
        }
    };

    void bindEditorView(EditorViewController *controller, IEditorView *view) {
        controller->setView(view);
        g_editorHost = view;
    }

    EditorViewState sampleState() {
        return {
            .trackPanel =
                {
                             .centerTick = 1920,
                             .centerTrackIndex = 2.5,
                             .horizontalScale = 1.75,
                             .verticalScale = 1.25,
                             },
            .layout =
                {
                             .trackPanelVisible = true,
                             .bottomPanelVisible = true,
                             .bottomPanelPageId = QStringLiteral("ClipEditor"),
                             },
            .pianoRoll =
                {
                             .centerTick = 2400,
                             .centerKeyIndex = 64.5,
                             .horizontalScale = 2.0,
                             .verticalScale = 1.5,
                             .editMode = EditorViewGlobal::DrawNote,
                             },
        };
    }

    void testNoView(EditorViewController *controller) {
        bindEditorView(controller, nullptr);
        expect(!controller->captureState().has_value(),
               "capture without a bound view must return no state");
        expect(!controller->restoreState(sampleState()), "restore without a bound view must fail");
        expect(!controller->centerTrackPanelAt(1, 2),
               "track centering without a bound view must fail");
        expect(!controller->setTrackPanelScale(1, 1),
               "track scaling without a bound view must fail");
        expect(!controller->setPanelVisibility(true, false),
               "panel visibility without a bound view must fail");
        expect(!controller->showBottomPanelPage(QStringLiteral("ClipEditor")),
               "page switching without a bound view must fail");
        expect(!controller->centerPianoRollAt(1, 60),
               "piano-roll centering without a bound view must fail");
        expect(!controller->setPianoRollScale(1, 1),
               "piano-roll scaling without a bound view must fail");
        expect(!controller->setPianoRollEditMode(EditorViewGlobal::Select),
               "tool switching without a bound view must fail");
        controller->refreshActiveClipTrackPresentation();
        controller->previewActiveClipTrackColor(3);
        HistoryFocus focus;
        expect(controller->focusVisibility(focus) == HistoryFocusVisibility::Unavailable,
               "focus visibility without a bound view must be unavailable");
        expect(!controller->revealFocus(focus), "focus reveal without a bound view must fail");
        expect(!controller->finalizeFocus(focus), "focus finalize without a bound view must fail");
        controller->clearFocusPreview();
    }

    void testCommandCapabilities() {
        using EditorInteraction::Command;
        using EditorInteraction::Target;
        expect(EditorInteraction::supportsCommand(Target::Tracks, Command::Cut) &&
                   EditorInteraction::supportsCommand(Target::PianoRoll, Command::SelectAll),
               "track and piano-roll editors must expose their complete edit command set");
        expect(EditorInteraction::supportsCommand(Target::Parameters, Command::DeleteSelection) &&
                   !EditorInteraction::supportsCommand(Target::Parameters, Command::Cut) &&
                   !EditorInteraction::supportsCommand(Target::Parameters, Command::Copy) &&
                   !EditorInteraction::supportsCommand(Target::Parameters, Command::Paste) &&
                   !EditorInteraction::supportsCommand(Target::Parameters, Command::SelectAll),
               "the parameter editor must expose only its implemented delete command");
        expect(!EditorInteraction::supportsCommand(Target::None, Command::DeleteSelection),
               "a non-editor target must not expose edit commands");

        const auto supportsNoCommands = [](const EditorViewGlobal::PianoRollEditMode mode) {
            return !EditorInteraction::supportsCommand(Target::PianoRoll, Command::Cut, mode) &&
                   !EditorInteraction::supportsCommand(Target::PianoRoll, Command::Copy, mode) &&
                   !EditorInteraction::supportsCommand(Target::PianoRoll, Command::Paste, mode) &&
                   !EditorInteraction::supportsCommand(Target::PianoRoll, Command::SelectAll,
                                                       mode) &&
                   !EditorInteraction::supportsCommand(Target::PianoRoll,
                                                       Command::DeleteSelection, mode);
        };
        expect(supportsNoCommands(EditorViewGlobal::DrawPitch) &&
                   supportsNoCommands(EditorViewGlobal::ErasePitch) &&
                   supportsNoCommands(EditorViewGlobal::BakePitch) &&
                   supportsNoCommands(EditorViewGlobal::ModulatePitch),
               "pitch drawing, erasing, baking, and modulation must reject note edit commands");
        expect(!EditorInteraction::supportsCommand(Target::PianoRoll, Command::Cut,
                                                   EditorViewGlobal::EditPitchAnchor) &&
                   !EditorInteraction::supportsCommand(Target::PianoRoll, Command::Copy,
                                                       EditorViewGlobal::EditPitchAnchor) &&
                   !EditorInteraction::supportsCommand(Target::PianoRoll, Command::Paste,
                                                       EditorViewGlobal::EditPitchAnchor) &&
                   !EditorInteraction::supportsCommand(Target::PianoRoll, Command::SelectAll,
                                                       EditorViewGlobal::EditPitchAnchor) &&
                   EditorInteraction::supportsCommand(Target::PianoRoll,
                                                      Command::DeleteSelection,
                                                      EditorViewGlobal::EditPitchAnchor),
               "pitch anchor editing must expose only anchor deletion");
        expect(EditorInteraction::supportsCommand(Target::PianoRoll, Command::SelectAll,
                                                  EditorViewGlobal::Select) &&
                   EditorInteraction::supportsCommand(Target::PianoRoll, Command::Paste,
                                                      EditorViewGlobal::DrawNote),
               "note edit modes must retain piano-roll edit commands");
    }

    void testModeAwareCommandRouting(EditorViewController *controller) {
        controller->setActivePanel(AppGlobal::ClipEditor);
        controller->syncPianoRollEditMode(EditorViewGlobal::Select);

        int capabilityChangeCount = 0;
        int commandCount = 0;
        EditorInteraction::Command requestedCommand = EditorInteraction::Command::Cut;
        const auto capabilityConnection =
            QObject::connect(controller, &EditorViewController::editCommandCapabilitiesChanged,
                             [&capabilityChangeCount] { ++capabilityChangeCount; });
        const auto commandConnection = QObject::connect(
            controller, &EditorViewController::editCommandRequested,
            [&commandCount, &requestedCommand](EditorInteraction::Target,
                                                const EditorInteraction::Command command) {
                ++commandCount;
                requestedCommand = command;
            });

        controller->requestEditCommand(EditorInteraction::Command::SelectAll);
        expect(commandCount == 1 && requestedCommand == EditorInteraction::Command::SelectAll,
               "note modes must dispatch supported piano-roll commands");

        controller->syncPianoRollEditMode(EditorViewGlobal::DrawPitch);
        expect(capabilityChangeCount == 1 &&
                   !controller->supportsEditCommand(EditorInteraction::Command::SelectAll),
               "entering pitch drawing must publish disabled note command capabilities");
        controller->requestEditCommand(EditorInteraction::Command::SelectAll);
        controller->requestEditCommand(EditorInteraction::Command::DeleteSelection);
        expect(commandCount == 1, "pitch drawing must not dispatch note edit commands");

        controller->syncPianoRollEditMode(EditorViewGlobal::ModulatePitch);
        expect(capabilityChangeCount == 2 &&
                   !controller->supportsEditCommand(EditorInteraction::Command::SelectAll) &&
                   !controller->supportsEditCommand(EditorInteraction::Command::DeleteSelection),
               "entering pitch modulation must publish disabled note command capabilities");
        controller->requestEditCommand(EditorInteraction::Command::SelectAll);
        controller->requestEditCommand(EditorInteraction::Command::DeleteSelection);
        expect(commandCount == 1, "pitch modulation must not dispatch note edit commands");

        controller->syncPianoRollEditMode(EditorViewGlobal::EditPitchAnchor);
        expect(capabilityChangeCount == 3 &&
                   controller->supportsEditCommand(EditorInteraction::Command::DeleteSelection),
               "pitch anchor mode must publish anchor deletion capability");
        controller->requestEditCommand(EditorInteraction::Command::DeleteSelection);
        expect(commandCount == 2 &&
                   requestedCommand == EditorInteraction::Command::DeleteSelection,
               "pitch anchor mode must dispatch anchor deletion");

        controller->syncPianoRollEditMode(EditorViewGlobal::Select);
        QObject::disconnect(commandConnection);
        QObject::disconnect(capabilityConnection);
        controller->setActivePanel(AppGlobal::TracksEditor);
    }

    void testForwardingAndSnapshots(EditorViewController *controller) {
        FakeEditorView view;
        view.state = sampleState();
        bindEditorView(controller, &view);

        const auto captured = controller->captureState();
        expect(captured.has_value() && *captured == sampleState(),
               "capture must return the complete view state");
        expect(view.captureCount == 1, "capture must be forwarded exactly once");

        expect(controller->centerTrackPanelAt(960, 1.5), "track centering must be forwarded");
        expect(controller->setTrackPanelScale(2.5, 3.0), "track scaling must be forwarded");
        expect(controller->setPanelVisibility(false, true),
               "a single visible main panel must be accepted");
        expect(controller->showBottomPanelPage(QStringLiteral("MixConsole")),
               "stable bottom page IDs must be forwarded");
        expect(controller->centerPianoRollAt(1440, 72.5), "piano-roll centering must be forwarded");
        expect(controller->setPianoRollScale(1.25, 2.25), "piano-roll scaling must be forwarded");
        expect(controller->setPianoRollEditMode(EditorViewGlobal::ModulatePitch),
               "tool switching must be forwarded");
        controller->refreshActiveClipTrackPresentation();
        controller->previewActiveClipTrackColor(7);
        HistoryFocus focus;
        focus.objectIds = {42};
        focus.tickEnd = 10;
        expect(controller->focusVisibility(focus) == HistoryFocusVisibility::Visible,
               "focus visibility must be forwarded");
        expect(controller->revealFocus(focus), "focus reveal must be forwarded");
        expect(controller->finalizeFocus(focus), "focus finalize must be forwarded");
        controller->clearFocusPreview();

        expect(view.state.trackPanel.centerTick == 960 &&
                   view.state.trackPanel.centerTrackIndex == 1.5,
               "track semantic center must reach the view");
        expect(view.state.trackPanel.horizontalScale == 2.5 &&
                   view.state.trackPanel.verticalScale == 3.0,
               "track scale must reach the view");
        expect(!view.state.layout.trackPanelVisible && view.state.layout.bottomPanelVisible &&
                   view.state.layout.bottomPanelPageId == QStringLiteral("MixConsole"),
               "layout and page operations must reach the view");
        expect(view.state.pianoRoll.centerTick == 1440 &&
                   view.state.pianoRoll.centerKeyIndex == 72.5,
               "piano-roll semantic center must reach the view");
        expect(view.state.pianoRoll.horizontalScale == 1.25 &&
                   view.state.pianoRoll.verticalScale == 2.25 &&
                   view.state.pianoRoll.editMode == EditorViewGlobal::ModulatePitch,
               "piano-roll scale and tool must reach the view");
        expect(view.refreshCount == 1 && view.previewCount == 1 && view.previewColorIndex == 7,
               "track presentation operations must be forwarded");
        expect(view.focusVisibilityCount == 1 && view.revealFocusCount == 1 &&
                   view.finalizeFocusCount == 1 && view.clearFocusPreviewCount == 1,
               "history focus operations must be forwarded");

        const auto restored = sampleState();
        expect(controller->restoreState(restored), "a valid snapshot must restore");
        expect(view.state == restored, "snapshot restore must round-trip every field");

        const auto beforeInvalidRestore = view.state;
        auto invalidPage = restored;
        invalidPage.layout.bottomPanelPageId = QStringLiteral("MissingPage");
        expect(!controller->restoreState(invalidPage), "an unknown page ID must be rejected");
        expect(view.state == beforeInvalidRestore,
               "a rejected page ID must not partially mutate the view");

        auto bothHidden = restored;
        bothHidden.layout.trackPanelVisible = false;
        bothHidden.layout.bottomPanelVisible = false;
        expect(!controller->restoreState(bothHidden),
               "a snapshot hiding both main panels must be rejected");
        expect(view.state == beforeInvalidRestore,
               "a rejected visibility snapshot must not partially mutate the view");

        auto allClipEditorRegionsHidden = restored;
        allClipEditorRegionsHidden.layout.pianoRollVisible = false;
        allClipEditorRegionsHidden.layout.parametersVisible = false;
        expect(!controller->restoreState(allClipEditorRegionsHidden),
               "a snapshot hiding both clip-editor regions must be rejected");
        expect(view.state == beforeInvalidRestore,
               "a rejected clip-editor visibility snapshot must not partially mutate the view");

        auto invalidScale = restored;
        invalidScale.pianoRoll.horizontalScale = 0;
        expect(!controller->restoreState(invalidScale),
               "a snapshot with a non-positive scale must be rejected");
        expect(view.state == beforeInvalidRestore,
               "a rejected scale snapshot must not partially mutate the view");

        const auto visibilityCallCount = view.visibilityCallCount;
        expect(!controller->setPanelVisibility(false, false),
               "direct panel control must reject hiding both panels");
        expect(view.visibilityCallCount == visibilityCallCount,
               "the controller must reject both-hidden before touching the view");
        expect(!controller->showBottomPanelPage(QStringLiteral("MissingPage")),
               "direct page control must report an unknown stable ID");

        bindEditorView(controller, nullptr);
    }

    void testActivePanels(EditorViewController *controller) {
        controller->setActivePanel(AppGlobal::TracksEditor);
        FakePanel trackPanel(AppGlobal::TracksEditor);
        FakePanel bottomPanel(AppGlobal::ClipEditor);
        controller->registerPanel(&trackPanel);
        controller->registerPanel(&bottomPanel);

        expect(trackPanel.panelActive() && !bottomPanel.panelActive(),
               "registration must apply the current active context");

        int signalCount = 0;
        AppGlobal::PanelType signaledPanel = AppGlobal::Generic;
        const auto connection =
            QObject::connect(controller, &EditorViewController::activePanelChanged,
                             [&signalCount, &signaledPanel](const AppGlobal::PanelType panel) {
                                 ++signalCount;
                                 signaledPanel = panel;
                             });

        controller->setActivePanel(AppGlobal::ClipEditor);
        expect(!trackPanel.panelActive() && bottomPanel.panelActive(),
               "active context changes must update every registered panel");
        expect(signalCount == 1 && signaledPanel == AppGlobal::ClipEditor,
               "active context changes must be observable by menu callers");

        bottomPanel.setPanelType(AppGlobal::Generic);
        controller->setActivePanel(AppGlobal::Generic);
        expect(!trackPanel.panelActive() && bottomPanel.panelActive(),
               "registered tab panels must honor their current dynamic page type");

        QObject::disconnect(connection);
        controller->unregisterPanel(&bottomPanel);
        controller->unregisterPanel(&trackPanel);
    }

    void testInteractionRouting(EditorViewController *controller) {
        controller->setActivePanel(AppGlobal::TracksEditor);

        QObject trackArea;
        QObject trackChild(&trackArea);
        QObject bottomContainer;
        QObject titleBarArea(&bottomContainer);
        QObject titleBarChild(&titleBarArea);
        QObject pianoRollArea(&bottomContainer);
        QObject pianoRollChild(&pianoRollArea);
        QObject parameterArea(&bottomContainer);
        QObject parameterChild(&parameterArea);
        QObject separator(&bottomContainer);

        controller->registerInteractionArea(&trackArea, AppGlobal::TracksEditor,
                                            EditorInteraction::Target::Tracks);
        controller->registerInteractionArea(&titleBarArea, AppGlobal::ClipEditor,
                                            EditorInteraction::Target::PianoRoll);
        controller->registerInteractionArea(&pianoRollArea, AppGlobal::ClipEditor,
                                            EditorInteraction::Target::PianoRoll);
        controller->registerInteractionArea(&parameterArea, AppGlobal::ClipEditor,
                                            EditorInteraction::Target::Parameters);

        int panelSignalCount = 0;
        int targetSignalCount = 0;
        const auto panelConnection =
            QObject::connect(controller, &EditorViewController::activePanelChanged,
                             [&panelSignalCount] { ++panelSignalCount; });
        const auto targetConnection =
            QObject::connect(controller, &EditorViewController::activeEditTargetChanged,
                             [&targetSignalCount] { ++targetSignalCount; });

        QEvent titleBarPress(QEvent::MouseButtonPress);
        QCoreApplication::sendEvent(&titleBarChild, &titleBarPress);
        expect(controller->activePanel() == AppGlobal::ClipEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::PianoRoll,
               "the visible piano-roll toolbar must activate note editing");
        expect(panelSignalCount == 1 && targetSignalCount == 1,
               "a context change must emit each state transition once");

        QEvent pianoPress(QEvent::MouseButtonPress);
        QCoreApplication::sendEvent(&pianoRollChild, &pianoPress);
        expect(panelSignalCount == 1 && targetSignalCount == 1,
               "moving within the visual note editor must not duplicate state signals");

        QEvent parameterPress(QEvent::MouseButtonPress);
        QCoreApplication::sendEvent(&parameterChild, &parameterPress);
        expect(controller->activePanel() == AppGlobal::ClipEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::Parameters,
               "the visual parameter editor must activate parameter editing independently");
        expect(panelSignalCount == 1 && targetSignalCount == 2,
               "switching between bottom edit areas must preserve the panel border");

        QEvent separatorPress(QEvent::MouseButtonPress);
        QCoreApplication::sendEvent(&separator, &separatorPress);
        expect(controller->activeEditTarget() == EditorInteraction::Target::Parameters &&
                   targetSignalCount == 2,
               "unassigned container space must not inherit a sibling editor target");

        QEvent trackFocus(QEvent::FocusIn);
        QCoreApplication::sendEvent(&trackChild, &trackFocus);
        expect(controller->activePanel() == AppGlobal::TracksEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::Tracks,
               "focus entering a registered track descendant must activate track editing");

        EditorInteraction::Target commandTarget = EditorInteraction::Target::None;
        EditorInteraction::Command requestedCommand = EditorInteraction::Command::Cut;
        int commandCount = 0;
        const auto commandConnection = QObject::connect(
            controller, &EditorViewController::editCommandRequested,
            [&commandTarget, &requestedCommand, &commandCount](
                const EditorInteraction::Target target, const EditorInteraction::Command command) {
                commandTarget = target;
                requestedCommand = command;
                ++commandCount;
            });
        controller->requestEditCommand(EditorInteraction::Command::SelectAll);
        expect(commandCount == 1 && commandTarget == EditorInteraction::Target::Tracks &&
                   requestedCommand == EditorInteraction::Command::SelectAll,
               "edit commands must carry the current interaction target");

        controller->updateInteractionArea(&titleBarArea, AppGlobal::Generic,
                                          EditorInteraction::Target::None);
        QEvent genericPress(QEvent::MouseButtonPress);
        QCoreApplication::sendEvent(&titleBarChild, &genericPress);
        expect(controller->activePanel() == AppGlobal::Generic &&
                   controller->activeEditTarget() == EditorInteraction::Target::None,
               "a dynamic generic bottom page must deactivate editor commands");

        QObject::disconnect(commandConnection);
        QObject::disconnect(targetConnection);
        QObject::disconnect(panelConnection);
        controller->unregisterInteractionArea(&parameterArea);
        controller->unregisterInteractionArea(&pianoRollArea);
        controller->unregisterInteractionArea(&titleBarArea);
        controller->unregisterInteractionArea(&trackArea);
        controller->setActivePanel(AppGlobal::TracksEditor);
    }

    void testPanelVisibilityRouting(EditorViewController *controller) {
        controller->setActivePanel(AppGlobal::TracksEditor);
        FakePanel trackPanel(AppGlobal::TracksEditor);
        FakePanel bottomPanel(AppGlobal::ClipEditor);
        controller->registerPanel(&trackPanel);
        controller->registerPanel(&bottomPanel);

        QObject parameterArea;
        controller->registerInteractionArea(&parameterArea, AppGlobal::ClipEditor,
                                            EditorInteraction::Target::Parameters);
        QEvent parameterPress(QEvent::MouseButtonPress);
        QCoreApplication::sendEvent(&parameterArea, &parameterPress);

        controller->syncPanelVisibility(true, true, AppGlobal::ClipEditor);
        expect(controller->activePanel() == AppGlobal::ClipEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::Parameters,
               "showing both panels must preserve the focused visual editor");

        controller->activatePanelContext(AppGlobal::TracksEditor);
        controller->activatePanelContext(AppGlobal::ClipEditor);
        expect(controller->activePanel() == AppGlobal::ClipEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::Parameters,
               "reactivating the bottom window must restore its last focused visual editor");

        controller->syncPanelVisibility(true, false, AppGlobal::ClipEditor);
        expect(controller->activePanel() == AppGlobal::TracksEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::Tracks &&
                   trackPanel.panelActive() && !bottomPanel.panelActive(),
               "hiding the bottom panel must transfer focus and commands to tracks");

        controller->syncPanelVisibility(true, true, AppGlobal::ClipEditor);
        expect(controller->activePanel() == AppGlobal::TracksEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::Tracks,
               "reopening the bottom panel must not steal track focus");

        controller->syncPanelVisibility(false, true, AppGlobal::ClipEditor);
        expect(controller->activePanel() == AppGlobal::ClipEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::Parameters &&
                   !trackPanel.panelActive() && bottomPanel.panelActive(),
               "hiding tracks must restore the last focused region of the visible bottom page");

        QCoreApplication::sendEvent(&parameterArea, &parameterPress);
        controller->syncPanelVisibility(false, true, AppGlobal::ClipEditor);
        expect(controller->activeEditTarget() == EditorInteraction::Target::Parameters,
               "visibility sync must preserve a focused parameter editor");

        controller->syncEditTargetVisibility(EditorInteraction::Target::Parameters, true,
                                             AppGlobal::ClipEditor);
        expect(controller->activeEditTarget() == EditorInteraction::Target::Parameters,
               "a visible parameter editor must retain its edit target");
        controller->syncEditTargetVisibility(EditorInteraction::Target::Parameters, false,
                                             AppGlobal::ClipEditor);
        expect(controller->activePanel() == AppGlobal::ClipEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::PianoRoll,
               "collapsing the parameter editor must transfer commands to the piano roll");

        controller->setActivePanel(AppGlobal::TracksEditor);
        controller->syncEditTargetVisibility(EditorInteraction::Target::Parameters, false,
                                             AppGlobal::ClipEditor);
        expect(controller->activePanel() == AppGlobal::TracksEditor &&
                   controller->activeEditTarget() == EditorInteraction::Target::Tracks,
               "collapsing an unfocused edit target must not steal focus");
        bottomPanel.setPanelType(AppGlobal::Generic);
        controller->syncPanelVisibility(false, true, AppGlobal::Generic);
        expect(controller->activePanel() == AppGlobal::Generic &&
                   controller->activeEditTarget() == EditorInteraction::Target::None,
               "a visible non-editor bottom page must disable edit commands");

        controller->unregisterInteractionArea(&parameterArea);
        controller->unregisterPanel(&bottomPanel);
        controller->unregisterPanel(&trackPanel);
        controller->setActivePanel(AppGlobal::TracksEditor);
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    AutomationTestSupport::TestRuntime testRuntime(
        AutomationTestSupport::editorServices(&g_editorHost));
    g_runtime = &testRuntime.runtime();
    auto *controller = editorViewController;

    testNoView(controller);
    testCommandCapabilities();
    testModeAwareCommandRouting(controller);
    testForwardingAndSnapshots(controller);
    testActivePanels(controller);
    testInteractionRouting(controller);
    testPanelVisibilityRouting(controller);

    bindEditorView(controller, nullptr);
    g_runtime = nullptr;
    if (g_failures == 0) {
        QTextStream(stdout) << "All EditorViewController tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
