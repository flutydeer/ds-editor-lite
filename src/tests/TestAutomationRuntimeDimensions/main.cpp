#include "RuntimeDimensionSupport.h"

#include <QCoreApplication>

namespace RuntimeDimensions {

    RuntimeDimensionMatrix expectedRuntimeDimensions() {
        const auto query = CoverageDimensions(NormalData | SnapshotNoSideEffect | HostUnavailable |
                                              BoundaryOrUnicode);
        const auto applicationCommand =
            CoverageDimensions(NormalData | ValidateOnlyOrNoOp | UnknownTargetOrRevision |
                               HostUnavailable | PersistenceOrSingleHost | IdempotencyOrRepeat);
        const auto ephemeralDocumentCommand =
            CoverageDimensions(NormalData | ValidateOnlyOrNoOp | UnknownTargetOrRevision |
                               HostUnavailable | IdempotencyOrRepeat);
        const auto boundedEphemeralDocumentCommand =
            CoverageDimensions(ephemeralDocumentCommand | BoundaryOrUnicode);
        const auto persistentDocumentCommand =
            CoverageDimensions(boundedEphemeralDocumentCommand | PersistenceOrSingleHost);
        const auto viewCommand = CoverageDimensions(
            NormalData | SnapshotNoSideEffect | ValidateOnlyOrNoOp | UnknownTargetOrRevision |
            HostUnavailable | BoundaryOrUnicode | PersistenceOrSingleHost);
        const auto settingCommand =
            CoverageDimensions(NormalData | ValidateOnlyOrNoOp | HostUnavailable |
                               BoundaryOrUnicode | PersistenceOrSingleHost | IdempotencyOrRepeat);
        const auto storageCommand =
            CoverageDimensions(NormalData | ValidateOnlyOrNoOp | HostUnavailable |
                               BoundaryOrUnicode | PersistenceOrSingleHost);

        return {
            {Automation::OperationIds::application::get_info,                 query                                                             },
            {Automation::OperationIds::application::request_exit,             applicationCommand                                                },
            {Automation::OperationIds::application::request_restart,          applicationCommand                                                },
            {Automation::OperationIds::editor::center_piano_roll,             viewCommand                                                       },
            {Automation::OperationIds::editor::center_track_panel,            viewCommand                                                       },
            {Automation::OperationIds::editor::focus_region,                  viewCommand                                                       },
            {Automation::OperationIds::editor::get_capabilities,
             NormalData | SnapshotNoSideEffect | BoundaryOrUnicode                                                                              },
            {Automation::OperationIds::editor::get_state,
             NormalData | SnapshotNoSideEffect | UnknownTargetOrRevision | HostUnavailable                                                      },
            {Automation::OperationIds::editor::restore_view,                  viewCommand                                                       },
            {Automation::OperationIds::editor::reveal,
             NormalData | ValidateOnlyOrNoOp | UnknownTargetOrRevision | HostUnavailable |
                 BoundaryOrUnicode | PersistenceOrSingleHost                                                                                    },
            {Automation::OperationIds::editor::set_active_clip,
             NormalData | ValidateOnlyOrNoOp | UnknownTargetOrRevision | HostUnavailable |
                 BoundaryOrUnicode                                                                                                              },
            {Automation::OperationIds::editor::set_clip_editor_time_viewport, viewCommand                                                       },
            {Automation::OperationIds::editor::set_auto_page_turn,
             NormalData | ValidateOnlyOrNoOp | UnknownTargetOrRevision | HostUnavailable |
                 BoundaryOrUnicode                                                                                                              },
            {Automation::OperationIds::editor::set_panel_visibility,          viewCommand                                                       },
            {Automation::OperationIds::editor::set_parameter_background,      viewCommand                                                       },
            {Automation::OperationIds::editor::set_parameter_edit_mode,       viewCommand                                                       },
            {Automation::OperationIds::editor::set_parameter_foreground,      viewCommand                                                       },
            {Automation::OperationIds::editor::set_parameter_value_viewport,  viewCommand                                                       },
            {Automation::OperationIds::editor::set_piano_roll_edit_mode,      viewCommand                                                       },
            {Automation::OperationIds::editor::set_piano_roll_pitch_viewport, viewCommand                                                       },
            {Automation::OperationIds::editor::set_piano_roll_scale,          viewCommand                                                       },
            {Automation::OperationIds::editor::set_quantize,
             NormalData | ValidateOnlyOrNoOp | UnknownTargetOrRevision | HostUnavailable |
                 BoundaryOrUnicode                                                                                                              },
            {Automation::OperationIds::editor::set_selection,
             NormalData | ValidateOnlyOrNoOp | UnknownTargetOrRevision | HostUnavailable |
                 BoundaryOrUnicode                                                                                                              },
            {Automation::OperationIds::editor::set_track_panel_scale,         viewCommand                                                       },
            {Automation::OperationIds::editor::set_track_panel_viewport,      viewCommand                                                       },
            {Automation::OperationIds::editor::show_bottom_panel_page,        viewCommand                                                       },
            {Automation::OperationIds::editor::show_region,                   viewCommand                                                       },
            {Automation::OperationIds::editor::swap_parameters,               viewCommand                                                       },
            {Automation::OperationIds::playback::clear_loop,                  ephemeralDocumentCommand                                          },
            {Automation::OperationIds::playback::get,
             NormalData | SnapshotNoSideEffect | UnknownTargetOrRevision | HostUnavailable                                                      },
            {Automation::OperationIds::playback::pause,                       ephemeralDocumentCommand                                          },
            {Automation::OperationIds::playback::play,                        persistentDocumentCommand                                         },
            {Automation::OperationIds::playback::set_last_position,
             boundedEphemeralDocumentCommand                                                                                                    },
            {Automation::OperationIds::playback::set_loop,                    persistentDocumentCommand                                         },
            {Automation::OperationIds::playback::set_loop_enabled,            persistentDocumentCommand                                         },
            {Automation::OperationIds::playback::set_position,                boundedEphemeralDocumentCommand                                   },
            {Automation::OperationIds::playback::stop,                        ephemeralDocumentCommand                                          },
            {Automation::OperationIds::packages::get_search_paths,            query                                                             },
            {Automation::OperationIds::packages::list,                        query                                                             },
            {Automation::OperationIds::packages::resolve_document_voices,
             NormalData | ValidateOnlyOrNoOp | UnknownTargetOrRevision | HostUnavailable |
                 PersistenceOrSingleHost | IdempotencyOrRepeat                                                                                  },
            {Automation::OperationIds::packages::set_search_paths,            settingCommand                                                    },
            {Automation::OperationIds::packages::validate,                    NormalData | SnapshotNoSideEffect |
                                                               HostUnavailable | BoundaryOrUnicode |
                                                               PersistenceOrSingleHost},
            {Automation::OperationIds::recent_files::add,                     storageCommand                                                    },
            {Automation::OperationIds::recent_files::clear,                   storageCommand | IdempotencyOrRepeat                              },
            {Automation::OperationIds::recent_files::list,                    query                                                             },
            {Automation::OperationIds::recent_files::remove,                  storageCommand | IdempotencyOrRepeat                              },
            {Automation::OperationIds::settings::query,                         query                                                             },
            {Automation::OperationIds::settings::update_appearance,           settingCommand                                                    },
            {Automation::OperationIds::settings::update_audio,                settingCommand                                                    },
            {Automation::OperationIds::settings::update_developer,            settingCommand                                                    },
            {Automation::OperationIds::settings::update_fill_lyric,           settingCommand                                                    },
            {Automation::OperationIds::settings::update_g2p_language,         settingCommand                                                    },
            {Automation::OperationIds::settings::update_general,              settingCommand                                                    },
            {Automation::OperationIds::settings::update_inference,            settingCommand                                                    },
            {Automation::OperationIds::settings::update_window,               settingCommand                                                    },
            {Automation::OperationIds::speaker_mix_presets::delete_preset,
             storageCommand | IdempotencyOrRepeat                                                                                               },
            {Automation::OperationIds::speaker_mix_presets::list,             query                                                             },
            {Automation::OperationIds::speaker_mix_presets::save,             storageCommand                                                    },
        };
    }

} // namespace RuntimeDimensions

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    RuntimeDimensions::ScenarioLog log;
    RuntimeDimensions::runApplicationPlaybackDimensions(log);
    RuntimeDimensions::runEditorDimensions(log);
    RuntimeDimensions::runSettingsPackageDimensions(log);
    return log.finish(RuntimeDimensions::expectedRuntimeDimensions());
}
