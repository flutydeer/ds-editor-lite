#include "tst_application_services.h"
#include "ApplicationHarness.h"

#include <QtTest>
#include <limits>

using namespace ApplicationTest;

namespace {
    struct EditorObjects {
        Automation::TrackId trackId;
        Automation::ClipId clipId;
        Automation::NoteId noteId;
    };

    std::optional<EditorObjects> createEditorObjects(ApplicationHarness &harness) {
        Automation::TrackDraftDto track;
        track.clientRef = QStringLiteral("runtime-track");
        track.name = QStringLiteral("Runtime Track");
        track.gain = 1.0;
        track.defaultLanguage = QStringLiteral("unknown");

        Automation::ClipDraftDto clip;
        clip.clientRef = QStringLiteral("runtime-clip");
        clip.type = Automation::ClipDraftDto::Type::Singing;
        clip.properties.name = QStringLiteral("Runtime Clip");
        clip.properties.start = 0;
        clip.properties.length = 1920;
        clip.properties.clipStart = 0;
        clip.properties.clipLen = 1920;
        clip.properties.gain = 1.0;
        clip.defaultLanguage = QStringLiteral("unknown");
        clip.notes.append({
            .clientRef = QStringLiteral("runtime-note"),
            .localStart = 0,
            .length = 480,
            .keyIndex = 60,
            .lyric = QStringLiteral("la"),
            .language = QStringLiteral("unknown"),
        });
        track.clips.append(clip);

        const auto inserted =
            harness.core().project().insertTrack(commandContext(harness.core()), 0, track);
        if (!inserted)
            return std::nullopt;

        EditorObjects objects;
        for (const auto &created : inserted.get().createdObjects) {
            switch (created.object.kind) {
                case Automation::ObjectKind::Track:
                    objects.trackId = Automation::TrackId(created.object.value);
                    break;
                case Automation::ObjectKind::Clip:
                    objects.clipId = Automation::ClipId(created.object.value);
                    break;
                case Automation::ObjectKind::Note:
                    objects.noteId = Automation::NoteId(created.object.value);
                    break;
                default:
                    break;
            }
        }
        if (!objects.trackId.isValid() || !objects.clipId.isValid() || !objects.noteId.isValid())
            return std::nullopt;
        harness.resetHistory();
        return objects;
    }

    using GuiResult = Automation::AutomationResult<Automation::GuiMutationResult>;

    struct ViewCommandCase {
        QString name;
        Automation::OperationId operationId;
        std::function<GuiResult(ApplicationHarness &, const Automation::GuiCommandContext &)>
            invoke;
        std::function<GuiResult(ApplicationHarness &, const Automation::GuiCommandContext &)>
            invalid;
    };

