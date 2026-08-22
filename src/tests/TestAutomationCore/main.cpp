#include "Automation/AutomationDispatcher.h"
#include "Automation/CoreRuntime.h"
#include "Automation/ProjectAutomationDtos.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

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

    Automation::CommandContext commandContext(const Automation::CoreRuntime &runtime,
                                              const bool validateOnly = false) {
        return {.expected = runtime.documentVersion(),
                .validateOnly = validateOnly,
                .source = Automation::InvocationSource::Test};
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
    ok &= expect(capabilities && capabilities.get().operationIds.size() == 44 &&
                     capabilities.get().operationIds.contains(QStringLiteral("history.undo")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("tempos.set")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("tracks.insert")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("notes.insert")) &&
                     capabilities.get().operationIds.contains(QStringLiteral("parameters.replace")),
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

    Automation::TrackDraftDto trackDraft;
    trackDraft.name = QStringLiteral("Automation Track");
    trackDraft.gain = 1.0;
    trackDraft.defaultLanguage = QStringLiteral("unknown");

    const auto trackPreview = runtime.project().insertTrack(commandContext(runtime, true), 0,
                                                            trackDraft);
    ok &= expect(trackPreview && trackPreview.get().validatedOnly && trackPreview.get().changed &&
                     trackPreview.get().current.revision == runtime.documentVersion().revision + 1 &&
                     model.tracks().isEmpty(),
                 "track validate-only must not allocate or insert a track");

    const auto insertTrack = runtime.project().insertTrack(commandContext(runtime), 0, trackDraft);
    ok &= expect(insertTrack && insertTrack.get().changed &&
                     insertTrack.get().affectedObjects.size() == 1 && model.tracks().size() == 1,
                 "track insertion must commit once and return the new track ID");
    const auto trackId = insertTrack
                             ? Automation::TrackId(insertTrack.get().affectedObjects.first().value)
                             : Automation::TrackId();
    auto *track = model.findTrackById(trackId.value());
    ok &= expect(track && track->name() == trackDraft.name,
                 "inserted track must preserve requested properties");

    Automation::CommandContext wrongDocumentContext = commandContext(runtime);
    wrongDocumentContext.expected = {Automation::DocumentId::create(), 999};
    const auto wrongDocument =
        runtime.project().setTrackColor(wrongDocumentContext, Automation::TrackId(999999), 1);
    ok &= expect(!wrongDocument &&
                     wrongDocument.getError().code ==
                         Automation::AutomationErrorCode::DocumentChanged,
                 "document ID validation must precede revision and object validation");

    Automation::CommandContext staleObjectContext = commandContext(runtime);
    staleObjectContext.expected.revision -= 1;
    const auto staleObject =
        runtime.project().setTrackColor(staleObjectContext, Automation::TrackId(999999), 1);
    ok &= expect(!staleObject &&
                     staleObject.getError().code ==
                         Automation::AutomationErrorCode::RevisionConflict,
                 "revision validation must precede object validation");

    Automation::ClipDraftDto clipDraft;
    clipDraft.type = Automation::ClipDraftDto::Type::Singing;
    clipDraft.properties.name = QStringLiteral("Automation Clip");
    clipDraft.properties.start = 0;
    clipDraft.properties.length = 1920;
    clipDraft.properties.clipStart = 0;
    clipDraft.properties.clipLen = 1920;
    clipDraft.properties.gain = 1.0;
    clipDraft.defaultLanguage = QStringLiteral("unknown");
    const auto insertClip = runtime.project().insertClips(
        commandContext(runtime), {{.trackId = trackId, .clip = clipDraft}});
    ok &= expect(insertClip && insertClip.get().changed &&
                     insertClip.get().affectedObjects.size() == 1 && track->clips().count() == 1,
                 "singing clip insertion must commit through the project facade");
    const auto clipId = insertClip
                            ? Automation::ClipId(insertClip.get().affectedObjects.first().value)
                            : Automation::ClipId();
    auto *singingClip = dynamic_cast<SingingClip *>(model.findClipById(clipId.value()));
    ok &= expect(singingClip && singingClip->name() == clipDraft.properties.name,
                 "inserted singing clip must be addressable by its returned ID");

    const auto wrongClipType =
        runtime.project().confirmAudioClipPath(commandContext(runtime), clipId);
    ok &= expect(!wrongClipType &&
                     wrongClipType.getError().code ==
                         Automation::AutomationErrorCode::WrongObjectType,
                 "typed object resolution must distinguish missing and wrong-type clips");

    Automation::NoteDraftDto noteDraft;
    noteDraft.localStart = 0;
    noteDraft.length = 480;
    noteDraft.keyIndex = 60;
    noteDraft.lyric = QStringLiteral("la");
    const auto notePreview = runtime.notes().insertNotes(commandContext(runtime, true), clipId,
                                                        {noteDraft});
    ok &= expect(notePreview && notePreview.get().validatedOnly &&
                     singingClip->notes().count() == 0,
                 "note validate-only must not allocate or attach notes");

    const auto insertNote =
        runtime.notes().insertNotes(commandContext(runtime), clipId, {noteDraft});
    ok &= expect(insertNote && insertNote.get().changed &&
                     insertNote.get().affectedObjects.size() == 1 &&
                     singingClip->notes().count() == 1,
                 "note insertion must return the inserted note ID");
    const auto noteId = insertNote
                            ? Automation::NoteId(insertNote.get().affectedObjects.first().value)
                            : Automation::NoteId();
    auto *note = singingClip->findNoteById(noteId.value());

    const auto beforeNoteNoOp = runtime.documentVersion();
    const auto noteNoOp =
        runtime.notes().moveNotes(commandContext(runtime), clipId, {noteId}, 0, 0);
    ok &= expect(noteNoOp && !noteNoOp.get().changed &&
                     runtime.documentVersion() == beforeNoteNoOp,
                 "zero-distance note movement must be a successful no-op");

    const auto moveNote =
        runtime.notes().moveNotes(commandContext(runtime), clipId, {noteId}, 120, 2);
    ok &= expect(moveNote && moveNote.get().changed && note && note->localStart() == 120 &&
                     note->keyIndex() == 62,
                 "note movement must update time and key in one revision");

    Automation::CurveDraftDto curveDraft;
    curveDraft.type = Automation::CurveDraftDto::Type::Draw;
    curveDraft.localStart = 120;
    curveDraft.step = 5;
    curveDraft.values = {6000, 6025, 6050};
    const auto replaceParameter = runtime.parameters().replaceParameter(
        commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited, {curveDraft});
    ok &= expect(replaceParameter && replaceParameter.get().changed,
                 "parameter replacement must commit through the parameter facade");
    const auto parameter = runtime.parameters().getParameter(
        runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
    ok &= expect(parameter && parameter.get().curves.size() == 1 &&
                     parameter.get().curves.first().values == curveDraft.values,
                 "parameter query must return a value snapshot of edited curves");

    const auto parameterVersion = runtime.documentVersion();
    const auto parameterNoOp = runtime.parameters().replaceParameter(
        commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited, {curveDraft});
    ok &= expect(parameterNoOp && !parameterNoOp.get().changed &&
                     runtime.documentVersion() == parameterVersion,
                 "replacing a parameter with identical curves must not add history or revision");

    auto copiedClipDraft = Automation::clipDraftDto(*singingClip);
    copiedClipDraft.properties.start = 2400;
    const auto insertCopiedClip = runtime.project().insertClips(
        commandContext(runtime), {{.trackId = trackId, .clip = copiedClipDraft}});
    const auto copiedClipId =
        insertCopiedClip
            ? Automation::ClipId(insertCopiedClip.get().affectedObjects.first().value)
            : Automation::ClipId();
    auto *copiedClip = dynamic_cast<SingingClip *>(model.findClipById(copiedClipId.value()));
    const auto copiedParameter = copiedClip
                                     ? runtime.parameters().getParameter(
                                           runtime.documentVersion().documentId, copiedClipId,
                                           ParamInfo::Pitch, Param::Edited)
                                     : Automation::AutomationResult<
                                           Automation::ParameterSnapshotDto>(
                                           Automation::AutomationError::notFound(
                                               {Automation::ObjectKind::Clip,
                                                copiedClipId.value()},
                                               QStringLiteral("Copied clip was not found")));
    ok &= expect(insertCopiedClip && copiedClip && copiedClip->notes().count() == 1 &&
                     copiedParameter && copiedParameter.get().curves.size() == 1 &&
                     copiedParameter.get().curves.first().values == curveDraft.values,
                 "clip value DTO copying must preserve notes and edited parameters");

    const auto undoCopiedClip = runtime.history().undo(commandContext(runtime));
    ok &= expect(undoCopiedClip && undoCopiedClip.get().changed &&
                     model.findClipById(copiedClipId.value()) == nullptr,
                 "a copied clip insertion must undo as one history entry");

    const auto selectOwnVoice = runtime.parameters().selectClipSingleSpeaker(
        commandContext(runtime), clipId, {}, {});
    ok &= expect(selectOwnVoice && selectOwnVoice.get().changed &&
                     !singingClip->usesTrackVoiceContext(),
                 "clip voice selection must switch from inherited to owned context");
    const auto useTrackVoice =
        runtime.parameters().useTrackVoiceContext(commandContext(runtime), clipId);
    ok &= expect(useTrackVoice && useTrackVoice.get().changed &&
                     singingClip->usesTrackVoiceContext(),
                 "clip voice context must switch back to track inheritance through the facade");

    const auto setLanguage = runtime.project().setSingingClipDefaultLanguage(
        commandContext(runtime), clipId, QStringLiteral("en"));
    const auto languageVersion = runtime.documentVersion();
    const auto languageNoOp = runtime.project().setSingingClipDefaultLanguage(
        commandContext(runtime), clipId, QStringLiteral("en"));
    ok &= expect(setLanguage && setLanguage.get().changed && singingClip->defaultLanguage() == "en" &&
                     languageNoOp && !languageNoOp.get().changed &&
                     runtime.documentVersion() == languageVersion,
                 "non-history document state must still advance revision exactly once");

    history->reset();

    return ok ? 0 : 1;
}
