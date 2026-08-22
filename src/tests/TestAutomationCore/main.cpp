#include "Automation/AutomationDispatcher.h"
#include "Automation/CoreRuntime.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>
#include <functional>

namespace {
    bool expect(const bool condition, const char *message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    class FakeResolver final : public Automation::IDocumentSessionResolver {
    public:
        FakeResolver(Automation::DocumentSession &first, Automation::DocumentSession &second)
            : m_first(first), m_second(second) {
        }

        Automation::AutomationResult<std::reference_wrapper<Automation::DocumentSession>>
        resolveDocument(const Automation::DocumentId &documentId) override {
            if (documentId == m_first.documentId())
                return std::ref(m_first);
            if (documentId == m_second.documentId())
                return std::ref(m_second);
            return Automation::AutomationError::documentChanged(documentId,
                                                                m_first.documentId());
        }

    private:
        Automation::DocumentSession &m_first;
        Automation::DocumentSession &m_second;
    };

    Automation::OperationDescriptor commandDescriptor() {
        return {
            .id = QStringLiteral("test.command"),
            .category = QStringLiteral("test"),
            .kind = Automation::OperationKind::Command,
            .syncMode = Automation::SyncMode::Synchronous,
            .inputContract = QStringLiteral("test.CommandInput.v1"),
            .outputContract = QStringLiteral("automation.MutationResult.v1"),
            .documentPolicy = Automation::DocumentPolicy::Write,
            .revisionPolicy = Automation::RevisionPolicy::Increment,
            .historyPolicy = Automation::HistoryPolicy::Record,
            .fileAccess = Automation::FileAccessPolicy::None,
            .hostAvailability = Automation::HostAvailability::Core,
            .safety = Automation::SafetyClass::Reversible,
            .exposure = Automation::ExposurePolicy::InternalOnly,
            .idempotency = Automation::IdempotencyPolicy::DocumentGeneration,
        };
    }

    Automation::OperationDescriptor queryDescriptor() {
        return {
            .id = QStringLiteral("test.query"),
            .category = QStringLiteral("test"),
            .kind = Automation::OperationKind::Query,
            .syncMode = Automation::SyncMode::Synchronous,
            .inputContract = QStringLiteral("automation.DocumentRef.v1"),
            .outputContract = QStringLiteral("test.Revision.v1"),
            .documentPolicy = Automation::DocumentPolicy::Read,
            .revisionPolicy = Automation::RevisionPolicy::None,
            .historyPolicy = Automation::HistoryPolicy::None,
            .fileAccess = Automation::FileAccessPolicy::None,
            .hostAvailability = Automation::HostAvailability::Core,
            .safety = Automation::SafetyClass::ReadOnly,
            .exposure = Automation::ExposurePolicy::InternalOnly,
            .idempotency = Automation::IdempotencyPolicy::Unsupported,
        };
    }

    bool hasTempoAt(const AppModel &model, const int tick, const double value) {
        const auto &tempos = model.timeline().tempos();
        return std::any_of(tempos.cbegin(), tempos.cend(), [tick, value](const Tempo &tempo) {
            return tempo.pos == tick && tempo.value == value;
        });
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool ok = true;

    Automation::DocumentSession first(nullptr, nullptr);
    Automation::DocumentSession second(nullptr, nullptr);
    FakeResolver resolver(first, second);
    Automation::SingleWindowContext window;
    Automation::OperationCatalog catalog;

    ok &= expect(catalog.add(queryDescriptor()).isPresent(), "query descriptor must register");
    ok &= expect(catalog.add(commandDescriptor()).isPresent(), "command descriptor must register");
    ok &= expect(!catalog.add(commandDescriptor()).isPresent(),
                 "duplicate operation ID must be rejected");

    Automation::AutomationDispatcher dispatcher(resolver, window, catalog);
    auto secondQuery = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), second.documentId(),
        [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &= expect(secondQuery && secondQuery.get() == 0,
                 "dispatcher must route by explicit document ID");

    Automation::CommandContext context;
    context.expected = first.version();
    context.idempotencyKey = QStringLiteral("8ff6e1d7-a1d7-463d-9a6b-f85913fe0773");
    int executionCount = 0;
    const auto handler = [&executionCount](Automation::DocumentSession &session,
                                           const bool validateOnly) {
        ++executionCount;
        Automation::MutationResult result;
        result.previous = session.version();
        result.changed = true;
        result.validatedOnly = validateOnly;
        result.current = validateOnly ? session.version() : session.advanceRevision();
        return Automation::AutomationResult<Automation::MutationResult>(result);
    };

    const auto firstResult = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), context, QByteArrayLiteral("payload"), handler);
    ok &= expect(firstResult && firstResult.get().current.revision == 1,
                 "command must return the committed revision");

