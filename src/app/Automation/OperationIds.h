#ifndef OPERATIONIDS_H
#define OPERATIONIDS_H

#include "AutomationTypes.h"

#include <QLatin1StringView>

namespace Automation::OperationIds {

#define AUTOMATION_OPERATION_ID_LIST(X)                                                            \
    X(application, get_info, "application.get_info")                                               \
    X(application, request_exit, "application.request_exit")                                       \
    X(application, request_restart, "application.request_restart")                                 \
    X(audio_clips, apply_decode_cache, "audio_clips.apply_decode_cache")                           \
    X(audio_clips, apply_resolved_path, "audio_clips.apply_resolved_path")                         \
    X(audio_clips, confirm_path, "audio_clips.confirm_path")                                       \
    X(audio_clips, get, "audio_clips.get")                                                         \
    X(audio_clips, import_audio, "audio_clips.import")                                             \
    X(audio_clips, import_batch, "audio_clips.import_batch")                                       \
    X(audio_clips, relocate, "audio_clips.relocate")                                               \
    X(audio_clips, set_hash, "audio_clips.set_hash")                                               \
    X(audio_clips, set_path_status, "audio_clips.set_path_status")                                 \
    X(clips, clear_voice, "clips.clear_voice")                                                     \
    X(clips, duplicate, "clips.duplicate")                                                         \
    X(clips, get, "clips.get")                                                                     \
    X(clips, get_voice_context, "clips.get_voice_context")                                         \
    X(clips, insert, "clips.insert")                                                               \
    X(clips, list, "clips.list")                                                                   \
    X(clips, move, "clips.move")                                                                   \
    X(clips, remove, "clips.remove")                                                               \
    X(clips, rename, "clips.rename")                                                               \
    X(clips, resize_left, "clips.resize_left")                                                     \
    X(clips, resize_right, "clips.resize_right")                                                   \
    X(clips, set_default_language, "clips.set_default_language")                                   \
    X(clips, set_gain, "clips.set_gain")                                                           \
    X(clips, set_mute, "clips.set_mute")                                                           \
    X(clips, set_properties, "clips.set_properties")                                               \
    X(clips, set_voice, "clips.set_voice")                                                         \
    X(clips, use_track_voice, "clips.use_track_voice")                                             \
    X(documents, commit_import, "documents.commit_import")                                         \
    X(documents, commit_new, "documents.commit_new")                                               \
    X(documents, commit_open, "documents.commit_open")                                             \
    X(documents, get, "documents.get")                                                             \
    X(documents, import_document, "documents.import")                                              \
    X(documents, import_batch, "documents.import_batch")                                           \
    X(documents, new_document, "documents.new")                                                    \
    X(documents, open, "documents.open")                                                           \
    X(documents, save, "documents.save")                                                           \
    X(documents, save_as, "documents.save_as")                                                     \
    X(editor, center_piano_roll, "editor.center_piano_roll")                                       \
    X(editor, center_track_panel, "editor.center_track_panel")                                     \
    X(editor, focus_region, "editor.focus_region")                                                 \
    X(editor, get_capabilities, "editor.get_capabilities")                                         \
    X(editor, get_state, "editor.get_state")                                                       \
    X(editor, restore_view, "editor.restore_view")                                                 \
    X(editor, reveal, "editor.reveal")                                                             \
    X(editor, set_active_clip, "editor.set_active_clip")                                           \
    X(editor, set_auto_page_turn, "editor.set_auto_page_turn")                                     \
    X(editor, set_clip_editor_time_viewport, "editor.set_clip_editor_time_viewport")               \
    X(editor, set_parameter_background, "editor.set_parameter_background")                         \
    X(editor, set_parameter_edit_mode, "editor.set_parameter_edit_mode")                           \
    X(editor, set_parameter_foreground, "editor.set_parameter_foreground")                         \
    X(editor, set_parameter_value_viewport, "editor.set_parameter_value_viewport")                 \
    X(editor, set_panel_visibility, "editor.set_panel_visibility")                                 \
    X(editor, set_piano_roll_pitch_viewport, "editor.set_piano_roll_pitch_viewport")               \
    X(editor, set_piano_roll_edit_mode, "editor.set_piano_roll_edit_mode")                         \
    X(editor, set_piano_roll_scale, "editor.set_piano_roll_scale")                                 \
    X(editor, set_quantize, "editor.set_quantize")                                                 \
    X(editor, set_selection, "editor.set_selection")                                               \
    X(editor, set_track_panel_viewport, "editor.set_track_panel_viewport")                         \
    X(editor, set_track_panel_scale, "editor.set_track_panel_scale")                               \
    X(editor, show_region, "editor.show_region")                                                   \
    X(editor, show_bottom_panel_page, "editor.show_bottom_panel_page")                             \
    X(editor, swap_parameters, "editor.swap_parameters")                                           \
    X(exports::audio, cleanup, "exports.audio.cleanup")                                            \
    X(exports::audio, get_capabilities, "exports.audio.get_capabilities")                          \
    X(exports::audio, preview, "exports.audio.preview")                                            \
    X(exports::audio, start, "exports.audio.start")                                                \
    X(exports::midi, get_capabilities, "exports.midi.get_capabilities")                            \
    X(exports::midi, preview, "exports.midi.preview")                                              \
    X(exports::midi, start, "exports.midi.start")                                                  \
    X(extract, get_capabilities, "extract.get_capabilities")                                       \
    X(extract::midi, start, "extract.midi.start")                                                  \
    X(extract::pitch, start, "extract.pitch.start")                                                \
    X(formats, inspect, "formats.inspect")                                                         \
    X(formats, list, "formats.list")                                                               \
    X(history, get_state, "history.get_state")                                                     \
    X(history, redo, "history.redo")                                                               \
    X(history, undo, "history.undo")                                                               \
    X(imports, commit_batch, "imports.commit_batch")                                               \
    X(inference, apply_acoustic, "inference.apply_acoustic")                                       \
    X(inference, apply_duration, "inference.apply_duration")                                       \
    X(inference, apply_phoneme_names, "inference.apply_phoneme_names")                             \
    X(inference, apply_pitch, "inference.apply_pitch")                                             \
    X(inference, apply_pronunciations, "inference.apply_pronunciations")                           \
    X(inference, apply_variance, "inference.apply_variance")                                       \
    X(inference, get_capabilities, "inference.get_capabilities")                                   \
    X(inference, get_status, "inference.get_status")                                               \
    X(inference, invalidate_clip, "inference.invalidate_clip")                                     \
    X(inference, rebuild_original_params, "inference.rebuild_original_params")                     \
    X(inference, refresh_param_input, "inference.refresh_param_input")                             \
    X(inference, refresh_speaker_mix, "inference.refresh_speaker_mix")                             \
    X(inference, resegment_clip, "inference.resegment_clip")                                       \
    X(inference, reset_stage, "inference.reset_stage")                                             \
    X(inference, start, "inference.start")                                                         \
    X(master, get, "master.get")                                                                   \
    X(master, set_control, "master.set_control")                                                   \
    X(master, set_gain, "master.set_gain")                                                         \
    X(master, set_mute, "master.set_mute")                                                         \
    X(master, set_pan, "master.set_pan")                                                           \
    X(master, set_solo, "master.set_solo")                                                         \
    X(notes, duplicate, "notes.duplicate")                                                         \
    X(notes, fill_lyrics, "notes.fill_lyrics")                                                     \
    X(notes, list, "notes.list")                                                                   \
    X(notes, insert, "notes.insert")                                                               \
    X(notes, move, "notes.move")                                                                   \
    X(notes, quantize, "notes.quantize")                                                           \
    X(notes, remove, "notes.remove")                                                               \
    X(notes, reset_phoneme_offsets, "notes.reset_phoneme_offsets")                                 \
    X(notes, reset_phonemes, "notes.reset_phonemes")                                               \
    X(notes, reset_pronunciation, "notes.reset_pronunciation")                                     \
    X(notes, resize_left, "notes.resize_left")                                                     \
    X(notes, resize_right, "notes.resize_right")                                                   \
    X(notes, search, "notes.search")                                                               \
    X(notes, set_language, "notes.set_language")                                                   \
    X(notes, set_lyric, "notes.set_lyric")                                                         \
    X(notes, set_phoneme_offsets, "notes.set_phoneme_offsets")                                     \
    X(notes, set_phonemes, "notes.set_phonemes")                                                   \
    X(notes, set_pronunciation, "notes.set_pronunciation")                                         \
    X(notes, set_word_properties, "notes.set_word_properties")                                     \
    X(notes, split, "notes.split")                                                                 \
    X(notes, split_at, "notes.split_at")                                                           \
    X(packages, list, "packages.list")                                                             \
    X(packages, refresh, "packages.refresh")                                                       \
    X(packages, resolve_document_voices, "packages.resolve_document_voices")                       \
    X(packages, set_search_paths, "packages.set_search_paths")                                     \
    X(packages, validate, "packages.validate")                                                     \
    X(parameters, bake, "parameters.bake")                                                         \
    X(parameters, create_anchor_curve, "parameters.create_anchor_curve")                           \
    X(parameters, draw, "parameters.draw")                                                         \
    X(parameters, erase, "parameters.erase")                                                       \
    X(parameters, get, "parameters.get")                                                           \
    X(parameters, get_capabilities, "parameters.get_capabilities")                                 \
    X(parameters, insert_anchors, "parameters.insert_anchors")                                     \
    X(parameters, merge_anchor_curves, "parameters.merge_anchor_curves")                           \
    X(parameters, move_anchors, "parameters.move_anchors")                                         \
    X(parameters, remove_anchors, "parameters.remove_anchors")                                     \
    X(parameters, replace, "parameters.replace")                                                   \
    X(parameters, set_anchor_interpolation, "parameters.set_anchor_interpolation")                 \
    X(playback, clear_loop, "playback.clear_loop")                                                 \
    X(playback, get_state, "playback.get_state")                                                   \
    X(playback, pause, "playback.pause")                                                           \
    X(playback, play, "playback.play")                                                             \
    X(playback, seek, "playback.seek")                                                             \
    X(playback, set_last_position, "playback.set_last_position")                                   \
    X(playback, set_loop, "playback.set_loop")                                                     \
    X(playback, set_loop_enabled, "playback.set_loop_enabled")                                     \
    X(playback, set_position, "playback.set_position")                                             \
    X(playback, stop, "playback.stop")                                                             \
    X(project, get, "project.get")                                                                 \
    X(recent_files, add, "recent_files.add")                                                       \
    X(recent_files, clear, "recent_files.clear")                                                   \
    X(recent_files, list, "recent_files.list")                                                     \
    X(recent_files, remove, "recent_files.remove")                                                 \
    X(settings, query, "settings.query")                                                           \
    X(settings, update_appearance, "settings.update_appearance")                                   \
    X(settings, update_audio, "settings.update_audio")                                             \
    X(settings, update_developer, "settings.update_developer")                                     \
    X(settings, update_fill_lyric, "settings.update_fill_lyric")                                   \
    X(settings, update_g2p_language, "settings.update_g2p_language")                               \
    X(settings, update_general, "settings.update_general")                                         \
    X(settings, update_inference, "settings.update_inference")                                     \
    X(settings, update_window, "settings.update_window")                                           \
    X(speaker_mix::clip, apply, "speaker_mix.clip.apply")                                          \
    X(speaker_mix::clip, enable_dynamic, "speaker_mix.clip.enable_dynamic")                        \
    X(speaker_mix::clip, replace, "speaker_mix.clip.replace")                                      \
    X(speaker_mix::clip, select_single, "speaker_mix.clip.select_single")                          \
    X(speaker_mix::clip, use_track, "speaker_mix.clip.use_track")                                  \
    X(speaker_mix, disable_dynamic, "speaker_mix.disable_dynamic")                                 \
    X(speaker_mix, enable_dynamic, "speaker_mix.enable_dynamic")                                   \
    X(speaker_mix, get, "speaker_mix.get")                                                         \
    X(speaker_mix::keyframes, insert, "speaker_mix.keyframes.insert")                              \
    X(speaker_mix::keyframes, move, "speaker_mix.keyframes.move")                                  \
    X(speaker_mix::keyframes, remove, "speaker_mix.keyframes.remove")                              \
    X(speaker_mix::keyframes, set_weights, "speaker_mix.keyframes.set_weights")                    \
    X(speaker_mix, set_dynamic_bypass, "speaker_mix.set_dynamic_bypass")                           \
    X(speaker_mix, set_fixed, "speaker_mix.set_fixed")                                             \
    X(speaker_mix::track, apply, "speaker_mix.track.apply")                                        \
    X(speaker_mix::track, replace, "speaker_mix.track.replace")                                    \
    X(speaker_mix::track, select_single, "speaker_mix.track.select_single")                        \
    X(speaker_mix_presets, delete_preset, "speaker_mix_presets.delete")                            \
    X(speaker_mix_presets, list, "speaker_mix_presets.list")                                       \
    X(speaker_mix_presets, save, "speaker_mix_presets.save")                                       \
    X(tasks, cancel, "tasks.cancel")                                                               \
    X(tasks, get, "tasks.get")                                                                     \
    X(tasks, list, "tasks.list")                                                                   \
    X(tempos, remove, "tempos.remove")                                                             \
    X(tempos, set, "tempos.set")                                                                   \
    X(time_signatures, remove, "time_signatures.remove")                                           \
    X(time_signatures, set, "time_signatures.set")                                                 \
    X(timeline, get, "timeline.get")                                                               \
    X(tracks, clear_voice, "tracks.clear_voice")                                                   \
    X(tracks, get, "tracks.get")                                                                   \
    X(tracks, get_voice_context, "tracks.get_voice_context")                                       \
    X(tracks, insert, "tracks.insert")                                                             \
    X(tracks, list, "tracks.list")                                                                 \
    X(tracks, move, "tracks.move")                                                                 \
    X(tracks, remove, "tracks.remove")                                                             \
    X(tracks, rename, "tracks.rename")                                                             \
    X(tracks, set_color, "tracks.set_color")                                                       \
    X(tracks, set_default_language, "tracks.set_default_language")                                 \
    X(tracks, set_gain, "tracks.set_gain")                                                         \
    X(tracks, set_mute, "tracks.set_mute")                                                         \
    X(tracks, set_pan, "tracks.set_pan")                                                           \
    X(tracks, set_properties, "tracks.set_properties")                                             \
    X(tracks, set_solo, "tracks.set_solo")                                                         \
    X(tracks, set_voice, "tracks.set_voice")

#define AUTOMATION_DECLARE_OPERATION_ID(scope, name, value)                                        \
    namespace scope {                                                                              \
        inline constexpr QLatin1StringView name(value);                                            \
    }

    AUTOMATION_OPERATION_ID_LIST(AUTOMATION_DECLARE_OPERATION_ID)

#undef AUTOMATION_DECLARE_OPERATION_ID

    inline const QStringList &all() {
#define AUTOMATION_REFERENCE_OPERATION_ID(scope, name, value) scope::name,
        static const QStringList ids = [] {
            QStringList result = {AUTOMATION_OPERATION_ID_LIST(AUTOMATION_REFERENCE_OPERATION_ID)};
            result.sort();
            return result;
        }();
#undef AUTOMATION_REFERENCE_OPERATION_ID
        return ids;
    }

#undef AUTOMATION_OPERATION_ID_LIST

} // namespace Automation::OperationIds

#endif // OPERATIONIDS_H