    QList<ViewCommandCase> viewCommandCases() {
        EditorViewState restored;
        restored.trackPanel.centerTick = 240.0;
        restored.trackPanel.centerTrackIndex = 1.0;
        restored.trackPanel.horizontalScale = 2.0;
        restored.trackPanel.verticalScale = 1.5;
        restored.layout.bottomPanelPageId = QStringLiteral("Lyrics");
        restored.pianoRoll.centerTick = 480.0;
        restored.pianoRoll.centerKeyIndex = 72.0;
        restored.pianoRoll.horizontalScale = 1.25;
        restored.pianoRoll.verticalScale = 1.75;
        restored.pianoRoll.editMode = EditorViewGlobal::DrawNote;

        return {
            {
             .name = QStringLiteral("restore-view"),
             .operationId = Automation::OperationIds::editor::restore_view,
             .invoke =
                    [restored](ApplicationHarness &harness,                                                                  const auto &context) {
                        return harness.core().facade().restoreView(context, restored);
                    },                                                                   .invalid =
                    [](ApplicationHarness &harness,                   const auto &context) {
                        EditorViewState invalid;
                        invalid.layout.trackPanelVisible = false;
                        invalid.layout.bottomPanelVisible = false;
                        return harness.core().facade().restoreView(context, invalid);
                    }, },
            {
             .name = QStringLiteral("center-track"),
             .operationId = Automation::OperationIds::editor::center_track_panel,
             .invoke =
                    [](ApplicationHarness &harness,                                                                         const auto &context) {
                        return harness.core().facade().centerTrackPanel(context, 240.0, 2.0);
                    },                                                                   .invalid =
                    [](ApplicationHarness &harness,                                                                                                    const auto &context) {
                        return harness.core().facade().centerTrackPanel(context, -1.0, 0.0);
                    }, },
            {
             .name = QStringLiteral("scale-track"),
             .operationId = Automation::OperationIds::editor::set_track_panel_scale,
             .invoke =
                    [](ApplicationHarness &harness,                                   const auto &context) {
                        return harness.core().facade().setTrackPanelScale(context, 2.0, 1.5);
                    },                                                         .invalid =
                    [](ApplicationHarness &harness,                                                                                                    const auto &context) {
                        return harness.core().facade().setTrackPanelScale(context, 0.0, 1.0);
                    }, },
            {
             .name = QStringLiteral("panel-visibility"),
             .operationId = Automation::OperationIds::editor::set_panel_visibility,
             .invoke =
                    [](ApplicationHarness &harness, const auto &context) {
                        return harness.core().facade().setPanelVisibility(context, true, false);
                    }, .invalid =
                    [](ApplicationHarness &harness, const auto &context) {
                        return harness.core().facade().setPanelVisibility(context, false, false);
                    }, },
            {
             .name = QStringLiteral("bottom-page"),
             .operationId = Automation::OperationIds::editor::show_bottom_panel_page,
             .invoke =
                    [](ApplicationHarness &harness,                                                                          const auto &context) {
                        return harness.core().facade().showBottomPanelPage(
                            context, QStringLiteral("Lyrics"));
                    },                                                                           .invalid =
                    [](ApplicationHarness &harness,                           const auto &context) {
                        return harness.core().facade().showBottomPanelPage(context,
                                                                           QStringLiteral(" "));
                    }, },
            {
             .name = QStringLiteral("center-piano"),
             .operationId = Automation::OperationIds::editor::center_piano_roll,
             .invoke =
                    [](ApplicationHarness &harness,                                                                          const auto &context) {
                        return harness.core().facade().centerPianoRoll(context, 480.0, 72.0);
                    },                                                               .invalid =
                    [](ApplicationHarness &harness,                                                                                                    const auto &context) {
                        return harness.core().facade().centerPianoRoll(context, 0.0, 128.0);
                    }, },
            {
             .name = QStringLiteral("scale-piano"),
             .operationId = Automation::OperationIds::editor::set_piano_roll_scale,
             .invoke =
                    [](ApplicationHarness &harness,                                   const auto &context) {
                        return harness.core().facade().setPianoRollScale(context, 1.25, 1.75);
                    },                                                         .invalid =
                    [](ApplicationHarness &harness,                                                                                                   const auto &context) {
                        return harness.core().facade().setPianoRollScale(
                            context, std::numeric_limits<double>::quiet_NaN(), 1.0);
                    }, },
            {
             .name = QStringLiteral("edit-mode"),
             .operationId = Automation::OperationIds::editor::set_piano_roll_edit_mode,
             .invoke =
                    [](ApplicationHarness &harness, const auto &context) {
                        return harness.core().facade().setPianoRollEditMode(
                            context, EditorViewGlobal::ModulatePitch);
                    }, .invalid =
                    [](ApplicationHarness &harness, const auto &context) {
                        return harness.core().facade().setPianoRollEditMode(
                            context, static_cast<EditorViewGlobal::PianoRollEditMode>(99));
                    }, },
        };
    }

}

void ApplicationServicesTests::editorViewCommands_data() {
    QTest::addColumn<int>("caseIndex");
    const auto cases = viewCommandCases();
    for (int index = 0; index < cases.size(); ++index)
        QTest::newRow(qPrintable(cases.at(index).name)) << index;
}