    const auto replayed = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), context, QByteArrayLiteral("payload"), handler);
    ok &= expect(replayed && replayed.get() == firstResult.get(),
                 "same idempotency key must replay the original result");
    ok &= expect(executionCount == 1 && first.revision() == 1,
                 "idempotent replay must not execute or increment revision again");

    const auto conflict = dispatcher.dispatchDocumentCommand(
        QStringLiteral("test.command"), context, QByteArrayLiteral("different"), handler);
    ok &= expect(!conflict &&
                     conflict.getError().code == Automation::AutomationErrorCode::IdempotencyConflict,
                 "same key with another request must fail with idempotency conflict");

    const auto oldDocumentId = first.documentId();
    first.replaceGeneration({}, QStringLiteral("Replacement"));
    ok &= expect(first.idempotencyStore().size() == 0,
                 "replacing the generation must clear idempotency records");
    const auto stale = dispatcher.dispatchDocumentQuery<Automation::Revision>(
        QStringLiteral("test.query"), oldDocumentId,
        [](Automation::DocumentSession &session) {
            return Automation::AutomationResult<Automation::Revision>(session.revision());
        });
    ok &= expect(!stale && stale.getError().code == Automation::AutomationErrorCode::DocumentChanged,
                 "old document ID must fail after generation replacement");

    const auto invalidWindow = window.validateWindow(Automation::WindowId::create());
    ok &= expect(!invalidWindow &&
                     invalidWindow.getError().code ==
                         Automation::AutomationErrorCode::HostCapabilityUnavailable,
                 "single-window host must reject another window ID");

    AppModel model;
    auto *history = HistoryManager::instance();
    history->reset();
    Automation::CoreRuntime runtime(&model, history);
    const auto state = runtime.facade().getEditorState(runtime.windowId());
    const auto capabilities = runtime.facade().getEditorCapabilities();
    ok &= expect(state && state.get().document == runtime.documentVersion(),
                 "editor state must include the current document version");
    ok &= expect(capabilities && capabilities.get().maxConcurrentDocuments == 1 &&
                     capabilities.get().maxConcurrentWindows == 1,
                 "capabilities must declare the single document/window boundary");
    ok &= expect(capabilities && capabilities.get().operationIds.size() == 11 &&
                     capabilities.get().operationIds.contains(QStringLiteral("history.undo")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("tempos.set")),
                 "capabilities must be derived from every registered operation");

    Automation::CommandContext setTempoContext;
    setTempoContext.expected = runtime.documentVersion();
    const auto setTempo = runtime.timeline().setTempo(setTempoContext, 960, 150.0);
    ok &= expect(setTempo && setTempo.get().changed &&
                     setTempo.get().current.revision == 1 && hasTempoAt(model, 960, 150.0),
                 "timeline mutation must commit one action and one revision");

    Automation::CommandContext noOpContext;
    noOpContext.expected = runtime.documentVersion();
    const auto noOp = runtime.timeline().setTempo(noOpContext, 960, 150.0);
    ok &= expect(noOp && !noOp.get().changed && runtime.documentVersion().revision == 1,
                 "legal no-op must not record history or advance revision");

    Automation::CommandContext validateContext;
    validateContext.expected = runtime.documentVersion();
    validateContext.validateOnly = true;
    const auto preview = runtime.timeline().setTempo(validateContext, 960, 160.0);
    ok &= expect(preview && preview.get().validatedOnly && preview.get().changed &&
                     preview.get().current.revision == 2 &&
                     runtime.documentVersion().revision == 1 && hasTempoAt(model, 960, 150.0),
                 "validate-only must predict the result without changing model or revision");

    Automation::CommandContext staleContext;
    staleContext.expected = {runtime.documentVersion().documentId, 0};
    const auto staleMutation = runtime.timeline().setTempo(staleContext, 960, 160.0);
    ok &= expect(!staleMutation &&
                     staleMutation.getError().code ==
                         Automation::AutomationErrorCode::RevisionConflict,
                 "revision validation must precede domain mutation");

    const auto historyState = runtime.history().getState(runtime.documentVersion().documentId);
    ok &= expect(historyState && historyState.get().canUndo && !historyState.get().canRedo,
                 "history query must reflect the committed timeline action");

    Automation::CommandContext undoContext;
    undoContext.expected = runtime.documentVersion();
    const auto undo = runtime.history().undo(undoContext);
    ok &= expect(undo && undo.get().changed && undo.get().current.revision == 2 &&
                     !hasTempoAt(model, 960, 150.0),
                 "undo must use the same revision-owning commit path");

    Automation::CommandContext redoContext;
    redoContext.expected = runtime.documentVersion();
    const auto redo = runtime.history().redo(redoContext);
    ok &= expect(redo && redo.get().changed && redo.get().current.revision == 3 &&
                     hasTempoAt(model, 960, 150.0),
                 "redo must use the same revision-owning commit path");

    Automation::CommandContext emptyRedoContext;
    emptyRedoContext.expected = runtime.documentVersion();
    const auto emptyRedo = runtime.history().redo(emptyRedoContext);
    ok &= expect(emptyRedo && !emptyRedo.get().changed && runtime.documentVersion().revision == 3,
                 "empty redo must be a successful no-op");

    history->reset();

    return ok ? 0 : 1;
}
