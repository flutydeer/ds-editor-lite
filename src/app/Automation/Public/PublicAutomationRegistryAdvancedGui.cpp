#include "PublicAutomationRegistry.h"

#include "../CoreRuntime.h"

#include <lite/AutomationWire/PublicValueDomains.h>

#include <QJsonArray>

namespace Automation {

    namespace {

        namespace ToolNames = AutomationWire::PublicToolNames;

        DocumentId guiDocumentId(const QJsonObject &arguments) {
            return DocumentId::fromString(
                arguments.value(QStringLiteral("document_id")).toString());
        }

        WindowId guiWindowId(const QJsonObject &arguments) {
            return WindowId::fromString(arguments.value(QStringLiteral("window_id")).toString());
        }

        GuiCommandContext guiCommandContext(const QJsonObject &arguments,
                                            const PublicInvocationContext &invocation) {
            return {
                .windowId = guiWindowId(arguments),
                .validateOnly = arguments.value(QStringLiteral("validate_only")).toBool(false),
                .source = invocation.source,
                .clientId = invocation.clientId,
            };
        }

        GuiDocumentCommandContext
            guiDocumentCommandContext(const QJsonObject &arguments,
                                      const PublicInvocationContext &invocation) {
            return {
                .documentId = guiDocumentId(arguments),
                .windowId = guiWindowId(arguments),
                .validateOnly = arguments.value(QStringLiteral("validate_only")).toBool(false),
                .source = invocation.source,
                .clientId = invocation.clientId,
            };
        }

        QJsonObject encodeGuiMutation(const GuiMutationResult &result) {
            return {
                {QStringLiteral("window_id"),      result.windowId.toString()},
                {QStringLiteral("changed"),        result.changed            },
                {QStringLiteral("validated_only"), result.validatedOnly      },
            };
        }

        AutomationResult<QJsonObject>
            guiMutationResult(AutomationResult<GuiMutationResult> result) {
            if (!result)
                return result.getError();
            return encodeGuiMutation(result.get());
        }

        QJsonValue nullableId(const std::optional<TrackId> &id) {
            return id ? QJsonValue(id->value()) : QJsonValue(QJsonValue::Null);
        }

        QJsonValue nullableId(const std::optional<ClipId> &id) {
            return id ? QJsonValue(id->value()) : QJsonValue(QJsonValue::Null);
        }

        QJsonValue nullableId(const std::optional<NoteId> &id) {
            return id ? QJsonValue(id->value()) : QJsonValue(QJsonValue::Null);
        }

        template <typename Id>
        QJsonArray encodeIds(const QList<Id> &ids) {
            QJsonArray result;
            for (const auto id : ids)
                result.append(id.value());
            return result;
        }

        template <typename Id>
        QList<Id> decodeIds(const QJsonArray &values) {
            QList<Id> result;
            result.reserve(values.size());
            for (const auto &value : values)
                result.append(Id(value.toInt()));
            return result;
        }

        QString regionName(const EditorViewGlobal::Region region) {
            switch (region) {
                case EditorViewGlobal::Region::TrackPanel:
                    return QStringLiteral("track_panel");
                case EditorViewGlobal::Region::PianoRoll:
                    return QStringLiteral("piano");
                case EditorViewGlobal::Region::Parameters:
                    return QStringLiteral("parameters");
                case EditorViewGlobal::Region::None:
                    return QStringLiteral("none");
            }
            return QStringLiteral("none");
        }

        QString activePanelName(const EditorViewGlobal::Region region) {
            if (region == EditorViewGlobal::Region::TrackPanel)
                return QStringLiteral("track_panel");
            if (region == EditorViewGlobal::Region::PianoRoll ||
                region == EditorViewGlobal::Region::Parameters) {
                return QStringLiteral("clip_editor");
            }
            return QStringLiteral("none");
        }

