// symbol, MCP tool name, category, minimum profile, operation kind, sync mode,
// minimum toolset version, effect, repeatability, world access
AUTOMATION_WIRE_PUBLIC_TOOL(application_get_info, "application.get_info", "application", L0,
                            Query, Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(application_get_status, "application.get_status", "application", L0,
                            Query, Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(application_request_exit, "application.request_exit", "application", L0,
                            Command, Synchronous, 1, Destructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(application_request_restart, "application.request_restart", "application",
                            L0, Command, Synchronous, 1, Destructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(application_get_file_access, "application.get_file_access",
                            "application", L2, Query, Synchronous, 1, ReadOnly, Idempotent,
                            ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(documents_get, "documents.get", "documents", L1, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(documents_list_recent, "documents.list_recent", "documents", L2, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(documents_new, "documents.new", "documents", L2, Command, Synchronous,
                            1, Destructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(documents_open, "documents.open", "documents", L2, Command,
                            Asynchronous, 1, Destructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(documents_save, "documents.save", "documents", L2, Command, Synchronous,
                            1, Destructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(documents_save_as, "documents.save_as", "documents", L2, Command,
                            Synchronous, 1, Destructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(documents_import, "documents.import", "documents", L2, Command,
                            Asynchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(documents_import_batch, "documents.import_batch", "documents", L2,
                            Command, Asynchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(formats_list, "formats.list", "formats", L2, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(formats_inspect, "formats.inspect", "formats", L2, Query, Synchronous,
                            1, ReadOnly, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(tracks_list, "tracks.list", "tracks", L1, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_get, "tracks.get", "tracks", L1, Query, Synchronous, 1, ReadOnly,
                            Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_insert, "tracks.insert", "tracks", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_remove, "tracks.remove", "tracks", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_move, "tracks.move", "tracks", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_rename, "tracks.rename", "tracks", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_set_color, "tracks.set_color", "tracks", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_set_gain, "tracks.set_gain", "tracks", L1, Command, Synchronous,
                            1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_set_pan, "tracks.set_pan", "tracks", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_set_mute, "tracks.set_mute", "tracks", L1, Command, Synchronous,
                            1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_set_solo, "tracks.set_solo", "tracks", L1, Command, Synchronous,
                            1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_set_default_language, "tracks.set_default_language", "tracks",
                            L1, Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_set_voice, "tracks.set_voice", "tracks", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tracks_clear_voice, "tracks.clear_voice", "tracks", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(master_get, "master.get", "bus", L1, Query, Synchronous, 1, ReadOnly,
                            Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(master_set_gain, "master.set_gain", "bus", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(master_set_pan, "master.set_pan", "bus", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(master_set_mute, "master.set_mute", "bus", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(master_set_solo, "master.set_solo", "bus", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(clips_list, "clips.list", "clips", L1, Query, Synchronous, 1, ReadOnly,
                            Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_get, "clips.get", "clips", L1, Query, Synchronous, 1, ReadOnly,
                            Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_insert, "clips.insert", "clips", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_duplicate, "clips.duplicate", "clips", L1, Command, Synchronous,
                            1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_remove, "clips.remove", "clips", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_move, "clips.move", "clips", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_resize_left, "clips.resize_left", "clips", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_resize_right, "clips.resize_right", "clips", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_rename, "clips.rename", "clips", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_set_gain, "clips.set_gain", "clips", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_set_mute, "clips.set_mute", "clips", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_set_default_language, "clips.set_default_language", "clips", L1,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_use_track_voice, "clips.use_track_voice", "clips", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_set_voice, "clips.set_voice", "clips", L1, Command, Synchronous,
                            1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clips_clear_voice, "clips.clear_voice", "clips", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(audio_clips_get, "audio_clips.get", "audio_clips", L2, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(audio_clips_import, "audio_clips.import", "audio_clips", L2, Command,
                            Asynchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(audio_clips_import_batch, "audio_clips.import_batch", "audio_clips", L2,
                            Command, Asynchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(audio_clips_relocate, "audio_clips.relocate", "audio_clips", L2,
                            Command, Asynchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(audio_clips_confirm_path, "audio_clips.confirm_path", "audio_clips", L2,
                            Command, Asynchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(voices_list, "voices.list", "voices", L1, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(voices_describe, "voices.describe", "voices", L1, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_get, "speaker_mix.get", "speaker_mix", L1, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_set_fixed, "speaker_mix.set_fixed", "speaker_mix", L1,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_enable_dynamic, "speaker_mix.enable_dynamic", "speaker_mix",
                            L1, Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_disable_dynamic, "speaker_mix.disable_dynamic",
                            "speaker_mix", L1, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_set_dynamic_bypass, "speaker_mix.set_dynamic_bypass",
                            "speaker_mix", L1, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_keyframes_insert, "speaker_mix.keyframes.insert",
                            "speaker_mix", L1, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_keyframes_move, "speaker_mix.keyframes.move", "speaker_mix",
                            L1, Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_keyframes_set_weights, "speaker_mix.keyframes.set_weights",
                            "speaker_mix", L1, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_keyframes_remove, "speaker_mix.keyframes.remove",
                            "speaker_mix", L1, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_presets_list, "speaker_mix.presets.list", "speaker_mix", L2,
                            Query, Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_presets_save, "speaker_mix.presets.save", "speaker_mix", L2,
                            Command, Synchronous, 1, Destructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_presets_delete, "speaker_mix.presets.delete", "speaker_mix",
                            L2, Command, Synchronous, 1, Destructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(speaker_mix_presets_apply, "speaker_mix.presets.apply", "speaker_mix",
                            L2, Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(notes_list, "notes.list", "notes", L1, Query, Synchronous, 1, ReadOnly,
                            Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_search, "notes.search", "notes", L1, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_insert, "notes.insert", "notes", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_duplicate, "notes.duplicate", "notes", L1, Command, Synchronous,
                            1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_remove, "notes.remove", "notes", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_move, "notes.move", "notes", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_resize_left, "notes.resize_left", "notes", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_resize_right, "notes.resize_right", "notes", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_split_at, "notes.split_at", "notes", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_quantize, "notes.quantize", "notes", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_set_lyric, "notes.set_lyric", "notes", L1, Command, Synchronous,
                            1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_set_language, "notes.set_language", "notes", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_set_pronunciation, "notes.set_pronunciation", "notes", L1,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_reset_pronunciation, "notes.reset_pronunciation", "notes", L1,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_set_phonemes, "notes.set_phonemes", "notes", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_set_phoneme_offsets, "notes.set_phoneme_offsets", "notes", L1,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_reset_phoneme_offsets, "notes.reset_phoneme_offsets", "notes", L1,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_reset_phonemes, "notes.reset_phonemes", "notes", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(notes_fill_lyrics, "notes.fill_lyrics", "notes", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(parameters_get_capabilities, "parameters.get_capabilities",
                            "parameters", L1, Query, Synchronous, 1, ReadOnly, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_get, "parameters.get", "parameters", L1, Query, Synchronous,
                            1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_replace, "parameters.replace", "parameters", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_draw, "parameters.draw", "parameters", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_erase, "parameters.erase", "parameters", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_bake, "parameters.bake", "parameters", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_create_anchor_curve, "parameters.create_anchor_curve",
                            "parameters", L1, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_insert_anchors, "parameters.insert_anchors", "parameters",
                            L1, Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_merge_anchor_curves, "parameters.merge_anchor_curves",
                            "parameters", L1, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_move_anchors, "parameters.move_anchors", "parameters", L1,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_remove_anchors, "parameters.remove_anchors", "parameters",
                            L1, Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(parameters_set_anchor_interpolation,
                            "parameters.set_anchor_interpolation", "parameters", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(timeline_get, "timeline.get", "timeline", L1, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tempos_set, "tempos.set", "timeline", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tempos_remove, "tempos.remove", "timeline", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(time_signatures_set, "time_signatures.set", "timeline", L1, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(time_signatures_remove, "time_signatures.remove", "timeline", L1,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(history_get_state, "history.get_state", "history", L1, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(history_undo, "history.undo", "history", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(history_redo, "history.redo", "history", L1, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(playback_get_state, "playback.get_state", "playback", L2, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(playback_play, "playback.play", "playback", L2, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(playback_pause, "playback.pause", "playback", L2, Command, Synchronous,
                            1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(playback_stop, "playback.stop", "playback", L2, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(playback_seek, "playback.seek", "playback", L2, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(playback_set_loop, "playback.set_loop", "playback", L2, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(playback_set_loop_enabled, "playback.set_loop_enabled", "playback", L2,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(playback_clear_loop, "playback.clear_loop", "playback", L2, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(exports_midi_get_capabilities, "exports.midi.get_capabilities",
                            "exports", L2, Query, Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(exports_midi_preview, "exports.midi.preview", "exports", L2, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(exports_midi_start, "exports.midi.start", "exports", L2, Command,
                            Asynchronous, 1, Destructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(exports_audio_get_capabilities, "exports.audio.get_capabilities",
                            "exports", L2, Query, Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(exports_audio_preview, "exports.audio.preview", "exports", L2, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(exports_audio_start, "exports.audio.start", "exports", L2, Command,
                            Asynchronous, 1, Destructive, NonIdempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(extract_get_capabilities, "extract.get_capabilities", "extract", L2,
                            Query, Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(extract_pitch_start, "extract.pitch.start", "extract", L2, Command,
                            Asynchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(extract_midi_start, "extract.midi.start", "extract", L2, Command,
                            Asynchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(inference_get_capabilities, "inference.get_capabilities", "inference",
                            L2, Query, Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(inference_get_status, "inference.get_status", "inference", L2, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(inference_start, "inference.start", "inference", L2, Command,
                            Asynchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(inference_reset_stage, "inference.reset_stage", "inference", L2,
                            Command, Synchronous, 1, Destructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(tasks_list, "tasks.list", "tasks", L2, Query, Synchronous, 1, ReadOnly,
                            Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tasks_get, "tasks.get", "tasks", L2, Query, Synchronous, 1, ReadOnly,
                            Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(tasks_cancel, "tasks.cancel", "tasks", L2, Command, Synchronous, 1,
                            NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(workspace_get_state, "workspace.get_state", "workspace", L3, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(workspace_set_panel_visibility, "workspace.set_panel_visibility",
                            "workspace", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(track_panel_get_state, "track_panel.get_state", "track_panel", L3, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(track_panel_set_viewport, "track_panel.set_viewport", "track_panel", L3,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(track_panel_reveal_clips, "track_panel.reveal_clips", "track_panel", L3,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(track_panel_set_auto_page_turn, "track_panel.set_auto_page_turn",
                            "track_panel", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(track_panel_select_track, "track_panel.select_track", "track_panel", L3,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(track_panel_select_clips, "track_panel.select_clips", "track_panel", L3,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(track_panel_clear_selection, "track_panel.clear_selection",
                            "track_panel", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_get_state, "clip_editor.get_state", "clip_editor", L3, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_set_active_clip, "clip_editor.set_active_clip",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_set_time_viewport, "clip_editor.set_time_viewport",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_set_auto_page_turn, "clip_editor.set_auto_page_turn",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_show_region, "clip_editor.show_region", "clip_editor", L3,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_piano_set_pitch_viewport,
                            "clip_editor.piano.set_pitch_viewport", "clip_editor", L3, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_piano_reveal_notes, "clip_editor.piano.reveal_notes",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_piano_set_edit_mode, "clip_editor.piano.set_edit_mode",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_piano_set_quantize, "clip_editor.piano.set_quantize",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_piano_select_notes, "clip_editor.piano.select_notes",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_piano_clear_selection, "clip_editor.piano.clear_selection",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_parameters_set_foreground,
                            "clip_editor.parameters.set_foreground", "clip_editor", L3, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_parameters_set_background,
                            "clip_editor.parameters.set_background", "clip_editor", L3, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_parameters_swap, "clip_editor.parameters.swap",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive,
                            NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_parameters_set_tool, "clip_editor.parameters.set_tool",
                            "clip_editor", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(clip_editor_parameters_set_value_viewport,
                            "clip_editor.parameters.set_value_viewport", "clip_editor", L3, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(settings_query, "settings.query", "settings", L3, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_ui_language_update, "settings.ui_language.update", "settings",
                            L3, Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_singing_update, "settings.singing.update", "settings", L3,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_theme_update, "settings.theme.update", "settings", L3, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_audio_device_update, "settings.audio_device.update",
                            "settings", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_playback_behavior_update, "settings.playback_behavior.update",
                            "settings", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_compute_device_update, "settings.compute_device.update",
                            "settings", L3, Command, Synchronous, 1, NonDestructive, Idempotent,
                            ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_render_update, "settings.render.update", "settings", L3,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_singer_session_retention_update,
                            "settings.singer_session_retention.update", "settings", L3, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(settings_package_search_paths_update,
                            "settings.package_search_paths.update", "settings", L3, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(packages_list, "packages.list", "packages", L3, Query, Synchronous, 1,
                            ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(packages_describe, "packages.describe", "packages", L3, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(packages_refresh, "packages.refresh", "packages", L3, Command,
                            Asynchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)

AUTOMATION_WIRE_PUBLIC_TOOL(lyric_rules_list, "lyric_rules.list", "lyric_rules", L3, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(lyric_rules_create, "lyric_rules.create", "lyric_rules", L3, Command,
                            Synchronous, 1, NonDestructive, NonIdempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(lyric_rules_update, "lyric_rules.update", "lyric_rules", L3, Command,
                            Synchronous, 1, Destructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(lyric_rules_delete, "lyric_rules.delete", "lyric_rules", L3, Command,
                            Synchronous, 1, Destructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(lyric_rules_set_enabled, "lyric_rules.set_enabled", "lyric_rules", L3,
                            Command, Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(lyric_rules_move, "lyric_rules.move", "lyric_rules", L3, Command,
                            Synchronous, 1, NonDestructive, Idempotent, ClosedWorld)
AUTOMATION_WIRE_PUBLIC_TOOL(lyric_rules_test, "lyric_rules.test", "lyric_rules", L3, Query,
                            Synchronous, 1, ReadOnly, Idempotent, ClosedWorld)
