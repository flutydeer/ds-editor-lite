#include "Controller/EditorViewController.h"
#include "Interface/IEditorView.h"
#include "Interface/IPanel.h"

#include <QCoreApplication>
#include <QTextStream>

#include <cmath>

template <>
EditorViewController *AppContext::instance<EditorViewController>() {
    return nullptr;
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
               (state.layout.bottomPanelPageId == QStringLiteral("ClipEditor") ||
                state.layout.bottomPanelPageId == QStringLiteral("MixConsole")) &&
               state.pianoRoll.editMode >= EditorViewGlobal::Select &&
               state.pianoRoll.editMode <= EditorViewGlobal::ErasePitch &&
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

        bool setPianoRollEditMode(const EditorViewGlobal::PianoRollEditMode mode) override {
            if (mode < EditorViewGlobal::Select || mode > EditorViewGlobal::ErasePitch)
                return false;
            state.pianoRoll.editMode = mode;
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
        controller->setView(nullptr);
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

    void testForwardingAndSnapshots(EditorViewController *controller) {
        FakeEditorView view;
        view.state = sampleState();
        controller->setView(&view);

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
        expect(controller->setPianoRollEditMode(EditorViewGlobal::ErasePitch),
               "tool switching must be forwarded");
        controller->refreshActiveClipTrackPresentation();
        controller->previewActiveClipTrackColor(7);
        HistoryFocus focus;
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
                   view.state.pianoRoll.editMode == EditorViewGlobal::ErasePitch,
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

        controller->setView(nullptr);
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

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    auto *controller = editorViewController;

    testNoView(controller);
    testForwardingAndSnapshots(controller);
    testActivePanels(controller);

    controller->setView(nullptr);
    if (g_failures == 0) {
        QTextStream(stdout) << "All EditorViewController tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