        QString pianoEditModeName(const EditorViewGlobal::PianoRollEditMode mode) {
            static const QStringList names{
                QStringLiteral("select"),
                QStringLiteral("interval_select"),
                QStringLiteral("draw_note"),
                QStringLiteral("erase_note"),
                QStringLiteral("split_note"),
                QStringLiteral("draw_pitch"),
                QStringLiteral("edit_pitch_anchor"),
                QStringLiteral("erase_pitch"),
                QStringLiteral("bake_pitch"),
            };
            const auto index = static_cast<qsizetype>(mode);
            return index >= 0 && index < names.size() ? names.at(index) : QStringLiteral("select");
        }

        EditorViewGlobal::PianoRollEditMode pianoEditMode(const QString &name) {
            static const QStringList names{
                QStringLiteral("select"),
                QStringLiteral("interval_select"),
                QStringLiteral("draw_note"),
                QStringLiteral("erase_note"),
                QStringLiteral("split_note"),
                QStringLiteral("draw_pitch"),
                QStringLiteral("edit_pitch_anchor"),
                QStringLiteral("erase_pitch"),
                QStringLiteral("bake_pitch"),
            };
            const auto index = names.indexOf(name);
            return index < 0 ? EditorViewGlobal::Select
                             : static_cast<EditorViewGlobal::PianoRollEditMode>(index);
        }

        QString parameterEditModeName(const EditorViewGlobal::ParameterEditMode mode) {
            switch (mode) {
                case EditorViewGlobal::ParameterEditMode::Draw:
                    return QStringLiteral("draw");
                case EditorViewGlobal::ParameterEditMode::Erase:
                    return QStringLiteral("erase");
                case EditorViewGlobal::ParameterEditMode::Bake:
                    return QStringLiteral("bake");
                case EditorViewGlobal::ParameterEditMode::Anchor:
                    return QStringLiteral("anchor");
            }
            return QStringLiteral("draw");
        }

        EditorViewGlobal::ParameterEditMode parameterEditMode(const QString &name) {
            if (name == QStringLiteral("erase"))
                return EditorViewGlobal::ParameterEditMode::Erase;
            if (name == QStringLiteral("bake"))
                return EditorViewGlobal::ParameterEditMode::Bake;
            if (name == QStringLiteral("anchor"))
                return EditorViewGlobal::ParameterEditMode::Anchor;
            return EditorViewGlobal::ParameterEditMode::Draw;
        }

        ParamInfo::Name parameterName(const QString &name) {
            const auto names = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterName);
            const auto index = names.indexOf(name);
            return index < 0 || index >= static_cast<qsizetype>(ParamInfo::Unknown)
                       ? ParamInfo::Unknown
                       : static_cast<ParamInfo::Name>(index);
        }

