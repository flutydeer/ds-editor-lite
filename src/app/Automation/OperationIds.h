#ifndef OPERATIONIDS_H
#define OPERATIONIDS_H

#include "AutomationTypes.h"

#include <QLatin1StringView>

namespace Automation::OperationIds {

#define AUTOMATION_OPERATION_ID_LIST(X)                                                           \
    X(application, get_info, "application.get_info")                                             \
    X(application, request_exit, "application.request_exit")                                     \
    X(application, request_restart, "application.request_restart")                               \
    X(audio_clips, apply_decode_cache, "audio_clips.apply_decode_cache")                          \
    X(audio_clips, apply_resolved_path, "audio_clips.apply_resolved_path")                        \
    X(audio_clips, confirm_path, "audio_clips.confirm_path")                                     \
    X(audio_clips, relocate, "audio_clips.relocate")                                             \
    X(audio_clips, set_hash, "audio_clips.set_hash")                                             \
    X(audio_clips, set_path_status, "audio_clips.set_path_status")                               \
    X(clips, insert, "clips.insert")                                                              \
    X(clips, remove, "clips.remove")                                                              \
    X(clips, set_default_language, "clips.set_default_language")                                 \
    X(clips, set_properties, "clips.set_properties")                                             \
    X(documents, commit_import, "documents.commit_import")                                       \
    X(documents, commit_new, "documents.commit_new")                                             \
    X(documents, commit_open, "documents.commit_open")                                           \
    X(documents, get, "documents.get")                                                            \
    X(documents, save, "documents.save")                                                          \
    X(editor, center_piano_roll, "editor.center_piano_roll")                                     \
    X(editor, center_track_panel, "editor.center_track_panel")                                   \
    X(editor, get_capabilities, "editor.get_capabilities")                                       \
    X(editor, get_state, "editor.get_state")                                                     \
    X(editor, restore_view, "editor.restore_view")                                               \
    X(editor, reveal, "editor.reveal")                                                           \
    X(editor, set_active_clip, "editor.set_active_clip")                                         \
    X(editor, set_auto_page_turn, "editor.set_auto_page_turn")                                   \
    X(editor, set_panel_visibility, "editor.set_panel_visibility")                               \
    X(editor, set_piano_roll_edit_mode, "editor.set_piano_roll_edit_mode")                       \
    X(editor, set_piano_roll_scale, "editor.set_piano_roll_scale")                               \
    X(editor, set_quantize, "editor.set_quantize")                                               \
    X(editor, set_selection, "editor.set_selection")                                             \
    X(editor, set_track_panel_scale, "editor.set_track_panel_scale")                             \
    X(editor, show_bottom_panel_page, "editor.show_bottom_panel_page")                           \
    X(exports::audio, cleanup, "exports.audio.cleanup")                                          \
    X(exports::audio, preview, "exports.audio.preview")                                          \
    X(exports::audio, start, "exports.audio.start")                                              \
    X(exports::midi, start, "exports.midi.start")                                                \
    X(extract::midi, start, "extract.midi.start")                                                \
    X(extract::pitch, start, "extract.pitch.start")                                              \
    X(formats, list, "formats.list")                                                             \
    X(history, get_state, "history.get_state")                                                   \
    X(history, redo, "history.redo")                                                             \
    X(history, undo, "history.undo")                                                             \
    X(imports, commit_batch, "imports.commit_batch")                                             \
    X(inference, apply_acoustic, "inference.apply_acoustic")                                     \
    X(inference, apply_duration, "inference.apply_duration")                                     \
    X(inference, apply_phoneme_names, "inference.apply_phoneme_names")                           \
    X(inference, apply_pitch, "inference.apply_pitch")                                           \
    X(inference, apply_pronunciations, "inference.apply_pronunciations")                         \
    X(inference, apply_variance, "inference.apply_variance")                                     \
    X(inference, invalidate_clip, "inference.invalidate_clip")                                   \
    X(inference, rebuild_original_params, "inference.rebuild_original_params")                   \
    X(inference, refresh_param_input, "inference.refresh_param_input")                           \
    X(inference, refresh_speaker_mix, "inference.refresh_speaker_mix")                           \
    X(inference, resegment_clip, "inference.resegment_clip")                                     \
    X(inference, reset_stage, "inference.reset_stage")                                           \
    X(master, set_control, "master.set_control")                                                 \
    X(notes, get, "notes.get")                                                                   \
    X(notes, insert, "notes.insert")                                                             \
    X(notes, move, "notes.move")                                                                 \
    X(notes, quantize, "notes.quantize")                                                         \
    X(notes, remove, "notes.remove")                                                             \
    X(notes, resize_left, "notes.resize_left")                                                   \
    X(notes, resize_right, "notes.resize_right")                                                 \
    X(notes, set_phoneme_offsets, "notes.set_phoneme_offsets")                                   \
    X(notes, set_word_properties, "notes.set_word_properties")                                   \
    X(notes, split, "notes.split")                                                               \
    X(operations, cancel, "operations.cancel")                                                   \
    X(operations, get, "operations.get")                                                         \
    X(operations, list, "operations.list")                                                       \
    X(packages, get_search_paths, "packages.get_search_paths")                                   \
    X(packages, list, "packages.list")                                                           \
    X(packages, resolve_document_voices, "packages.resolve_document_voices")                     \
    X(packages, set_search_paths, "packages.set_search_paths")                                   \
    X(packages, validate, "packages.validate")                                                   \
    X(parameters, get, "parameters.get")                                                         \
    X(parameters, replace, "parameters.replace")                                                 \
    X(playback, clear_loop, "playback.clear_loop")                                               \
    X(playback, get, "playback.get")                                                             \
    X(playback, pause, "playback.pause")                                                         \
    X(playback, play, "playback.play")                                                           \
    X(playback, set_last_position, "playback.set_last_position")                                 \
    X(playback, set_loop, "playback.set_loop")                                                   \
    X(playback, set_loop_enabled, "playback.set_loop_enabled")                                   \
    X(playback, set_position, "playback.set_position")                                           \
    X(playback, stop, "playback.stop")                                                           \
    X(project, get, "project.get")                                                               \
    X(recent_files, add, "recent_files.add")                                                     \
    X(recent_files, clear, "recent_files.clear")                                                 \
    X(recent_files, list, "recent_files.list")                                                   \
    X(recent_files, remove, "recent_files.remove")                                               \
    X(settings, get, "settings.get")                                                             \
    X(settings, update_appearance, "settings.update_appearance")                                 \
    X(settings, update_audio, "settings.update_audio")                                           \
    X(settings, update_developer, "settings.update_developer")                                   \
    X(settings, update_fill_lyric, "settings.update_fill_lyric")                                 \
    X(settings, update_g2p_language, "settings.update_g2p_language")                             \
    X(settings, update_general, "settings.update_general")                                       \
    X(settings, update_inference, "settings.update_inference")                                   \
    X(settings, update_window, "settings.update_window")                                         \
    X(speaker_mix::clip, apply, "speaker_mix.clip.apply")                                        \
    X(speaker_mix::clip, enable_dynamic, "speaker_mix.clip.enable_dynamic")                      \
    X(speaker_mix::clip, replace, "speaker_mix.clip.replace")                                    \
    X(speaker_mix::clip, select_single, "speaker_mix.clip.select_single")                        \
    X(speaker_mix::clip, use_track, "speaker_mix.clip.use_track")                                \
    X(speaker_mix::track, apply, "speaker_mix.track.apply")                                      \
    X(speaker_mix::track, replace, "speaker_mix.track.replace")                                  \
    X(speaker_mix::track, select_single, "speaker_mix.track.select_single")                      \
    X(speaker_mix_presets, delete_preset, "speaker_mix_presets.delete")                          \
    X(speaker_mix_presets, list, "speaker_mix_presets.list")                                     \
    X(speaker_mix_presets, save, "speaker_mix_presets.save")                                    \
    X(tempos, delete_tempo, "tempos.delete")                                                     \
    X(tempos, set, "tempos.set")                                                                 \
    X(time_signatures, delete_signature, "time_signatures.delete")                               \
    X(time_signatures, set, "time_signatures.set")                                               \
    X(timeline, get, "timeline.get")                                                             \
    X(tracks, insert, "tracks.insert")                                                           \
    X(tracks, move, "tracks.move")                                                               \
    X(tracks, remove, "tracks.remove")                                                           \
    X(tracks, set_color, "tracks.set_color")                                                     \
    X(tracks, set_default_language, "tracks.set_default_language")                               \
    X(tracks, set_properties, "tracks.set_properties")

#define AUTOMATION_DECLARE_OPERATION_ID(scope, name, value)                                      \
    namespace scope {                                                                             \
        inline constexpr QLatin1StringView name(value);                                            \
    }

    AUTOMATION_OPERATION_ID_LIST(AUTOMATION_DECLARE_OPERATION_ID)

#undef AUTOMATION_DECLARE_OPERATION_ID

    inline const QStringList &all() {
#define AUTOMATION_REFERENCE_OPERATION_ID(scope, name, value) scope::name,
        static const QStringList ids = {
            AUTOMATION_OPERATION_ID_LIST(AUTOMATION_REFERENCE_OPERATION_ID)
        };
#undef AUTOMATION_REFERENCE_OPERATION_ID
        return ids;
    }

#undef AUTOMATION_OPERATION_ID_LIST

} // namespace Automation::OperationIds

#endif // OPERATIONIDS_H