void ApplicationServicesTests::editorViewCommands() {
    QFETCH(int, caseIndex);
    const auto testCase = viewCommandCases().at(caseIndex);

    ApplicationHarness harness;
    auto &runtime = harness.core();

    const auto preview = testCase.invoke(harness, guiContext(runtime, true));
    const auto committed = testCase.invoke(harness, guiContext(runtime));
    const auto noOp = testCase.invoke(harness, guiContext(runtime));
    QVERIFY2(
        (preview && preview.get().changed && preview.get().validatedOnly && committed &&
         committed.get().changed && noOp && !noOp.get().changed && harness.editorApplyCalls == 1),
        qPrintable(QStringLiteral("view command must preview, commit once, and detect no-op")));

    auto unknownWindow = guiContext(runtime);
    unknownWindow.windowId = Automation::WindowId::create();
    const auto unknown = testCase.invoke(harness, unknownWindow);
    QVERIFY2(
        (!unknown &&
         unknown.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         unknown.getError().operationId == testCase.operationId),
        qPrintable(QStringLiteral("view command must reject an unknown window")));
    QVERIFY2((harness.editorApplyCalls == 1),
             qPrintable(QStringLiteral("unknown window must not reach the view host")));

    const auto invalid = testCase.invalid(harness, guiContext(runtime));
    QVERIFY2(
        (!invalid && invalid.getError().code == Automation::AutomationErrorCode::InvalidArgument),
        qPrintable(QStringLiteral("view command must reject its invalid input")));

    harness.editorView = EditorViewState{};
    harness.editorApplySucceeds = false;
    const auto rejected = testCase.invoke(harness, guiContext(runtime));
    QVERIFY2(
        (!rejected &&
         rejected.getError().code == Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         rejected.getError().operationId == testCase.operationId),
        qPrintable(QStringLiteral("view host rejection must remain a stable error")));
    QVERIFY2((harness.editorView == EditorViewState{}),
             qPrintable(QStringLiteral("view host rejection must not partially mutate state")));
}

void ApplicationServicesTests::unavailableEditorView() {

    ApplicationHarness harness;
    auto &runtime = harness.core();
    harness.editorViewAvailable = false;
    const auto unavailable = runtime.facade().centerPianoRoll(guiContext(runtime), 120.0, 60.0);
    QVERIFY2(
        (!unavailable &&
         unavailable.getError().code ==
             Automation::AutomationErrorCode::HostCapabilityUnavailable &&
         unavailable.getError().operationId == Automation::OperationIds::editor::center_piano_roll),
        qPrintable(QStringLiteral("missing captured view must reject view mutation")));
}

void ApplicationServicesTests::editorPreferencesPreserveDocument() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto version = runtime.documentVersion();
    const auto quantizePreview =
        runtime.facade().setPianoRollQuantize(guiContext(runtime, true), 24, false);
    const auto quantize = runtime.facade().setPianoRollQuantize(guiContext(runtime), 24, false);
    const auto quantizeNoOp = runtime.facade().setPianoRollQuantize(guiContext(runtime), 24, false);
    const auto invalidQuantize =
        runtime.facade().setPianoRollQuantize(guiContext(runtime), 7, true);
    QVERIFY2(
        (quantizePreview && quantizePreview.get().validatedOnly && quantize &&
         quantize.get().changed && quantizeNoOp && !quantizeNoOp.get().changed &&
         harness.editorStableApplyCalls == 1 && runtime.documentVersion() == version),
        qPrintable(QStringLiteral("quantize must preview, commit, no-op and preserve revision")));
    QVERIFY2((!invalidQuantize &&
              invalidQuantize.getError().code == Automation::AutomationErrorCode::InvalidArgument),
             qPrintable(QStringLiteral("quantize must divide whole-note ticks")));

    const auto autoPagePreview = runtime.facade().setAutoPageTurn(
        guiContext(runtime, true), Automation::EditorAutoPageTarget::TrackPanel, false);
    const auto autoPage = runtime.facade().setAutoPageTurn(
        guiContext(runtime), Automation::EditorAutoPageTarget::TrackPanel, false);
    const auto autoPageNoOp = runtime.facade().setAutoPageTurn(
        guiContext(runtime), Automation::EditorAutoPageTarget::TrackPanel, false);
    const auto invalidTarget = runtime.facade().setAutoPageTurn(
        guiContext(runtime), static_cast<Automation::EditorAutoPageTarget>(99), false);
    QVERIFY2((autoPagePreview && autoPagePreview.get().validatedOnly && autoPage &&
              autoPage.get().changed && autoPageNoOp && !autoPageNoOp.get().changed &&
              harness.editorStableApplyCalls == 2),
             qPrintable(QStringLiteral("auto-page must preview, commit once and detect no-op")));
    QVERIFY2((!invalidTarget &&
              invalidTarget.getError().code == Automation::AutomationErrorCode::InvalidArgument),
             qPrintable(QStringLiteral("unknown auto-page target must be rejected")));
}