        QJsonValue parameterName(const ParamInfo::Name name) {
            if (name == ParamInfo::Unknown)
                return QJsonValue(QJsonValue::Null);
            const auto names = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterName);
            const auto index = static_cast<qsizetype>(name);
            return index >= 0 && index < names.size() ? QJsonValue(names.at(index))
                                                      : QJsonValue(QJsonValue::Null);
        }

        QJsonObject encodeWorkspace(const EditorStateDto &state) {
            const auto &layout = state.view->layout;
            return {
                {QStringLiteral("window_id"),           state.windowId.toString()           },
                {QStringLiteral("track_panel_visible"), layout.trackPanelVisible            },
                {QStringLiteral("clip_editor_visible"), layout.bottomPanelVisible           },
                {QStringLiteral("active_panel"),        activePanelName(layout.activeRegion)},
                {QStringLiteral("focused_region"),      regionName(layout.focusedRegion)    },
            };
        }

        QJsonObject encodeTrackPanel(const EditorStateDto &state) {
            const auto &view = state.view->trackPanel;
            return {
                {QStringLiteral("window_id"),         state.windowId.toString()                  },
                {QStringLiteral("document_id"),       state.document.documentId.toString()       },
                {QStringLiteral("viewport"),
                 QJsonObject{
                     {QStringLiteral("center_tick"), view.centerTick},
                     {QStringLiteral("center_track_index"), view.centerTrackIndex},
                     {QStringLiteral("horizontal_scale"), view.horizontalScale},
                     {QStringLiteral("vertical_scale"), view.verticalScale},
                 }                                                                               },
                {QStringLiteral("auto_page_turn"),    state.trackAutoPageTurnEnabled             },
                {QStringLiteral("selected_track_id"), nullableId(state.selection.selectedTrackId)},
                {QStringLiteral("selected_clip_ids"), encodeIds(state.selection.selectedClipIds) },
                {QStringLiteral("primary_clip_id"),   nullableId(state.selection.primaryClipId)  },
                {QStringLiteral("focused"),
                 state.view->layout.focusedRegion == EditorViewGlobal::Region::TrackPanel        },
            };
        }

        QJsonObject encodeClipEditor(const EditorStateDto &state) {
            const auto &view = *state.view;
            const auto &piano = view.pianoRoll;
            const auto &parameters = view.parameters;
            const auto focused = view.layout.focusedRegion;
            const auto active = view.layout.activeRegion;
            return {
                {QStringLiteral("window_id"),      state.windowId.toString()               },
                {QStringLiteral("document_id"),    state.document.documentId.toString()    },
                {QStringLiteral("visible"),        view.layout.bottomPanelVisible          },
                {QStringLiteral("active_clip_id"), nullableId(state.selection.activeClipId)},
                {QStringLiteral("active_region"),
                 active == EditorViewGlobal::Region::PianoRoll ||
                         active == EditorViewGlobal::Region::Parameters
                     ? regionName(active)
                     : QStringLiteral("none")                                              },
                {QStringLiteral("focused_region"),
                 focused == EditorViewGlobal::Region::PianoRoll ||
                         focused == EditorViewGlobal::Region::Parameters
                     ? regionName(focused)
                     : QStringLiteral("none")                                              },
                {QStringLiteral("time_viewport"),
                 QJsonObject{
                     {QStringLiteral("center_tick"), piano.centerTick},
                     {QStringLiteral("horizontal_scale"), piano.horizontalScale},
                 }                                                                         },
                {QStringLiteral("auto_page_turn"), state.pianoRollAutoPageTurnEnabled      },
                {QStringLiteral("piano"),
                 QJsonObject{
                     {QStringLiteral("visible"), view.layout.pianoRollVisible},
                     {QStringLiteral("focused"), focused == EditorViewGlobal::Region::PianoRoll},
                     {QStringLiteral("pitch_viewport"),
                      QJsonObject{
                          {QStringLiteral("center_key_index"), piano.centerKeyIndex},
                          {QStringLiteral("vertical_scale"), piano.verticalScale},
                      }},
                     {QStringLiteral("edit_mode"), pianoEditModeName(piano.editMode)},
                     {QStringLiteral("quantize"), state.pianoRollQuantize},
                     {QStringLiteral("quantize_enabled"), state.pianoRollQuantizeEnabled},
                     {QStringLiteral("selected_note_ids"),
                      encodeIds(state.selection.selectedNoteIds)},
                     {QStringLiteral("primary_note_id"), nullableId(state.selection.primaryNoteId)},
                 }                                                                         },
                {QStringLiteral("parameters"),
                 QJsonObject{
                     {QStringLiteral("visible"), view.layout.parametersVisible},
                     {QStringLiteral("focused"), focused == EditorViewGlobal::Region::Parameters},
                     {QStringLiteral("foreground"), parameterName(parameters.foreground)},
                     {QStringLiteral("background"), parameterName(parameters.background)},
                     {QStringLiteral("tool"), parameterEditModeName(parameters.editMode)},
                     {QStringLiteral("value_viewport"),
                      QJsonObject{
                          {QStringLiteral("center_ratio"), parameters.centerRatio},
                          {QStringLiteral("vertical_scale"), parameters.verticalScale},
                      }},
                 }                                                                         },
            };
        }

        AutomationResult<EditorStateDto> editorState(CoreRuntime &runtime,
                                                     const QJsonObject &arguments,
                                                     const bool workspace = false) {
            const auto document =
                workspace ? runtime.documentVersion().documentId : guiDocumentId(arguments);
            auto state = runtime.facade().getEditorState(document, guiWindowId(arguments));
            if (!state)
                return state.getError();
            if (!state.get().view) {
                AutomationError error;
                error.code = AutomationErrorCode::HostCapabilityUnavailable;
                error.message = QStringLiteral("Editor view state is unavailable");
                return error;
            }
            return state;
        }

    } // namespace

    void PublicAutomationRegistry::registerAdvancedGuiBindings() {
        addBinding(ToolNames::workspace_get_state,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto state = editorState(m_runtime, arguments, true);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       return AutomationResult<QJsonObject>(encodeWorkspace(state.get()));
                   });
        addBinding(
            ToolNames::workspace_set_panel_visibility,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                auto state = editorState(m_runtime, arguments, true);
                if (!state)
                    return AutomationResult<QJsonObject>(state.getError());
                const auto trackVisible =
                    arguments.contains(QStringLiteral("track_panel_visible"))
                        ? arguments.value(QStringLiteral("track_panel_visible")).toBool()
                        : state.get().view->layout.trackPanelVisible;
                const auto clipEditorVisible =
                    arguments.contains(QStringLiteral("clip_editor_visible"))
                        ? arguments.value(QStringLiteral("clip_editor_visible")).toBool()
                        : state.get().view->layout.bottomPanelVisible;
                return guiMutationResult(m_runtime.facade().setPanelVisibility(
                    guiCommandContext(arguments, invocation), trackVisible, clipEditorVisible));
            });

        addBinding(ToolNames::track_panel_get_state,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       return AutomationResult<QJsonObject>(encodeTrackPanel(state.get()));
                   });
        addBinding(ToolNames::track_panel_set_viewport,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       TrackPanelViewportPatch patch;
                       if (arguments.contains(QStringLiteral("center_tick")))
                           patch.centerTick =
                               arguments.value(QStringLiteral("center_tick")).toDouble();
                       if (arguments.contains(QStringLiteral("center_track_index"))) {
                           patch.centerTrackIndex =
                               arguments.value(QStringLiteral("center_track_index")).toDouble();
                       }
                       if (arguments.contains(QStringLiteral("horizontal_scale"))) {
                           patch.horizontalScale =
                               arguments.value(QStringLiteral("horizontal_scale")).toDouble();
                       }
                       if (arguments.contains(QStringLiteral("vertical_scale"))) {
                           patch.verticalScale =
                               arguments.value(QStringLiteral("vertical_scale")).toDouble();
                       }
                       return guiMutationResult(m_runtime.facade().setTrackPanelViewport(
                           guiCommandContext(arguments, invocation), patch));
                   });
        addBinding(ToolNames::track_panel_reveal_clips,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       EditorRevealDto target;
                       target.kind = EditorRevealKind::TrackClips;
                       if (arguments.contains(QStringLiteral("track_id")))
                           target.trackId = arguments.value(QStringLiteral("track_id")).toInt();
                       else {
                           const auto clipIds = decodeIds<ClipId>(
                               arguments.value(QStringLiteral("clip_ids")).toArray());
                           target.objectIds.reserve(clipIds.size());
                           for (const auto clipId : clipIds)
                               target.objectIds.append(clipId.value());
                       }
                       return guiMutationResult(m_runtime.facade().reveal(
                           guiDocumentCommandContext(arguments, invocation), target));
                   });
        addBinding(ToolNames::track_panel_set_auto_page_turn,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       return guiMutationResult(m_runtime.facade().setAutoPageTurn(
                           guiCommandContext(arguments, invocation),
                           EditorAutoPageTarget::TrackPanel,
                           arguments.value(QStringLiteral("enabled")).toBool()));
                   });
        addBinding(ToolNames::track_panel_select_track,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       const auto value = arguments.value(QStringLiteral("track_id"));
                       const auto id = value.isNull()
                                           ? std::optional<TrackId>{}
                                           : std::optional<TrackId>(TrackId(value.toInt()));
                       return guiMutationResult(m_runtime.facade().setSelectedTrack(
                           guiDocumentCommandContext(arguments, invocation), id, true));
                   });
        addBinding(ToolNames::track_panel_select_clips,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       const auto primary = arguments.value(QStringLiteral("primary_clip_id"));
                       return guiMutationResult(m_runtime.facade().setSelectedClips(
                           guiDocumentCommandContext(arguments, invocation),
                           decodeIds<ClipId>(arguments.value(QStringLiteral("clip_ids")).toArray()),
                           primary.isUndefined() || primary.isNull()
                               ? std::optional<ClipId>{}
                               : std::optional<ClipId>(ClipId(primary.toInt())),
                           true));
                   });
        addBinding(ToolNames::track_panel_clear_selection,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       const auto target = arguments.value(QStringLiteral("target")).toString();
                       return guiMutationResult(m_runtime.facade().clearTrackPanelSelection(
                           guiDocumentCommandContext(arguments, invocation),
                           target == QStringLiteral("track") || target == QStringLiteral("all"),
                           target == QStringLiteral("clips") || target == QStringLiteral("all"),
                           true));
                   });

        addBinding(ToolNames::clip_editor_get_state,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       return AutomationResult<QJsonObject>(encodeClipEditor(state.get()));
                   });
        addBinding(ToolNames::clip_editor_set_active_clip,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       const auto value = arguments.value(QStringLiteral("clip_id"));
                       const auto id = value.isNull()
                                           ? std::optional<ClipId>{}
                                           : std::optional<ClipId>(ClipId(value.toInt()));
                       return guiMutationResult(m_runtime.facade().setActiveClip(
                           guiDocumentCommandContext(arguments, invocation), id));
                   });
        addBinding(ToolNames::clip_editor_set_time_viewport,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       ClipEditorTimeViewportPatch patch;
                       if (arguments.contains(QStringLiteral("center_tick")))
                           patch.centerTick =
                               arguments.value(QStringLiteral("center_tick")).toDouble();
                       if (arguments.contains(QStringLiteral("horizontal_scale"))) {
                           patch.horizontalScale =
                               arguments.value(QStringLiteral("horizontal_scale")).toDouble();
                       }
                       return guiMutationResult(m_runtime.facade().setClipEditorTimeViewport(
                           guiCommandContext(arguments, invocation), patch));
                   });
        addBinding(ToolNames::clip_editor_set_auto_page_turn,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       return guiMutationResult(m_runtime.facade().setAutoPageTurn(
                           guiCommandContext(arguments, invocation),
                           EditorAutoPageTarget::PianoRoll,
                           arguments.value(QStringLiteral("enabled")).toBool()));
                   });
        addBinding(ToolNames::clip_editor_show_region,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       const auto region = arguments.value(QStringLiteral("region")).toString() ==
                                                   QStringLiteral("parameters")
                                               ? EditorViewGlobal::Region::Parameters
                                               : EditorViewGlobal::Region::PianoRoll;
                       return guiMutationResult(m_runtime.facade().showRegion(
                           guiCommandContext(arguments, invocation), region));
                   });
        addBinding(ToolNames::clip_editor_piano_set_pitch_viewport,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       PianoRollPitchViewportPatch patch;
                       if (arguments.contains(QStringLiteral("center_key_index"))) {
                           patch.centerKeyIndex =
                               arguments.value(QStringLiteral("center_key_index")).toDouble();
                       }
                       if (arguments.contains(QStringLiteral("vertical_scale"))) {
                           patch.verticalScale =
                               arguments.value(QStringLiteral("vertical_scale")).toDouble();
                       }
                       return guiMutationResult(m_runtime.facade().setPianoRollPitchViewport(
                           guiCommandContext(arguments, invocation), patch));
                   });
        addBinding(ToolNames::clip_editor_piano_reveal_notes,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       if (!state.get().selection.activeClipId) {
                           return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                               QStringLiteral("active_clip_id"),
                               QStringLiteral("An active singing clip is required")));
                       }
                       EditorRevealDto target;
                       target.kind = EditorRevealKind::PianoRollNotes;
                       target.containerId = state.get().selection.activeClipId->value();
                       for (const auto id : decodeIds<NoteId>(
                                arguments.value(QStringLiteral("note_ids")).toArray())) {
                           target.objectIds.append(id.value());
                       }
                       return guiMutationResult(m_runtime.facade().reveal(
                           guiDocumentCommandContext(arguments, invocation), target));
                   });
        addBinding(ToolNames::clip_editor_piano_set_edit_mode,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       return guiMutationResult(m_runtime.facade().setPianoRollEditMode(
                           guiCommandContext(arguments, invocation),
                           pianoEditMode(arguments.value(QStringLiteral("mode")).toString())));
                   });
        addBinding(ToolNames::clip_editor_piano_set_quantize,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       const auto quantize =
                           arguments.contains(QStringLiteral("quantize"))
                               ? arguments.value(QStringLiteral("quantize")).toInt()
                               : state.get().pianoRollQuantize;
                       const auto enabled =
                           arguments.contains(QStringLiteral("enabled"))
                               ? arguments.value(QStringLiteral("enabled")).toBool()
                               : state.get().pianoRollQuantizeEnabled;
                       return guiMutationResult(m_runtime.facade().setPianoRollQuantize(
                           guiCommandContext(arguments, invocation), quantize, enabled));
                   });
        addBinding(ToolNames::clip_editor_piano_select_notes,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       if (!state.get().selection.activeClipId) {
                           return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                               QStringLiteral("active_clip_id"),
                               QStringLiteral("An active singing clip is required")));
                       }
                       const auto primary = arguments.value(QStringLiteral("primary_note_id"));
                       return guiMutationResult(m_runtime.facade().setSelectedNotes(
                           guiDocumentCommandContext(arguments, invocation),
                           *state.get().selection.activeClipId,
                           decodeIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           primary.isUndefined() || primary.isNull()
                               ? std::optional<NoteId>{}
                               : std::optional<NoteId>(NoteId(primary.toInt())),
                           true));
                   });
        addBinding(ToolNames::clip_editor_piano_clear_selection,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto state = editorState(m_runtime, arguments);
                       if (!state)
                           return AutomationResult<QJsonObject>(state.getError());
                       if (!state.get().selection.activeClipId) {
                           return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                               QStringLiteral("active_clip_id"),
                               QStringLiteral("An active singing clip is required")));
                       }
                       return guiMutationResult(m_runtime.facade().setSelectedNotes(
                           guiDocumentCommandContext(arguments, invocation),
                           *state.get().selection.activeClipId, {}, std::nullopt, true));
                   });
        addBinding(ToolNames::clip_editor_parameters_set_foreground,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return guiMutationResult(m_runtime.facade().setParameterForeground(
                           guiDocumentCommandContext(arguments, invocation),
                           parameterName(arguments.value(QStringLiteral("parameter")).toString())));
                   });
        addBinding(
            ToolNames::clip_editor_parameters_set_background,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto parameter = arguments.value(QStringLiteral("parameter"));
                return guiMutationResult(m_runtime.facade().setParameterBackground(
                    guiDocumentCommandContext(arguments, invocation),
                    parameter.isNull() ? ParamInfo::Unknown : parameterName(parameter.toString())));
            });
        addBinding(ToolNames::clip_editor_parameters_swap,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return guiMutationResult(m_runtime.facade().swapParameters(
                           guiDocumentCommandContext(arguments, invocation)));
                   });
        addBinding(ToolNames::clip_editor_parameters_set_tool,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return guiMutationResult(m_runtime.facade().setParameterEditMode(
                           guiDocumentCommandContext(arguments, invocation),
                           parameterEditMode(arguments.value(QStringLiteral("tool")).toString())));
                   });
        addBinding(ToolNames::clip_editor_parameters_set_value_viewport,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       ParameterValueViewportPatch patch;
                       if (arguments.contains(QStringLiteral("center_ratio"))) {
                           patch.centerRatio =
                               arguments.value(QStringLiteral("center_ratio")).toDouble();
                       }
                       if (arguments.contains(QStringLiteral("vertical_scale"))) {
                           patch.verticalScale =
                               arguments.value(QStringLiteral("vertical_scale")).toDouble();
                       }
                       return guiMutationResult(m_runtime.facade().setParameterValueViewport(
                           guiDocumentCommandContext(arguments, invocation), patch));
                   });
    }

} // namespace Automation
