#ifndef TESTS_PUBLICAUTOMATIONTOOLSETEXPECTATIONS_H
#define TESTS_PUBLICAUTOMATIONTOOLSETEXPECTATIONS_H

#include <lite/AutomationWire/AutomationProfile.h>

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

namespace PublicAutomationToolsetExpectations {

    struct EditorTool final {
        QString operationId;
        QString category;
        AutomationWire::AutomationProfile minimumProfile;
    };

    inline const QList<EditorTool> &editorTools() {
        using AutomationWire::AutomationProfile;
        static const QList<EditorTool> tools{
            {QStringLiteral("application.get_info"), QStringLiteral("application"),
             AutomationProfile::Meta},

            {QStringLiteral("automation.get_status"), QStringLiteral("automation"),
             AutomationProfile::Meta},
            {QStringLiteral("automation.get_manifest"), QStringLiteral("automation"),
             AutomationProfile::Meta},
            {QStringLiteral("automation.get_options"), QStringLiteral("automation"),
             AutomationProfile::Meta},
            {QStringLiteral("automation.get_file_access"), QStringLiteral("automation"),
             AutomationProfile::L2},

            {QStringLiteral("documents.get"), QStringLiteral("documents"), AutomationProfile::L1},
            {QStringLiteral("project.get"), QStringLiteral("documents"), AutomationProfile::L1},
            {QStringLiteral("documents.new"), QStringLiteral("documents"), AutomationProfile::L2},
            {QStringLiteral("documents.open"), QStringLiteral("documents"), AutomationProfile::L2},
            {QStringLiteral("documents.save"), QStringLiteral("documents"), AutomationProfile::L2},
            {QStringLiteral("documents.save_as"), QStringLiteral("documents"),
             AutomationProfile::L2},
            {QStringLiteral("documents.import"), QStringLiteral("documents"),
             AutomationProfile::L2},
            {QStringLiteral("documents.import_batch"), QStringLiteral("documents"),
             AutomationProfile::L2},

            {QStringLiteral("formats.list"), QStringLiteral("formats"), AutomationProfile::L2},
            {QStringLiteral("formats.inspect"), QStringLiteral("formats"), AutomationProfile::L2},

            {QStringLiteral("tracks.list"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.get"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.insert"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.remove"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.move"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.rename"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.set_color"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.set_gain"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.set_pan"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.set_mute"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.set_solo"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.set_default_language"), QStringLiteral("tracks"),
             AutomationProfile::L1},
            {QStringLiteral("tracks.get_voice_context"), QStringLiteral("tracks"),
             AutomationProfile::L1},
            {QStringLiteral("tracks.set_voice"), QStringLiteral("tracks"), AutomationProfile::L1},
            {QStringLiteral("tracks.clear_voice"), QStringLiteral("tracks"),
             AutomationProfile::L1},

            {QStringLiteral("master.get"), QStringLiteral("bus"), AutomationProfile::L1},
            {QStringLiteral("master.set_gain"), QStringLiteral("bus"), AutomationProfile::L1},
            {QStringLiteral("master.set_pan"), QStringLiteral("bus"), AutomationProfile::L1},
            {QStringLiteral("master.set_mute"), QStringLiteral("bus"), AutomationProfile::L1},
            {QStringLiteral("master.set_solo"), QStringLiteral("bus"), AutomationProfile::L1},

            {QStringLiteral("clips.list"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.get"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.insert"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.duplicate"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.remove"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.move"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.resize_left"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.resize_right"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.rename"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.set_gain"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.set_mute"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.set_default_language"), QStringLiteral("clips"),
             AutomationProfile::L1},
            {QStringLiteral("clips.get_voice_context"), QStringLiteral("clips"),
             AutomationProfile::L1},
            {QStringLiteral("clips.use_track_voice"), QStringLiteral("clips"),
             AutomationProfile::L1},
            {QStringLiteral("clips.set_voice"), QStringLiteral("clips"), AutomationProfile::L1},
            {QStringLiteral("clips.clear_voice"), QStringLiteral("clips"), AutomationProfile::L1},

            {QStringLiteral("audio_clips.get"), QStringLiteral("audio_clips"),
             AutomationProfile::L2},
            {QStringLiteral("audio_clips.import"), QStringLiteral("audio_clips"),
             AutomationProfile::L2},
            {QStringLiteral("audio_clips.import_batch"), QStringLiteral("audio_clips"),
             AutomationProfile::L2},
            {QStringLiteral("audio_clips.relocate"), QStringLiteral("audio_clips"),
             AutomationProfile::L2},
            {QStringLiteral("audio_clips.confirm_path"), QStringLiteral("audio_clips"),
             AutomationProfile::L2},

            {QStringLiteral("voices.list"), QStringLiteral("voices"), AutomationProfile::L1},
            {QStringLiteral("voices.describe"), QStringLiteral("voices"), AutomationProfile::L1},

            {QStringLiteral("speaker_mix.get"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},
            {QStringLiteral("speaker_mix.set_fixed"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},
            {QStringLiteral("speaker_mix.enable_dynamic"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},
            {QStringLiteral("speaker_mix.disable_dynamic"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},
            {QStringLiteral("speaker_mix.set_dynamic_bypass"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},
            {QStringLiteral("speaker_mix.keyframes.insert"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},
            {QStringLiteral("speaker_mix.keyframes.move"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},
            {QStringLiteral("speaker_mix.keyframes.set_weights"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},
            {QStringLiteral("speaker_mix.keyframes.remove"), QStringLiteral("speaker_mix"),
             AutomationProfile::L1},

            {QStringLiteral("notes.get"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.search"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.insert"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.duplicate"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.remove"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.move"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.resize_left"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.resize_right"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.split_at"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.quantize"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.set_lyric"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.set_language"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.set_pronunciation"), QStringLiteral("notes"),
             AutomationProfile::L1},
            {QStringLiteral("notes.reset_pronunciation"), QStringLiteral("notes"),
             AutomationProfile::L1},
            {QStringLiteral("notes.set_phonemes"), QStringLiteral("notes"), AutomationProfile::L1},
            {QStringLiteral("notes.set_phoneme_offsets"), QStringLiteral("notes"),
             AutomationProfile::L1},
            {QStringLiteral("notes.reset_phoneme_offsets"), QStringLiteral("notes"),
             AutomationProfile::L1},
            {QStringLiteral("notes.reset_phonemes"), QStringLiteral("notes"),
             AutomationProfile::L1},
            {QStringLiteral("notes.fill_lyrics"), QStringLiteral("notes"), AutomationProfile::L1},

            {QStringLiteral("parameters.get_capabilities"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.get"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.replace"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.draw"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.erase"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.bake"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.insert_anchors"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.move_anchors"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.remove_anchors"), QStringLiteral("parameters"),
             AutomationProfile::L1},
            {QStringLiteral("parameters.set_anchor_interpolation"), QStringLiteral("parameters"),
             AutomationProfile::L1},

            {QStringLiteral("timeline.get"), QStringLiteral("timeline"), AutomationProfile::L1},
            {QStringLiteral("tempos.set"), QStringLiteral("timeline"), AutomationProfile::L1},
            {QStringLiteral("tempos.delete"), QStringLiteral("timeline"), AutomationProfile::L1},
            {QStringLiteral("time_signatures.set"), QStringLiteral("timeline"),
             AutomationProfile::L1},
            {QStringLiteral("time_signatures.delete"), QStringLiteral("timeline"),
             AutomationProfile::L1},

            {QStringLiteral("history.get_state"), QStringLiteral("history"), AutomationProfile::L1},
            {QStringLiteral("history.undo"), QStringLiteral("history"), AutomationProfile::L1},
            {QStringLiteral("history.redo"), QStringLiteral("history"), AutomationProfile::L1},

            {QStringLiteral("playback.get"), QStringLiteral("playback"), AutomationProfile::L2},
            {QStringLiteral("playback.play"), QStringLiteral("playback"), AutomationProfile::L2},
            {QStringLiteral("playback.pause"), QStringLiteral("playback"), AutomationProfile::L2},
            {QStringLiteral("playback.stop"), QStringLiteral("playback"), AutomationProfile::L2},
            {QStringLiteral("playback.seek"), QStringLiteral("playback"), AutomationProfile::L2},
            {QStringLiteral("playback.set_loop"), QStringLiteral("playback"),
             AutomationProfile::L2},
            {QStringLiteral("playback.set_loop_enabled"), QStringLiteral("playback"),
             AutomationProfile::L2},
            {QStringLiteral("playback.clear_loop"), QStringLiteral("playback"),
             AutomationProfile::L2},

            {QStringLiteral("exports.midi.get_capabilities"), QStringLiteral("exports"),
             AutomationProfile::L2},
            {QStringLiteral("exports.midi.preview"), QStringLiteral("exports"),
             AutomationProfile::L2},
            {QStringLiteral("exports.midi.start"), QStringLiteral("exports"),
             AutomationProfile::L2},
            {QStringLiteral("exports.audio.get_capabilities"), QStringLiteral("exports"),
             AutomationProfile::L2},
            {QStringLiteral("exports.audio.preview"), QStringLiteral("exports"),
             AutomationProfile::L2},
            {QStringLiteral("exports.audio.start"), QStringLiteral("exports"),
             AutomationProfile::L2},

            {QStringLiteral("extract.get_capabilities"), QStringLiteral("extract"),
             AutomationProfile::L2},
            {QStringLiteral("extract.pitch.start"), QStringLiteral("extract"),
             AutomationProfile::L2},
            {QStringLiteral("extract.midi.start"), QStringLiteral("extract"),
             AutomationProfile::L2},

            {QStringLiteral("inference.get_capabilities"), QStringLiteral("inference"),
             AutomationProfile::L2},
            {QStringLiteral("inference.get_status"), QStringLiteral("inference"),
             AutomationProfile::L2},
            {QStringLiteral("inference.start"), QStringLiteral("inference"),
             AutomationProfile::L2},
            {QStringLiteral("inference.reset_stage"), QStringLiteral("inference"),
             AutomationProfile::L2},

            {QStringLiteral("tasks.list"), QStringLiteral("tasks"), AutomationProfile::L2},
            {QStringLiteral("tasks.get"), QStringLiteral("tasks"), AutomationProfile::L2},
            {QStringLiteral("tasks.cancel"), QStringLiteral("tasks"), AutomationProfile::L2},
        };
        return tools;
    }

    inline QStringList editorToolIds() {
        QStringList result;
        result.reserve(editorTools().size());
        for (const auto &tool : editorTools())
            result.append(tool.operationId);
        return result;
    }

    inline QSet<QString> editorToolIdSet() {
        const auto ids = editorToolIds();
        return QSet<QString>(ids.cbegin(), ids.cend());
    }

    inline const QStringList &connectorToolIds() {
        static const QStringList ids{
            QStringLiteral("connector.get_status"),
            QStringLiteral("connector.reconnect"),
            QStringLiteral("editor.tools.list"),
            QStringLiteral("editor.tools.search"),
            QStringLiteral("editor.tools.describe"),
            QStringLiteral("editor.tools.invoke"),
        };
        return ids;
    }

    inline QSet<QString> completeToolIdSet() {
        auto result = editorToolIdSet();
        for (const auto &id : connectorToolIds())
            result.insert(id);
        return result;
    }

}

#endif // TESTS_PUBLICAUTOMATIONTOOLSETEXPECTATIONS_H
