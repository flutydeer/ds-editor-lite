#include "Controller/EditorViewController.h"
#include "Controller/UndoRedoController.h"
#include "Interface/IEditorView.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/History/ActionSequence.h"
#include "Modules/History/HistoryManager.h"

#include <QCoreApplication>
#include <QTextStream>

template <>
EditorViewController *AppContext::instance<EditorViewController>() {
    return nullptr;
}

template <>
UndoRedoController *AppContext::instance<UndoRedoController>() {
    return nullptr;
}

template <>
HistoryManager *AppContext::instance<HistoryManager>() {
    return nullptr;
}

template <>
AppStatus *AppContext::instance<AppStatus>() {
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

    class CounterAction final : public IAction {
    public:
        explicit CounterAction(int *value) : m_value(value) {
        }

        void execute() override {
            ++*m_value;
        }

        void undo() override {
            --*m_value;
        }

    private:
        int *m_value;
    };

    class TestSequence final : public ActionSequence {
    public:
        TestSequence(int *value, const bool withFocus) {
            addAction(new CounterAction(value));
            setName(QStringLiteral("Test action"));
            if (withFocus) {
                HistoryFocus focus;
                focus.kind = HistoryFocusKind::TrackClips;
                focus.objectIds = {42};
                focus.tickStart = 100;
                focus.tickEnd = 200;
                focus.valueStart = 1;
                focus.valueEnd = 1;
                setFocusTransition({focus, focus});
            }
        }
    };

    class FakeEditorView final : public IEditorView {
    public:
        HistoryFocusVisibility visibility = HistoryFocusVisibility::Visible;
        bool revealResult = true;
        int visibilityCount = 0;
        int revealCount = 0;
        int finalizeCount = 0;
        int clearCount = 0;

        EditorViewState captureEditorViewState() const override {
            return {};
        }

        bool restoreEditorViewState(const EditorViewState &) override {
            return true;
        }

        bool centerTrackPanelAt(double, double) override {
            return true;
        }

        bool setTrackPanelScale(double, double) override {
            return true;
        }

        bool setEditorPanelVisibility(bool, bool) override {
            return true;
        }

        bool showBottomPanelPage(const QString &) override {
            return true;
        }

        bool centerPianoRollAt(double, double) override {
            return true;
        }

        bool setPianoRollScale(double, double) override {
            return true;
        }

        bool setPianoRollEditMode(EditorViewGlobal::PianoRollEditMode) override {
            return true;
        }

        void refreshActiveClipTrackPresentation() override {
        }

        void previewActiveClipTrackColor(int) override {
        }

        HistoryFocusVisibility focusVisibility(const HistoryFocus &) const override {
            ++const_cast<FakeEditorView *>(this)->visibilityCount;
            return visibility;
        }

        bool revealFocus(const HistoryFocus &) override {
            ++revealCount;
            return revealResult;
        }

        bool finalizeFocus(const HistoryFocus &) override {
            ++finalizeCount;
            return true;
        }

        void clearFocusPreview() override {
            ++clearCount;
        }
    };

    void reset(FakeEditorView &view) {
        historyManager->reset();
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
        view.visibility = HistoryFocusVisibility::Visible;
        view.revealResult = true;
        view.visibilityCount = 0;
        view.revealCount = 0;
        view.finalizeCount = 0;
        view.clearCount = 0;
    }

    TestSequence *recordAction(int &value, const bool withFocus = true) {
        auto *sequence = new TestSequence(&value, withFocus);
        sequence->execute();
        historyManager->record(sequence);
        return sequence;
    }

    void testVisibleExecutesImmediately(FakeEditorView &view) {
        reset(view);
        int value = 0;
        const auto sequence = recordAction(value);
        expect(sequence->historyId() != 0, "record must assign a stable history ID");
        undoRedoController->requestUndo();
        expect(value == 0, "visible focus must undo immediately");
        expect(view.revealCount == 0 && view.finalizeCount == 1,
               "visible focus must skip preview and finalize the result");
    }

    void testScrollRequiredExecutesOnSecondRequest(FakeEditorView &view) {
        reset(view);
        int value = 0;
        recordAction(value);
        view.visibility = HistoryFocusVisibility::ScrollRequired;
        undoRedoController->requestUndo();
        expect(value == 1 && historyManager->canUndo(),
               "first hidden request must not mutate the model or stack");
        expect(view.revealCount == 1, "first hidden request must reveal the focus");

        undoRedoController->requestUndo();
        expect(value == 0 && historyManager->canRedo(),
               "second matching request must execute while scrolling is still in progress");
        expect(view.revealCount == 1 && view.finalizeCount == 1 && view.clearCount == 1,
               "executing a pending request must clear preview and finalize focus");
    }

    void testRedoUsesTwoPhases(FakeEditorView &view) {
        reset(view);
        int value = 0;
        recordAction(value);
        undoRedoController->requestUndo();
        expect(value == 0 && historyManager->canRedo(), "test setup must create a redo entry");

        view.visibility = HistoryFocusVisibility::ScrollRequired;
        undoRedoController->requestRedo();
        expect(value == 0 && view.revealCount == 1,
               "first hidden redo request must only reveal the before focus");
        undoRedoController->requestRedo();
        expect(value == 1 && view.finalizeCount == 2,
               "second matching redo request must execute before scrolling completes");
    }

    void testContextSwitchExecutesOnSecondRequest(FakeEditorView &view) {
        reset(view);
        int value = 0;
        recordAction(value);
        view.visibility = HistoryFocusVisibility::ContextSwitchRequired;
        undoRedoController->requestUndo();
        expect(value == 1 && view.revealCount == 1,
               "first context-switch request must only navigate");

        undoRedoController->requestUndo();
        expect(value == 0 && view.revealCount == 1 && view.finalizeCount == 1,
               "second context-switch request must execute without navigating again");
    }

    void testDirectionChangeClearsPending(FakeEditorView &view) {
        reset(view);
        int value = 0;
        recordAction(value);
        view.visibility = HistoryFocusVisibility::ScrollRequired;
        undoRedoController->requestUndo();
        undoRedoController->requestRedo();
        expect(view.clearCount == 1, "changing direction must clear pending navigation");
        view.visibility = HistoryFocusVisibility::Visible;
        undoRedoController->requestUndo();
        expect(value == 0, "request after direction change must be evaluated afresh");
    }

    void testHistoryChangeInvalidatesPending(FakeEditorView &view) {
        reset(view);
        int value = 0;
        recordAction(value);
        view.visibility = HistoryFocusVisibility::ScrollRequired;
        undoRedoController->requestUndo();
        recordAction(value);
        expect(view.clearCount == 1, "recording a new entry must invalidate pending navigation");
        view.visibility = HistoryFocusVisibility::Visible;
        undoRedoController->requestUndo();
        expect(value == 1,
               "the new stack top must execute instead of the previously pending entry");
    }

    void testFallbacksAndEditGuard(FakeEditorView &view) {
        reset(view);
        int value = 0;
        recordAction(value, false);
        undoRedoController->requestUndo();
        expect(value == 0, "history without focus metadata must remain one-step undo");

        reset(view);
        value = 0;
        recordAction(value);
        view.visibility = HistoryFocusVisibility::Unavailable;
        undoRedoController->requestUndo();
        expect(value == 0, "unavailable focus navigation must fall back to direct undo");

        reset(view);
        value = 0;
        recordAction(value);
        view.visibility = HistoryFocusVisibility::ScrollRequired;
        view.revealResult = false;
        undoRedoController->requestUndo();
        expect(value == 0, "failed focus navigation must not block undo");

        reset(view);
        value = 0;
        recordAction(value);
        view.visibility = HistoryFocusVisibility::ContextSwitchRequired;
        appStatus->currentEditObject = AppStatus::EditObjectType::Note;
        undoRedoController->requestUndo();
        expect(value == 1 && view.revealCount == 0,
               "an active edit transaction must neither navigate nor execute");
        appStatus->currentEditObject = AppStatus::EditObjectType::None;
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    FakeEditorView view;
    editorViewController->setView(&view);

    testVisibleExecutesImmediately(view);
    testScrollRequiredExecutesOnSecondRequest(view);
    testRedoUsesTwoPhases(view);
    testContextSwitchExecutesOnSecondRequest(view);
    testDirectionChangeClearsPending(view);
    testHistoryChangeInvalidatesPending(view);
    testFallbacksAndEditGuard(view);

    editorViewController->setView(nullptr);
    historyManager->reset();
    if (g_failures == 0) {
        QTextStream(stdout) << "All UndoRedoController tests passed" << Qt::endl;
        return 0;
    }
    QTextStream(stderr) << g_failures << " test(s) failed" << Qt::endl;
    return 1;
}