void ApplicationServicesTests::editorSelectionAndSnapshots() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto objects = createEditorObjects(harness);
    QVERIFY(objects);
    const auto version = runtime.documentVersion();
    harness.editorStable.pianoRollQuantize = 24;
    harness.editorStable.trackAutoPageTurnEnabled = false;
    auto context = guiDocumentContext(runtime);
    const auto selectTrack = runtime.facade().setSelectedTrack(context, objects->trackId);
    const auto selectClip = runtime.facade().setActiveClip(context, objects->clipId);
    const auto selectClips =
        runtime.facade().setSelectedClips(context, {objects->clipId, objects->clipId});
    const auto selectNotes = runtime.facade().setSelectedNotes(context, objects->clipId,
                                                               {objects->noteId, objects->noteId});
    const auto selectNotesNoOp = runtime.facade().setSelectedNotes(
        context, objects->clipId, {objects->noteId, objects->noteId});
    QVERIFY2(
        (selectTrack && selectClip && selectClips && selectNotes && selectNotesNoOp &&
         !selectNotesNoOp.get().changed && harness.editorStable.selectedTrackIndex == 0 &&
         harness.editorStable.activeClipId == objects->clipId.value() &&
         harness.editorStable.selectedClipIds == QList<int>{objects->clipId.value()} &&
         harness.editorStable.selectedNoteIds == QList<int>{objects->noteId.value()} &&
         runtime.documentVersion() == version),
        qPrintable(QStringLiteral("selection must normalize IDs and not mutate the document")));

    harness.editorView.pianoRoll.centerTick = 640.0;
    runtime.setDocumentBusy(version.documentId, true);
    const auto state = runtime.facade().getEditorState(version.documentId, *runtime.windowId());
    QVERIFY2(
        (state && state.get().document == version && state.get().windowId == *runtime.windowId() &&
         state.get().documentBusy && state.get().view &&
         state.get().view->pianoRoll.centerTick == 640.0 &&
         state.get().selection.selectedTrackId == objects->trackId &&
         state.get().selection.activeClipId == objects->clipId &&
         state.get().selection.selectedClipIds == QList<Automation::ClipId>{objects->clipId} &&
         state.get().selection.selectedNoteIds == QList<Automation::NoteId>{objects->noteId} &&
         state.get().pianoRollQuantize == 24 && !state.get().trackAutoPageTurnEnabled),
        qPrintable(QStringLiteral("editor state must combine session, view and stable selection")));
    runtime.setDocumentBusy(version.documentId, false);

    harness.editorViewAvailable = false;
    const auto noViewState =
        runtime.facade().getEditorState(version.documentId, *runtime.windowId());
    QVERIFY2((noViewState && !noViewState.get().view),
             qPrintable(
                 QStringLiteral("state query must remain available when no view snapshot exists")));
    harness.editorViewAvailable = true;

    const auto wrongDocument = runtime.facade().getEditorState(Automation::DocumentId::create(),
                                                               Automation::WindowId::create());
    const auto wrongWindow =
        runtime.facade().getEditorState(version.documentId, Automation::WindowId::create());
    QVERIFY2((!wrongDocument &&
              wrongDocument.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
              wrongDocument.getError().operationId == Automation::OperationIds::editor::get_state),
             qPrintable(QStringLiteral("document must be resolved before window for state query")));
    QVERIFY2((!wrongWindow &&
              wrongWindow.getError().code ==
                  Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              wrongWindow.getError().operationId == Automation::OperationIds::editor::get_state),
             qPrintable(QStringLiteral("state query must reject unknown window")));
}

