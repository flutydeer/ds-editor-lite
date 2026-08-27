#include "RuntimeDimensionSupport.h"

#include <limits>

namespace RuntimeDimensions {
    namespace {
        using GuiResult = Automation::AutomationResult<Automation::GuiMutationResult>;

        struct ViewCase {
            Automation::OperationId operationId;
            std::function<GuiResult(RuntimeHarness &, const Automation::GuiCommandContext &)> valid;
            std::function<GuiResult(RuntimeHarness &, const Automation::GuiCommandContext &)>
                invalid;
            std::function<void(RuntimeHarness &)> prepare;
        };

        QList<ViewCase> viewCases() {
            auto restored = validViewState();
            restored.trackPanel.centerTick = 960.0;
            restored.trackPanel.centerTrackIndex = 3.0;
            restored.layout.bottomPanelPageId = QStringLiteral("歌词/Phoneme");
            restored.pianoRoll.centerTick = 1440.0;
            restored.pianoRoll.centerKeyIndex = 72.0;
            restored.pianoRoll.editMode = EditorViewGlobal::DrawNote;
            return {
                {
                 .operationId = Automation::OperationIds::editor::restore_view,
                 .valid =
                        [restored](RuntimeHarness &harness,                                     const auto &context) {
                            return harness.core().facade().restoreView(context, restored);
                        },                                            .invalid =
                        [](RuntimeHarness &harness,                    const auto &context) {
                            auto invalid = validViewState();
                            invalid.layout.trackPanelVisible = false;
                            invalid.layout.bottomPanelVisible = false;
                            return harness.core().facade().restoreView(context, invalid);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::center_track_panel,
                 .valid =
                        [](RuntimeHarness &harness,                                                                           const auto &context) {
                            return harness.core().facade().centerTrackPanel(context, 960.0, 3.0);
                        },                                            .invalid =
                        [](RuntimeHarness &harness,                                                                                                         const auto &context) {
                            return harness.core().facade().centerTrackPanel(context, -1.0, 0.0);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_track_panel_scale,
                 .valid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setTrackPanelScale(context, 2.0, 1.5);
                        },                               .invalid =
                        [](RuntimeHarness &harness,                                                         const auto &context) {
                            return harness.core().facade().setTrackPanelScale(context, 0.0, 1.0);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_track_panel_viewport,
                 .valid =
                        [](RuntimeHarness &harness,                   const auto &context) {
                            Automation::TrackPanelViewportPatch patch;
                            patch.centerTick = 960.0;
                            patch.verticalScale = 1.5;
                            return harness.core().facade().setTrackPanelViewport(context, patch);
                        }, .invalid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setTrackPanelViewport(context, {});
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_panel_visibility,
                 .valid =
                        [](RuntimeHarness &harness,     const auto &context) {
                            return harness.core().facade().setPanelVisibility(context, true, false);
                        },                                                                              .invalid =
                        [](RuntimeHarness &harness,                                                                               const auto &context) {
                            return harness.core().facade().setPanelVisibility(context, false,
                                                                              false);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::show_bottom_panel_page,
                 .valid =
                        [](RuntimeHarness &harness,                                                 const auto &context) {
                            return harness.core().facade().showBottomPanelPage(
                                context, QStringLiteral("歌词/Phoneme"));
                        },                                                                                 .invalid =
                        [](RuntimeHarness &harness,                                                                     const auto &context) {
                            return harness.core().facade().showBottomPanelPage(
                                context, QStringLiteral("   "));
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::show_region,
                 .valid =
                        [](RuntimeHarness &harness,  const auto &context) {
                            return harness.core().facade().showRegion(
                                context, EditorViewGlobal::Region::Parameters);
                        },        .invalid =
                        [](RuntimeHarness &harness,                                const auto &context) {
                            return harness.core().facade().showRegion(
                                context, EditorViewGlobal::Region::TrackPanel);
                        }, .prepare =
                        [](RuntimeHarness &harness) {
                            harness.editorView.layout.bottomPanelVisible = true;
                            harness.editorView.layout.bottomPanelPageId =
                                QStringLiteral("ClipEditor");
                            harness.editorView.layout.parametersVisible = false;
                            harness.editorView.layout.activeRegion =
                                EditorViewGlobal::Region::Parameters;
                            harness.editorView.layout.focusedRegion =
                                EditorViewGlobal::Region::Parameters;
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::focus_region,
                 .valid =
                        [](RuntimeHarness &harness,                                             const auto &context) {
                            return harness.core().facade().focusRegion(
                                context, EditorViewGlobal::Region::PianoRoll);
                        },                                                    .invalid =
                        [](RuntimeHarness &harness,                            const auto &context) {
                            return harness.core().facade().focusRegion(
                                context, EditorViewGlobal::Region::None);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::center_piano_roll,
                 .valid =
                        [](RuntimeHarness &harness,                                                                           const auto &context) {
                            return harness.core().facade().centerPianoRoll(context, 1440.0, 72.0);
                        },                                                   .invalid =
                        [](RuntimeHarness &harness,                                                                                                         const auto &context) {
                            return harness.core().facade().centerPianoRoll(context, 0.0, 128.0);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_piano_roll_scale,
                 .valid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setPianoRollScale(context, 1.5, 2.0);
                        },                               .invalid =
                        [](RuntimeHarness &harness,                                                         const auto &context) {
                            return harness.core().facade().setPianoRollScale(
                                context, std::numeric_limits<double>::infinity(), 1.0);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_clip_editor_time_viewport,
                 .valid =
                        [](RuntimeHarness &harness,                   const auto &context) {
                            Automation::ClipEditorTimeViewportPatch patch;
                            patch.centerTick = 1440.0;
                            return harness.core().facade().setClipEditorTimeViewport(context,
                                                                                     patch);
                        }, .invalid =
                        [](RuntimeHarness &harness, const auto &context) {
                            Automation::ClipEditorTimeViewportPatch patch;
                            patch.horizontalScale = 0.0;
                            return harness.core().facade().setClipEditorTimeViewport(context,
                                                                                     patch);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_piano_roll_pitch_viewport,
                 .valid =
                        [](RuntimeHarness &harness,            const auto &context) {
                            Automation::PianoRollPitchViewportPatch patch;
                            patch.centerKeyIndex = 72.0;
                            return harness.core().facade().setPianoRollPitchViewport(context,
                                                                                     patch);
                        },                                                                              .invalid =
                        [](RuntimeHarness &harness,                                                                               const auto &context) {
                            Automation::PianoRollPitchViewportPatch patch;
                            patch.centerKeyIndex = 128.0;
                            return harness.core().facade().setPianoRollPitchViewport(context,
                                                                                     patch);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_piano_roll_edit_mode,
                 .valid =
                        [](RuntimeHarness &harness,                                                 const auto &context) {
                            return harness.core().facade().setPianoRollEditMode(
                                context, EditorViewGlobal::DrawNote);
                        },                                                                                 .invalid =
                        [](RuntimeHarness &harness,                                                                     const auto &context) {
                            return harness.core().facade().setPianoRollEditMode(
                                context, static_cast<EditorViewGlobal::PianoRollEditMode>(99));
                        }, },
            };
        }

        void runViewCommands(ScenarioLog &log) {
            for (const auto &testCase : viewCases()) {
                const auto operationId = testCase.operationId;
                const auto prepare = [&testCase](RuntimeHarness &harness) {
                    if (testCase.prepare)
                        testCase.prepare(harness);
                };
                log.run(operationId, QStringLiteral("NORMAL"), [&] {
                    RuntimeHarness harness;
                    prepare(harness);
                    const auto result = testCase.valid(harness, guiContext(harness));
                    log.expect(result && result.get().changed && !result.get().validatedOnly &&
                                   result.get().windowId == harness.core().windowId() &&
                                   harness.hostCalls.value(operationId) == 1,
                               QStringLiteral("view mutation must apply exactly once"));
                });
                log.run(operationId, QStringLiteral("NO-OP"), [&] {
                    RuntimeHarness harness;
                    prepare(harness);
                    const auto first = testCase.valid(harness, guiContext(harness));
                    const auto second = testCase.valid(harness, guiContext(harness));
                    log.expect(first && first.get().changed && second && !second.get().changed &&
                                   harness.hostCalls.value(operationId) == 1,
                               QStringLiteral("repeated view mutation must be a host-free no-op"));
                });
                log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                    RuntimeHarness harness;
                    prepare(harness);
                    const auto before = harness.editorView;
                    const auto result = testCase.valid(harness, guiContext(harness, true));
                    log.expect(result && result.get().changed && result.get().validatedOnly &&
                                   harness.editorView == before &&
                                   harness.hostCalls.value(operationId) == 0,
                               QStringLiteral("view preview must not invoke its apply callback"));
                });
                log.run(operationId, QStringLiteral("INVALID-INPUT"), [&] {
                    RuntimeHarness harness;
                    prepare(harness);
                    const auto result = testCase.invalid(harness, guiContext(harness));
                    log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                    operationId,
                                    QStringLiteral("invalid view input must retain operation ID"));
                    log.expect(harness.hostCalls.value(operationId) == 0,
                               QStringLiteral("invalid view input must not reach the host"));
                });
                log.run(operationId, QStringLiteral("UNKNOWN-WINDOW"), [&] {
                    RuntimeHarness harness;
                    prepare(harness);
                    auto context = guiContext(harness);
                    context.windowId = Automation::WindowId::create();
                    const auto result = testCase.valid(harness, context);
                    log.expectError(
                        result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        operationId, QStringLiteral("view command must reject unknown window"));
                    log.expect(harness.hostCalls.value(operationId) == 0,
                               QStringLiteral("unknown window must not invoke view host"));
                });
                log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                    RuntimeHarness harness({.missingOperation = operationId});
                    prepare(harness);
                    const auto result = testCase.valid(harness, guiContext(harness));
                    log.expectError(
                        result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        operationId, QStringLiteral("missing view callback must be explicit"));
                });
                log.run(operationId, QStringLiteral("HOST-REJECT"), [&] {
                    RuntimeHarness harness;
                    prepare(harness);
                    const auto before = harness.editorView;
                    harness.editorApplySucceeds = false;
                    const auto result = testCase.valid(harness, guiContext(harness));
                    log.expectError(result,
                                    Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                    operationId,
                                    QStringLiteral("view host rejection must retain operation ID"));
                    log.expect(harness.editorView == before &&
                                   harness.hostCalls.value(operationId) == 1,
                               QStringLiteral("view host rejection must not partially mutate"));
                });
                log.run(operationId, QStringLiteral("DOCUMENT-NO-SIDE-EFFECT"), [&] {
                    RuntimeHarness harness;
                    prepare(harness);
                    const auto version = harness.core().documentVersion();
                    const auto result = testCase.valid(harness, guiContext(harness));
                    log.expect(result && harness.core().documentVersion() == version,
                               QStringLiteral("view command must not change document revision"));
                });
            }
        }

        void runCapabilities(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::editor::get_capabilities);
            log.run(operationId, QStringLiteral("LIMITS"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().getEditorCapabilities();
                log.expect(result && result.get().maxConcurrentDocuments == 1 &&
                               result.get().maxConcurrentWindows == 1,
                           QStringLiteral("capabilities must expose single session/window limits"));
            });
            log.run(operationId, QStringLiteral("OPERATION-SURFACE"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().getEditorCapabilities();
                log.expect(result &&
                               result.get().operationIds ==
                                   harness.core().catalog().operationIds() &&
                               result.get().operationIds.contains(
                                   Automation::OperationIds::editor::set_selection),
                           QStringLiteral("capability handler must return the runtime surface"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                const auto first = harness.core().facade().getEditorCapabilities();
                auto detached = first ? first.get().operationIds : QStringList{};
                detached.clear();
                const auto second = harness.core().facade().getEditorCapabilities();
                log.expect(first && second && !first.get().operationIds.isEmpty() &&
                               first.get().operationIds == second.get().operationIds &&
                               detached.isEmpty(),
                           QStringLiteral("capability list must be an owned value snapshot"));
            });
            log.run(operationId, QStringLiteral("DETERMINISTIC"), [&] {
                RuntimeHarness harness;
                const auto first = harness.core().facade().getEditorCapabilities();
                const auto second = harness.core().facade().getEditorCapabilities();
                log.expect(first && second && first.get().operationIds == second.get().operationIds,
                           QStringLiteral("capability order must be deterministic"));
            });
            log.run(operationId, QStringLiteral("NO-HOST-DEPENDENCY"), [&] {
                RuntimeHarness harness(
                    {.missingOperation = Automation::OperationIds::editor::get_state});
                const auto result = harness.core().facade().getEditorCapabilities();
                log.expect(
                    result && harness.hostCalls.isEmpty(),
                    QStringLiteral("capabilities must not depend on an attached editor host"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().facade().getEditorCapabilities();
                log.expect(result && harness.core().documentVersion() == version &&
                               harness.settingsWrites == 0 && harness.presetWrites == 0,
                           QStringLiteral("capability query must not mutate runtime state"));
            });
        }

        void runEditorState(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::editor::get_state);
            log.run(operationId, QStringLiteral("MINIMAL"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().getEditorState(
                    harness.core().documentVersion().documentId, harness.core().windowId());
                log.expect(result && result.get().document == harness.core().documentVersion() &&
                               result.get().windowId == harness.core().windowId() &&
                               result.get().view,
                           QStringLiteral("editor state must expose explicit document and window"));
            });
            log.run(operationId, QStringLiteral("RICH-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                if (objects) {
                    harness.editorStable.selectedTrackIndex = 0;
                    harness.editorStable.activeClipId = objects->clipId.value();
                    harness.editorStable.selectedClipIds = {objects->clipId.value()};
                    harness.editorStable.primaryClipId = objects->clipId.value();
                    harness.editorStable.selectedNoteIds = {objects->noteId.value()};
                    harness.editorStable.primaryNoteId = objects->noteId.value();
                    harness.editorStable.pianoRollQuantize = 24;
                    harness.editorStable.trackAutoPageTurnEnabled = false;
                }
                const auto result = harness.core().facade().getEditorState(
                    harness.core().documentVersion().documentId, harness.core().windowId());
                log.expect(objects && result &&
                               result.get().selection.selectedTrackId == objects->trackId &&
                               result.get().selection.activeClipId == objects->clipId &&
                               result.get().selection.selectedClipIds ==
                                   QList<Automation::ClipId>{objects->clipId} &&
                               result.get().selection.primaryClipId == objects->clipId &&
                               result.get().selection.selectedNoteIds ==
                                   QList<Automation::NoteId>{objects->noteId} &&
                               result.get().selection.primaryNoteId == objects->noteId &&
                               result.get().pianoRollQuantize == 24 &&
                               !result.get().trackAutoPageTurnEnabled,
                           QStringLiteral("editor state must preserve rich typed selection"));
            });
            log.run(operationId, QStringLiteral("DETACHED-SNAPSHOT"), [&] {
                RuntimeHarness harness;
                harness.editorView.pianoRoll.centerTick = 320.0;
                const auto result = harness.core().facade().getEditorState(
                    harness.core().documentVersion().documentId, harness.core().windowId());
                harness.editorView.pianoRoll.centerTick = 640.0;
                harness.editorStable.pianoRollQuantize = 48;
                log.expect(result && result.get().view &&
                               result.get().view->pianoRoll.centerTick == 320.0 &&
                               result.get().pianoRollQuantize == 16,
                           QStringLiteral("editor state must be detached from mutable host state"));
            });
            log.run(operationId, QStringLiteral("NO-SIDE-EFFECT"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().facade().getEditorState(
                    version.documentId, harness.core().windowId());
                log.expect(
                    result && harness.core().documentVersion() == version &&
                        harness.hostCalls.value(QStringLiteral("editor.capture_view")) == 1 &&
                        harness.hostCalls.value(QStringLiteral("editor.capture_stable")) == 1,
                    QStringLiteral("editor state query must only capture each host once"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().getEditorState(
                    Automation::DocumentId::create(), Automation::WindowId::create());
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("document must resolve before window and host"));
                log.expect(harness.hostCalls.isEmpty(),
                           QStringLiteral("unknown document must not capture editor state"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-WINDOW"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().getEditorState(
                    harness.core().documentVersion().documentId, Automation::WindowId::create());
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("editor state must reject unknown window"));
                log.expect(harness.hostCalls.isEmpty(),
                           QStringLiteral("unknown window must not capture editor state"));
            });
            log.run(operationId, QStringLiteral("OPTIONAL-VIEW"), [&] {
                RuntimeHarness harness;
                harness.editorViewAvailable = false;
                const auto result = harness.core().facade().getEditorState(
                    harness.core().documentVersion().documentId, harness.core().windowId());
                log.expect(
                    result && !result.get().view,
                    QStringLiteral("editor state must remain available without view snapshot"));
            });
            log.run(operationId, QStringLiteral("OPTIONAL-HOST"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().facade().getEditorState(
                    harness.core().documentVersion().documentId, harness.core().windowId());
                log.expect(
                    result && !result.get().view && !result.get().selection.selectedTrackId &&
                        result.get().pianoRollQuantize == 16,
                    QStringLiteral("editor state has stable defaults without GUI callbacks"));
            });
        }

        void runQuantize(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::editor::set_quantize);
            log.run(operationId, QStringLiteral("NORMAL"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result =
                    harness.core().facade().setPianoRollQuantize(guiContext(harness), 24, false);
                log.expect(result && result.get().changed &&
                               harness.editorStable.pianoRollQuantize == 24 &&
                               !harness.editorStable.pianoRollQuantizeEnabled &&
                               harness.hostCalls.value(operationId) == 1 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("quantize must apply stable GUI state without revision"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                const auto result =
                    harness.core().facade().setPianoRollQuantize(guiContext(harness), 16, true);
                log.expect(result && !result.get().changed &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("identical quantize must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().setPianoRollQuantize(
                    guiContext(harness, true), 24, false);
                log.expect(result && result.get().changed && result.get().validatedOnly &&
                               harness.editorStable.pianoRollQuantize == 16 &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("quantize preview must not apply host state"));
            });
            log.run(operationId, QStringLiteral("BOUNDARIES"), [&] {
                RuntimeHarness harness;
                const auto smallest =
                    harness.core().facade().setPianoRollQuantize(guiContext(harness), 1, true);
                const auto whole =
                    harness.core().facade().setPianoRollQuantize(guiContext(harness), 1920, true);
                log.expect(smallest && whole && harness.editorStable.pianoRollQuantize == 1920,
                           QStringLiteral("quantize must accept divisors at both boundaries"));
            });
            log.run(operationId, QStringLiteral("INVALID"), [&] {
                RuntimeHarness harness;
                const auto zero =
                    harness.core().facade().setPianoRollQuantize(guiContext(harness), 0, true);
                const auto nonDivisor =
                    harness.core().facade().setPianoRollQuantize(guiContext(harness), 7, true);
                log.expectError(zero, Automation::AutomationErrorCode::InvalidArgument, operationId,
                                QStringLiteral("zero quantize must retain operation ID"));
                log.expectError(nonDivisor, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("non-divisor quantize must retain operation ID"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-WINDOW"), [&] {
                RuntimeHarness harness;
                auto context = guiContext(harness);
                context.windowId = Automation::WindowId::create();
                const auto result =
                    harness.core().facade().setPianoRollQuantize(context, 24, false);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId, QStringLiteral("quantize must reject unknown window"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result =
                    harness.core().facade().setPianoRollQuantize(guiContext(harness), 24, false);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing quantize host must be explicit"));
            });
        }

        void runAutoPage(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::editor::set_auto_page_turn);
            log.run(operationId, QStringLiteral("TRACK-PANEL"), [&] {
                RuntimeHarness harness;
                const auto version = harness.core().documentVersion();
                const auto result = harness.core().facade().setAutoPageTurn(
                    guiContext(harness), Automation::EditorAutoPageTarget::TrackPanel, false);
                log.expect(result && result.get().changed &&
                               !harness.editorStable.trackAutoPageTurnEnabled &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("track auto-page must apply without document revision"));
            });
            log.run(operationId, QStringLiteral("PIANO-ROLL"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().setAutoPageTurn(
                    guiContext(harness), Automation::EditorAutoPageTarget::PianoRoll, false);
                log.expect(result && result.get().changed &&
                               !harness.editorStable.pianoRollAutoPageTurnEnabled,
                           QStringLiteral("piano-roll auto-page target must remain distinct"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().setAutoPageTurn(
                    guiContext(harness), Automation::EditorAutoPageTarget::TrackPanel, true);
                log.expect(result && !result.get().changed &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("identical auto-page value must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().setAutoPageTurn(
                    guiContext(harness, true), Automation::EditorAutoPageTarget::TrackPanel, false);
                log.expect(result && result.get().validatedOnly && result.get().changed &&
                               harness.editorStable.trackAutoPageTurnEnabled &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("auto-page preview must not apply state"));
            });
            log.run(operationId, QStringLiteral("INVALID-ENUM"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().setAutoPageTurn(
                    guiContext(harness), static_cast<Automation::EditorAutoPageTarget>(99), false);
                log.expectError(
                    result, Automation::AutomationErrorCode::InvalidArgument, operationId,
                    QStringLiteral("unknown auto-page target must retain operation ID"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-WINDOW"), [&] {
                RuntimeHarness harness;
                auto context = guiContext(harness);
                context.windowId = Automation::WindowId::create();
                const auto result = harness.core().facade().setAutoPageTurn(
                    context, Automation::EditorAutoPageTarget::TrackPanel, false);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("auto-page must reject unknown window"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto result = harness.core().facade().setAutoPageTurn(
                    guiContext(harness), Automation::EditorAutoPageTarget::TrackPanel, false);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing auto-page host must be explicit"));
            });
        }

        void runActiveClip(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::editor::set_active_clip);
            log.run(operationId, QStringLiteral("NORMAL"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                const auto version = harness.core().documentVersion();
                const auto result = objects ? harness.core().facade().setActiveClip(
                                                  guiDocumentContext(harness), objects->clipId)
                                            : GuiResult(Automation::AutomationError{});
                log.expect(objects && result && result.get().changed &&
                               harness.editorStable.activeClipId == objects->clipId.value() &&
                               harness.hostCalls.value(operationId) == 1 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("active clip must resolve and apply without revision"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                const auto first = objects ? harness.core().facade().setActiveClip(
                                                 guiDocumentContext(harness), objects->clipId)
                                           : GuiResult(Automation::AutomationError{});
                const auto second = objects ? harness.core().facade().setActiveClip(
                                                  guiDocumentContext(harness), objects->clipId)
                                            : GuiResult(Automation::AutomationError{});
                log.expect(objects && first && second && !second.get().changed &&
                               harness.hostCalls.value(operationId) == 1,
                           QStringLiteral("repeated active clip must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                const auto result = objects
                                        ? harness.core().facade().setActiveClip(
                                              guiDocumentContext(harness, true), objects->clipId)
                                        : GuiResult(Automation::AutomationError{});
                log.expect(objects && result && result.get().validatedOnly &&
                               result.get().changed && harness.editorStable.activeClipId == -1 &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("active clip preview must resolve without applying"));
            });
            log.run(operationId, QStringLiteral("CLEAR"), [&] {
                RuntimeHarness harness;
                harness.editorStable.activeClipId = 123;
                const auto result = harness.core().facade().setActiveClip(
                    guiDocumentContext(harness), std::nullopt);
                log.expect(result && result.get().changed &&
                               harness.editorStable.activeClipId == -1,
                           QStringLiteral("active clip must support explicit clearing"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                context.expected.revision += 100;
                context.windowId = Automation::WindowId::create();
                const auto result =
                    harness.core().facade().setActiveClip(context, Automation::ClipId(999999));
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("document must win over revision/window/object"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                ++context.expected.revision;
                context.windowId = Automation::WindowId::create();
                const auto result =
                    harness.core().facade().setActiveClip(context, Automation::ClipId(999999));
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId,
                                QStringLiteral("revision must win over window/object"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-WINDOW"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                context.windowId = Automation::WindowId::create();
                const auto result =
                    harness.core().facade().setActiveClip(context, Automation::ClipId(999999));
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("window must win over object resolution"));
            });
            log.run(operationId, QStringLiteral("MISSING-CLIP"), [&] {
                RuntimeHarness harness;
                const auto result = harness.core().facade().setActiveClip(
                    guiDocumentContext(harness), Automation::ClipId(999999));
                log.expectError(result, Automation::AutomationErrorCode::NotFound, operationId,
                                QStringLiteral("missing active clip must be typed not-found"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto objects = createEditorObjects(harness);
                const auto result = objects ? harness.core().facade().setActiveClip(
                                                  guiDocumentContext(harness), objects->clipId)
                                            : GuiResult(Automation::AutomationError{});
                log.expect(bool(objects), QStringLiteral("active clip fixture must be created"));
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing active clip host must be explicit"));
            });
        }

        void runSelection(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::editor::set_selection);
            log.run(operationId, QStringLiteral("TYPED-NORMALIZATION"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                Automation::NoteId secondNoteId;
                if (objects) {
                    Automation::NoteDraftDto additionalNote;
                    additionalNote.clientRef = QStringLiteral("runtime-dimension-primary-note");
                    additionalNote.localStart = 600;
                    additionalNote.length = 480;
                    additionalNote.keyIndex = 64;
                    additionalNote.lyric = QStringLiteral("主");
                    additionalNote.language = QStringLiteral("cmn");
                    const auto inserted = harness.core().notes().insertNotes(
                        commandContext(harness), objects->clipId, {additionalNote});
                    if (inserted && !inserted.get().createdObjects.isEmpty()) {
                        secondNoteId = Automation::NoteId(
                            inserted.get().createdObjects.constFirst().object.value);
                    }
                }
                const auto version = harness.core().documentVersion();
                const auto context = guiDocumentContext(harness);
                const auto track =
                    objects
                        ? harness.core().facade().setSelectedTrack(context, objects->trackId, true)
                        : GuiResult(Automation::AutomationError{});
                const auto clips = objects ? harness.core().facade().setSelectedClips(
                                                 context, {objects->clipId, objects->clipId},
                                                 objects->clipId, true)
                                           : GuiResult(Automation::AutomationError{});
                const auto notes = objects && secondNoteId.isValid()
                                       ? harness.core().facade().setSelectedNotes(
                                             context, objects->clipId,
                                             {objects->noteId, secondNoteId}, objects->noteId, true)
                                       : GuiResult(Automation::AutomationError{});
                const auto state = harness.core().facade().getEditorState(
                    harness.core().documentVersion().documentId, harness.core().windowId());
                log.expect(objects && secondNoteId.isValid() && track && clips && notes && state &&
                               harness.editorStable.selectedTrackIndex == 0 &&
                               harness.editorStable.selectedClipIds ==
                                   QList<int>{objects->clipId.value()} &&
                               harness.editorStable.selectedNoteIds ==
                                   QList<int>{objects->noteId.value(), secondNoteId.value()} &&
                               state.get().selection.selectedNoteIds ==
                                   QList<Automation::NoteId>{objects->noteId, secondNoteId} &&
                               state.get().selection.primaryNoteId == objects->noteId &&
                               harness.editorView.layout.focusedRegion ==
                                   EditorViewGlobal::Region::PianoRoll &&
                               harness.hostCalls.value(operationId) == 3 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral(
                               "selection must resolve types, deduplicate, and preserve revision"));
            });
            log.run(operationId, QStringLiteral("NO-OP"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                const auto first = objects ? harness.core().facade().setSelectedClips(
                                                 guiDocumentContext(harness), {objects->clipId})
                                           : GuiResult(Automation::AutomationError{});
                const auto second =
                    objects ? harness.core().facade().setSelectedClips(
                                  guiDocumentContext(harness), {objects->clipId, objects->clipId})
                            : GuiResult(Automation::AutomationError{});
                log.expect(objects && first && second && !second.get().changed &&
                               harness.hostCalls.value(operationId) == 1,
                           QStringLiteral("normalized identical selection must be a no-op"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                const auto result = objects
                                        ? harness.core().facade().setSelectedTrack(
                                              guiDocumentContext(harness, true), objects->trackId)
                                        : GuiResult(Automation::AutomationError{});
                log.expect(objects && result && result.get().validatedOnly &&
                               result.get().changed &&
                               harness.editorStable.selectedTrackIndex == -1 &&
                               harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("selection preview must resolve without applying"));
            });
            log.run(operationId, QStringLiteral("CLEAR"), [&] {
                RuntimeHarness harness;
                harness.editorStable.selectedClipIds = {1, 2};
                harness.editorStable.selectedTrackIndex = 0;
                const auto result = harness.core().facade().clearTrackPanelSelection(
                    guiDocumentContext(harness), true, true, true);
                log.expect(result && harness.editorStable.selectedClipIds.isEmpty() &&
                               harness.editorStable.selectedTrackIndex == -1 &&
                               harness.editorView.layout.focusedRegion ==
                                   EditorViewGlobal::Region::TrackPanel,
                           QStringLiteral(
                               "selection clear must atomically clear scopes and focus the panel"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                context.expected.revision += 10;
                context.windowId = Automation::WindowId::create();
                const auto result =
                    harness.core().facade().setSelectedClips(context, {Automation::ClipId(999999)});
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("selection must prioritize document identity"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                ++context.expected.revision;
                context.windowId = Automation::WindowId::create();
                const auto result =
                    harness.core().facade().setSelectedClips(context, {Automation::ClipId(999999)});
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId, QStringLiteral("selection must prioritize revision"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-WINDOW"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                context.windowId = Automation::WindowId::create();
                const auto result =
                    harness.core().facade().setSelectedClips(context, {Automation::ClipId(999999)});
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("selection must prioritize window before objects"));
            });
            log.run(operationId, QStringLiteral("MISSING-OBJECT"), [&] {
                RuntimeHarness harness;
                const auto missing = harness.core().facade().setSelectedClips(
                    guiDocumentContext(harness), {Automation::ClipId(999999)});
                const auto objects = createEditorObjects(harness);
                const auto invalidPrimary =
                    objects ? harness.core().facade().setSelectedClips(guiDocumentContext(harness),
                                                                       {objects->clipId},
                                                                       Automation::ClipId(999998))
                            : GuiResult(Automation::AutomationError{});
                log.expectError(missing, Automation::AutomationErrorCode::NotFound, operationId,
                                QStringLiteral("selection must report missing typed object"));
                log.expectError(invalidPrimary, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("primary selection must belong to ordered IDs"));
            });
            log.run(operationId, QStringLiteral("HOST-UNAVAILABLE"), [&] {
                RuntimeHarness harness({.missingOperation = operationId});
                const auto objects = createEditorObjects(harness);
                const auto result = objects ? harness.core().facade().setSelectedClips(
                                                  guiDocumentContext(harness), {objects->clipId})
                                            : GuiResult(Automation::AutomationError{});
                log.expect(bool(objects), QStringLiteral("selection fixture must be created"));
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing selection host must be explicit"));
            });
        }

        struct ParameterCase {
            Automation::OperationId operationId;
            std::function<GuiResult(RuntimeHarness &,
                                    const Automation::GuiDocumentCommandContext &)>
                valid;
            std::function<GuiResult(RuntimeHarness &,
                                    const Automation::GuiDocumentCommandContext &)>
                invalid;
            bool repeatedRequestIsNoOp = true;
        };

        QList<ParameterCase> parameterCases() {
            return {
                {
                 .operationId = Automation::OperationIds::editor::set_parameter_foreground,
                 .valid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setParameterForeground(
                                context, ParamInfo::Energy);
                        }, .invalid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setParameterForeground(context,
                                                                                  ParamInfo::Pitch);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_parameter_background,
                 .valid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setParameterBackground(
                                context, ParamInfo::Breathiness);
                        }, .invalid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setParameterBackground(
                                context, ParamInfo::SpeakerMix);
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::swap_parameters,
                 .valid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().swapParameters(context);
                        }, .invalid =
                        [](RuntimeHarness &harness, const auto &context) {
                            harness.editorView.parameters.background = ParamInfo::Unknown;
                            return harness.core().facade().swapParameters(context);
                        }, .repeatedRequestIsNoOp = false,
                 },
                {
                 .operationId = Automation::OperationIds::editor::set_parameter_edit_mode,
                 .valid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setParameterEditMode(
                                context, EditorViewGlobal::ParameterEditMode::Erase);
                        }, .invalid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setParameterEditMode(
                                context, static_cast<EditorViewGlobal::ParameterEditMode>(99));
                        }, },
                {
                 .operationId = Automation::OperationIds::editor::set_parameter_value_viewport,
                 .valid =
                        [](RuntimeHarness &harness, const auto &context) {
                            Automation::ParameterValueViewportPatch patch;
                            patch.centerRatio = 0.0;
                            patch.verticalScale = 4.0;
                            return harness.core().facade().setParameterValueViewport(context,
                                                                                     patch);
                        }, .invalid =
                        [](RuntimeHarness &harness, const auto &context) {
                            return harness.core().facade().setParameterValueViewport(context, {});
                        }, },
            };
        }

        void runParameterCommands(ScenarioLog &log) {
            for (const auto &testCase : parameterCases()) {
                const auto operationId = testCase.operationId;
                log.run(operationId, QStringLiteral("NORMAL-NO-SIDE-EFFECT"), [&] {
                    RuntimeHarness harness;
                    const auto objects = createEditorObjects(harness);
                    if (objects)
                        harness.editorStable.activeClipId = objects->clipId.value();
                    const auto version = harness.core().documentVersion();
                    const auto result = testCase.valid(harness, guiDocumentContext(harness));
                    log.expect(
                        objects && result && result.get().changed && !result.get().validatedOnly &&
                            harness.hostCalls.value(operationId) == 1 &&
                            harness.core().documentVersion() == version,
                        QStringLiteral("parameter GUI mutation must apply once without revision"));
                });
                log.run(operationId,
                        testCase.repeatedRequestIsNoOp ? QStringLiteral("NO-OP")
                                                       : QStringLiteral("REPEATED-REQUEST"),
                        [&] {
                            RuntimeHarness harness;
                            const auto objects = createEditorObjects(harness);
                            if (objects)
                                harness.editorStable.activeClipId = objects->clipId.value();
                            const auto first = testCase.valid(harness, guiDocumentContext(harness));
                            const auto second =
                                testCase.valid(harness, guiDocumentContext(harness));
                            const auto expectedCalls = testCase.repeatedRequestIsNoOp ? 1 : 2;
                            log.expect(objects && first && second &&
                                           second.get().changed != testCase.repeatedRequestIsNoOp &&
                                           harness.hostCalls.value(operationId) == expectedCalls,
                                       QStringLiteral(
                                           "parameter repeated-request semantics must be stable"));
                        });
                log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                    RuntimeHarness harness;
                    const auto objects = createEditorObjects(harness);
                    if (objects)
                        harness.editorStable.activeClipId = objects->clipId.value();
                    const auto before = harness.editorView;
                    const auto result = testCase.valid(harness, guiDocumentContext(harness, true));
                    log.expect(objects && result && result.get().changed &&
                                   result.get().validatedOnly && harness.editorView == before &&
                                   harness.hostCalls.value(operationId) == 0,
                               QStringLiteral("parameter preview must not call its GUI host"));
                });
                log.run(operationId, QStringLiteral("INVALID-INPUT"), [&] {
                    RuntimeHarness harness;
                    const auto result = testCase.invalid(harness, guiDocumentContext(harness));
                    log.expectError(result, Automation::AutomationErrorCode::InvalidArgument,
                                    operationId,
                                    QStringLiteral("invalid parameter GUI input must be rejected"));
                    log.expect(harness.hostCalls.value(operationId) == 0,
                               QStringLiteral("invalid parameter input must not reach the host"));
                });
                log.run(operationId, QStringLiteral("MISSING-CLIP"), [&] {
                    RuntimeHarness harness;
                    const auto result = testCase.valid(harness, guiDocumentContext(harness));
                    log.expectError(
                        result, Automation::AutomationErrorCode::InvalidArgument, operationId,
                        QStringLiteral("parameter command must require an active singing clip"));
                    log.expect(harness.hostCalls.value(operationId) == 0,
                               QStringLiteral("missing clip must not reach the parameter host"));
                });
                log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                    RuntimeHarness harness;
                    auto context = guiDocumentContext(harness);
                    ++context.expected.revision;
                    const auto result = testCase.valid(harness, context);
                    log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                    operationId,
                                    QStringLiteral("parameter GUI command must check revision"));
                });
                log.run(operationId, QStringLiteral("BUSY"), [&] {
                    RuntimeHarness harness;
                    const auto objects = createEditorObjects(harness);
                    if (objects)
                        harness.editorStable.activeClipId = objects->clipId.value();
                    harness.editorStable.parameterEditInProgress = true;
                    const auto before = harness.editorView;
                    const auto result = testCase.valid(harness, guiDocumentContext(harness));
                    log.expectError(
                        result, Automation::AutomationErrorCode::Busy, operationId,
                        QStringLiteral("parameter command must not interrupt an active edit"));
                    log.expect(
                        objects && harness.editorView == before &&
                            harness.hostCalls.value(operationId) == 0,
                        QStringLiteral("busy rejection must not partially mutate GUI state"));
                });
                log.run(operationId, QStringLiteral("HOST-FAILURES"), [&] {
                    RuntimeHarness missingHarness({.missingOperation = operationId});
                    const auto missingObjects = createEditorObjects(missingHarness);
                    if (missingObjects)
                        missingHarness.editorStable.activeClipId = missingObjects->clipId.value();
                    const auto missing =
                        testCase.valid(missingHarness, guiDocumentContext(missingHarness));

                    RuntimeHarness rejectedHarness;
                    const auto rejectedObjects = createEditorObjects(rejectedHarness);
                    if (rejectedObjects)
                        rejectedHarness.editorStable.activeClipId = rejectedObjects->clipId.value();
                    rejectedHarness.editorApplySucceeds = false;
                    const auto rejected =
                        testCase.valid(rejectedHarness, guiDocumentContext(rejectedHarness));
                    log.expect(missingObjects && rejectedObjects,
                               QStringLiteral("parameter fixtures must be created"));
                    log.expectError(
                        missing, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                        operationId, QStringLiteral("missing parameter GUI host must be explicit"));
                    log.expectError(rejected,
                                    Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                    operationId,
                                    QStringLiteral("rejected parameter GUI host must be explicit"));
                });
            }
        }

        Automation::EditorRevealDto noteReveal(const EditorObjects &objects) {
            return {
                .kind = Automation::EditorRevealKind::PianoRollNotes,
                .objectIds = {objects.noteId.value()},
                .containerId = objects.clipId.value(),
                .tickStart = 0.0,
                .tickEnd = 480.0,
                .valueStart = 60.0,
                .valueEnd = 60.0,
                .ticksAreLocal = true,
            };
        }

        void runReveal(ScenarioLog &log) {
            const auto operationId =
                Automation::OperationId(Automation::OperationIds::editor::reveal);
            log.run(operationId, QStringLiteral("PIANO-NOTES"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                const auto version = harness.core().documentVersion();
                const auto result =
                    objects ? harness.core().facade().reveal(guiDocumentContext(harness),
                                                             noteReveal(*objects), true)
                            : GuiResult(Automation::AutomationError{});
                log.expect(objects && result && result.get().changed &&
                               harness.hostCalls.value(operationId) == 1 &&
                               harness.core().documentVersion() == version,
                           QStringLiteral("note reveal must apply once without revision"));
            });
            log.run(operationId, QStringLiteral("TRACK-CLIPS-NO-OP-PRESERVES-SELECTION"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                Automation::EditorRevealDto target;
                if (objects) {
                    target.kind = Automation::EditorRevealKind::TrackClips;
                    target.objectIds = {objects->clipId.value()};
                    target.trackId = objects->trackId.value();
                    target.tickStart = 0.0;
                    target.tickEnd = 1920.0;
                }
                const auto first =
                    objects
                        ? harness.core().facade().reveal(guiDocumentContext(harness), target, false)
                        : GuiResult(Automation::AutomationError{});
                if (objects) {
                    harness.editorStable.selectedClipIds = {objects->clipId.value()};
                    harness.editorStable.primaryClipId = objects->clipId.value();
                    harness.editorFocusVisibility = HistoryFocusVisibility::Visible;
                }
                const auto before = harness.editorStable;
                const auto second =
                    objects ? harness.core().facade().reveal(guiDocumentContext(harness), target)
                            : GuiResult(Automation::AutomationError{});
                log.expect(
                    objects && first && first.get().changed && second && !second.get().changed &&
                        harness.editorStable == before && harness.hostCalls.value(operationId) == 2,
                    QStringLiteral("track reveal must resolve its target, then remain a no-op and "
                                   "preserve selection when fully visible"));
            });
            log.run(operationId, QStringLiteral("RANGE-FALLBACK"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                auto target = objects ? noteReveal(*objects) : Automation::EditorRevealDto{};
                target.objectIds = {999999};
                target.allowRangeFallback = true;
                const auto result =
                    objects ? harness.core().facade().reveal(guiDocumentContext(harness), target)
                            : GuiResult(Automation::AutomationError{});
                log.expect(objects && result && harness.hostCalls.value(operationId) == 1,
                           QStringLiteral("range fallback must tolerate deleted note IDs"));
            });
            log.run(operationId, QStringLiteral("VALIDATE-ONLY"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                const auto result =
                    objects ? harness.core().facade().reveal(guiDocumentContext(harness, true),
                                                             noteReveal(*objects))
                            : GuiResult(Automation::AutomationError{});
                log.expect(objects && result && result.get().validatedOnly &&
                               result.get().changed && harness.hostCalls.value(operationId) == 0,
                           QStringLiteral("reveal preview must resolve without calling host"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-DOCUMENT"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                context.expected.documentId = Automation::DocumentId::create();
                context.expected.revision += 10;
                context.windowId = Automation::WindowId::create();
                Automation::EditorRevealDto invalid;
                invalid.kind = static_cast<Automation::EditorRevealKind>(99);
                const auto result = harness.core().facade().reveal(context, invalid);
                log.expectError(result, Automation::AutomationErrorCode::DocumentChanged,
                                operationId,
                                QStringLiteral("reveal must prioritize document identity"));
            });
            log.run(operationId, QStringLiteral("STALE-REVISION"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                ++context.expected.revision;
                context.windowId = Automation::WindowId::create();
                Automation::EditorRevealDto invalid;
                invalid.kind = static_cast<Automation::EditorRevealKind>(99);
                const auto result = harness.core().facade().reveal(context, invalid);
                log.expectError(result, Automation::AutomationErrorCode::RevisionConflict,
                                operationId, QStringLiteral("reveal must prioritize revision"));
            });
            log.run(operationId, QStringLiteral("UNKNOWN-WINDOW"), [&] {
                RuntimeHarness harness;
                auto context = guiDocumentContext(harness);
                context.windowId = Automation::WindowId::create();
                Automation::EditorRevealDto invalid;
                invalid.kind = static_cast<Automation::EditorRevealKind>(99);
                const auto result = harness.core().facade().reveal(context, invalid);
                log.expectError(result, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId, QStringLiteral("reveal must prioritize window"));
            });
            log.run(operationId, QStringLiteral("INVALID-TARGET"), [&] {
                RuntimeHarness harness;
                const auto objects = createEditorObjects(harness);
                auto inverted = objects ? noteReveal(*objects) : Automation::EditorRevealDto{};
                inverted.tickStart = 10.0;
                inverted.tickEnd = 5.0;
                auto unsupported = inverted;
                unsupported.tickStart = 0.0;
                unsupported.tickEnd = 10.0;
                unsupported.kind = static_cast<Automation::EditorRevealKind>(99);
                const auto range =
                    harness.core().facade().reveal(guiDocumentContext(harness), inverted);
                const auto kind =
                    harness.core().facade().reveal(guiDocumentContext(harness), unsupported);
                log.expectError(range, Automation::AutomationErrorCode::InvalidArgument,
                                operationId,
                                QStringLiteral("inverted reveal range must be rejected"));
                log.expectError(kind, Automation::AutomationErrorCode::InvalidArgument, operationId,
                                QStringLiteral("unsupported reveal kind must be rejected"));
            });
            log.run(operationId, QStringLiteral("HOST-FAILURES"), [&] {
                RuntimeHarness missingHarness({.missingOperation = operationId});
                const auto missingObjects = createEditorObjects(missingHarness);
                const auto missing =
                    missingObjects
                        ? missingHarness.core().facade().reveal(guiDocumentContext(missingHarness),
                                                                noteReveal(*missingObjects))
                        : GuiResult(Automation::AutomationError{});
                RuntimeHarness rejectedHarness;
                const auto rejectedObjects = createEditorObjects(rejectedHarness);
                rejectedHarness.editorRevealSucceeds = false;
                const auto rejected = rejectedObjects ? rejectedHarness.core().facade().reveal(
                                                            guiDocumentContext(rejectedHarness),
                                                            noteReveal(*rejectedObjects))
                                                      : GuiResult(Automation::AutomationError{});
                log.expect(missingObjects && rejectedObjects,
                           QStringLiteral("reveal fixtures must be created"));
                log.expectError(missing, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                operationId,
                                QStringLiteral("missing reveal host must be explicit"));
                log.expectError(
                    rejected, Automation::AutomationErrorCode::HostCapabilityUnavailable,
                    operationId, QStringLiteral("rejected reveal host must be explicit"));
            });
        }
    }

    void runEditorDimensions(ScenarioLog &log) {
        runCapabilities(log);
        runEditorState(log);
        runViewCommands(log);
        runQuantize(log);
        runAutoPage(log);
        runActiveClip(log);
        runSelection(log);
        runParameterCommands(log);
        runReveal(log);
    }

} // namespace RuntimeDimensions
