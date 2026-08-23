// tracking id, operation id, category, minimum profile, operation kind, sync mode
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-001", "application.get_info", "application", Meta, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-002", "automation.get_status", "automation", Meta, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-003", "automation.get_manifest", "automation", Meta, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-004", "automation.get_options", "automation", Meta, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-005", "documents.get", "documents", L1, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-006", "project.get", "project", L1, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-007", "notes.get", "notes", L1, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-008", "parameters.get", "parameters", L1, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-009", "parameters.get_capabilities", "parameters", L1,
                            Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-010", "timeline.get", "timeline", L1, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-011", "history.get_state", "history", L1, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-012", "voices.list", "voices", L1, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-013", "voices.describe", "voices", L1, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-014", "tracks.insert", "tracks", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-015", "tracks.remove", "tracks", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-016", "tracks.move", "tracks", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-017", "tracks.set_properties", "tracks", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-018", "tracks.set_color", "tracks", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-019", "tracks.set_default_language", "tracks", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-020", "clips.insert", "clips", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-021", "clips.remove", "clips", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-022", "clips.set_properties", "clips", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-023", "clips.set_default_language", "clips", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-024", "notes.insert", "notes", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-025", "notes.remove", "notes", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-026", "notes.move", "notes", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-027", "notes.resize_left", "notes", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-028", "notes.resize_right", "notes", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-029", "notes.split", "notes", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-030", "notes.quantize", "notes", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-031", "notes.set_word_properties", "notes", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-032", "notes.set_phoneme_offsets", "notes", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-033", "parameters.replace", "parameters", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-034", "parameters.draw", "parameters", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-035", "parameters.erase", "parameters", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-036", "parameters.insert_anchor", "parameters", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-037", "parameters.move_anchor", "parameters", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-038", "parameters.remove_anchor", "parameters", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-039", "parameters.set_anchor_interpolation", "parameters",
                            L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-040", "parameters.bake", "parameters", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-041", "speaker_mix.track.select_single", "speaker_mix", L1,
                            Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-042", "speaker_mix.track.apply", "speaker_mix", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-043", "speaker_mix.track.replace", "speaker_mix", L1,
                            Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-044", "speaker_mix.clip.use_track", "speaker_mix", L1,
                            Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-045", "speaker_mix.clip.select_single", "speaker_mix", L1,
                            Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-046", "speaker_mix.clip.enable_dynamic", "speaker_mix", L1,
                            Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-047", "speaker_mix.clip.apply", "speaker_mix", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-048", "speaker_mix.clip.replace", "speaker_mix", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-049", "tempos.set", "tempos", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-050", "tempos.delete", "tempos", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-051", "time_signatures.set", "time_signatures", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-052", "time_signatures.delete", "time_signatures", L1,
                            Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-053", "master.set_control", "master", L1, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-054", "history.undo", "history", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-055", "history.redo", "history", L1, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-056", "automation.get_file_access", "automation", L2, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-057", "documents.new", "documents", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-058", "documents.open", "documents", L2, Command,
                            Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-059", "documents.import", "documents", L2, Command,
                            Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-060", "documents.save", "documents", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-061", "formats.list", "formats", L2, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-062", "audio_clips.import", "audio_clips", L2, Command,
                            Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-063", "audio_clips.import_batch", "audio_clips", L2,
                            Command, Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-064", "audio_clips.relocate", "audio_clips", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-065", "audio_clips.confirm_path", "audio_clips", L2,
                            Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-066", "exports.midi.start", "exports", L2, Command,
                            Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-067", "exports.audio.get_capabilities", "exports", L2, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-068", "exports.audio.preview", "exports", L2, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-069", "exports.audio.start", "exports", L2, Command,
                            Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-070", "extract.get_capabilities", "extract", L2, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-071", "extract.pitch.start", "extract", L2, Command,
                            Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-072", "extract.midi.start", "extract", L2, Command,
                            Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-073", "inference.get_capabilities", "inference", L2, Query,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-074", "inference.start", "inference", L2, Command,
                            Asynchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-075", "inference.reset_stage", "inference", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-076", "tasks.list", "tasks", L2, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-077", "tasks.get", "tasks", L2, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-078", "tasks.cancel", "tasks", L2, Command, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-079", "playback.get", "playback", L2, Query, Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-080", "playback.play", "playback", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-081", "playback.pause", "playback", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-082", "playback.stop", "playback", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-083", "playback.set_position", "playback", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-084", "playback.set_last_position", "playback", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-085", "playback.set_loop", "playback", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-086", "playback.set_loop_enabled", "playback", L2, Command,
                            Synchronous)
AUTOMATION_WIRE_PUBLIC_TOOL("P2-TOOL-087", "playback.clear_loop", "playback", L2, Command,
                            Synchronous)