void ApplicationServicesTests::editorSelectionRoutingErrors() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto context = guiDocumentContext(runtime);
    auto wrongDocumentContext = context;
    wrongDocumentContext.documentId = Automation::DocumentId::create();
    wrongDocumentContext.expectedRevision = runtime.documentVersion().revision + 100;
    wrongDocumentContext.windowId = Automation::WindowId::create();
    const auto wrongDocumentSelection =
        runtime.facade().setActiveClip(wrongDocumentContext, Automation::ClipId(999999));
    auto staleContext = context;
    ++*staleContext.expectedRevision;
    staleContext.windowId = Automation::WindowId::create();
    const auto staleSelection =
        runtime.facade().setActiveClip(staleContext, Automation::ClipId(999999));
    auto wrongWindowContext = context;
    wrongWindowContext.windowId = Automation::WindowId::create();
    const auto wrongWindowSelection =
        runtime.facade().setActiveClip(wrongWindowContext, Automation::ClipId(999999));
    const auto missingClip = runtime.facade().setActiveClip(context, Automation::ClipId(999999));
    QVERIFY2((!wrongDocumentSelection &&
              wrongDocumentSelection.getError().code ==
                  Automation::AutomationErrorCode::DocumentChanged &&
              wrongDocumentSelection.getError().operationId ==
                  Automation::OperationIds::editor::set_active_clip),
             qPrintable(QStringLiteral("document must win over revision, window and object")));
    QVERIFY2((!staleSelection &&
              staleSelection.getError().code == Automation::AutomationErrorCode::RevisionConflict &&
              staleSelection.getError().operationId ==
                  Automation::OperationIds::editor::set_active_clip),
             qPrintable(QStringLiteral("revision must win over window and object")));
    QVERIFY2((!wrongWindowSelection &&
              wrongWindowSelection.getError().code ==
                  Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              wrongWindowSelection.getError().operationId ==
                  Automation::OperationIds::editor::set_active_clip),
             qPrintable(QStringLiteral("window must win over object resolution")));
    QVERIFY2(
        (!missingClip && missingClip.getError().code == Automation::AutomationErrorCode::NotFound &&
         missingClip.getError().operationId == Automation::OperationIds::editor::set_active_clip),
        qPrintable(QStringLiteral("valid routing must reach object resolution")));
}

void ApplicationServicesTests::editorRevealAndRangeFallback() {
    ApplicationHarness harness;
    auto &runtime = harness.core();
    const auto objects = createEditorObjects(harness);
    QVERIFY(objects);
    const auto version = runtime.documentVersion();
    const auto context = guiDocumentContext(runtime);
    Automation::EditorRevealDto target{
        .kind = Automation::EditorRevealKind::PianoRollNotes,
        .objectIds = {objects->noteId.value()},
        .containerId = objects->clipId.value(),
        .tickStart = 0.0,
        .tickEnd = 480.0,
        .valueStart = 60.0,
        .valueEnd = 60.0,
        .ticksAreLocal = true,
    };
    const auto revealPreview =
        runtime.facade().reveal(guiDocumentContext(runtime, true), target, false);
    const auto reveal = runtime.facade().reveal(context, target, true);
    QVERIFY2(
        (revealPreview && revealPreview.get().validatedOnly && reveal && reveal.get().changed &&
         harness.revealCalls == 1 && runtime.documentVersion() == version),
        qPrintable(QStringLiteral("reveal must validate without host action and then apply once")));

    target.objectIds = {999999};
    target.allowRangeFallback = true;
    const auto fallback = runtime.facade().reveal(context, target);
    QVERIFY2((fallback && harness.revealCalls == 2),
             qPrintable(QStringLiteral("range fallback must tolerate a deleted note ID")));

    target.allowRangeFallback = false;
    const auto missingNote = runtime.facade().reveal(context, target);
    target.objectIds = {objects->noteId.value()};
    target.tickStart = 10.0;
    target.tickEnd = 5.0;
    const auto invalidRange = runtime.facade().reveal(context, target);
    target.tickStart = 0.0;
    target.tickEnd = 480.0;
    harness.editorRevealSucceeds = false;
    const auto hostRejected = runtime.facade().reveal(context, target);
    QVERIFY2((!missingNote &&
              missingNote.getError().code == Automation::AutomationErrorCode::NotFound &&
              missingNote.getError().operationId == Automation::OperationIds::editor::reveal),
             qPrintable(QStringLiteral("reveal must reject a missing note without fallback")));
    QVERIFY2((!invalidRange &&
              invalidRange.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
              invalidRange.getError().operationId == Automation::OperationIds::editor::reveal),
             qPrintable(QStringLiteral("reveal must reject an inverted range")));
    QVERIFY2((!hostRejected &&
              hostRejected.getError().code ==
                  Automation::AutomationErrorCode::HostCapabilityUnavailable &&
              hostRejected.getError().operationId == Automation::OperationIds::editor::reveal),
             qPrintable(QStringLiteral("reveal host rejection must be stable")));
}
