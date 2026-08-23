#include "Automation/OperationIds.h"
#include "TestRuntime.h"

#include <QCoreApplication>
#include <QHash>
#include <QSet>
#include <QTextStream>

namespace {
    struct ExpectedOperation {
        Automation::OperationDescriptor descriptor;
        QStringList scenarioIds;
    };

#define EXPECTED_DESCRIPTOR(idValue, scenarioValue, categoryValue, kindValue, syncValue,         \
                            documentValue, revisionValue, historyValue, fileValue, hostValue,    \
                            safetyValue, exposureValue, idempotencyValue)                         \
    {                                                                                             \
        .descriptor =                                                                            \
            {                                                                                     \
                .id = Automation::OperationIds::idValue,                                          \
                .category = QStringLiteral(categoryValue),                                        \
                .kind = Automation::OperationKind::kindValue,                                     \
                .syncMode = Automation::SyncMode::syncValue,                                      \
                .documentPolicy = Automation::DocumentPolicy::documentValue,                      \
                .revisionPolicy = Automation::RevisionPolicy::revisionValue,                      \
                .historyPolicy = Automation::HistoryPolicy::historyValue,                         \
                .fileAccess = Automation::FileAccessPolicy::fileValue,                            \
                .hostAvailability = Automation::HostAvailability::hostValue,                      \
                .safety = Automation::SafetyClass::safetyValue,                                   \
                .exposure = Automation::ExposurePolicy::exposureValue,                            \
                .idempotency = Automation::IdempotencyPolicy::idempotencyValue,                   \
            },                                                                                    \
        .scenarioIds = {QStringLiteral(scenarioValue)},                                           \
    }

    const QList<ExpectedOperation> kExpectedOperations{
        EXPECTED_DESCRIPTOR(application::get_info, "AFC-CATALOG-001", "application", Query,
                            Synchronous, None, None, None, None, Core, ReadOnly, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(application::request_exit, "AFC-CATALOG-002", "application", Command,
                            Synchronous, None, None, None, None, GuiOnly, Destructive, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(application::request_restart, "AFC-CATALOG-003", "application",
                            Command, Synchronous, None, None, None, None, GuiOnly, Destructive,
                            InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(audio_clips::apply_decode_cache, "AFC-CATALOG-004", "audio_clips",
                            Command, Synchronous, Write, None, None, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(audio_clips::apply_resolved_path, "AFC-CATALOG-005", "audio_clips",
                            Command, Synchronous, Write, None, None, Read, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(audio_clips::confirm_path, "AFC-CATALOG-006", "audio_clips", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(audio_clips::relocate, "AFC-CATALOG-007", "audio_clips", Command,
                            Synchronous, Write, Increment, Record, Read, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(audio_clips::set_hash, "AFC-CATALOG-008", "audio_clips", Command,
                            Synchronous, Write, None, None, Read, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(audio_clips::set_path_status, "AFC-CATALOG-009", "audio_clips", Command,
                            Synchronous, Write, None, None, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(clips::insert, "AFC-CATALOG-010", "clips", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(clips::remove, "AFC-CATALOG-011", "clips", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(clips::set_default_language, "AFC-CATALOG-012", "clips", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(clips::set_properties, "AFC-CATALOG-013", "clips", Command, Synchronous,
                            Write, Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(documents::commit_import, "AFC-CATALOG-014", "documents", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(documents::commit_new, "AFC-CATALOG-015", "documents", Command,
                            Synchronous, Replace, Reset, None, None, Core, Destructive, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(documents::commit_open, "AFC-CATALOG-016", "documents", Command,
                            Synchronous, Replace, Reset, None, None, Core, Destructive, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(documents::get, "AFC-CATALOG-017", "documents", Query, Synchronous, Read,
                            None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(documents::save, "AFC-CATALOG-018", "documents", Command, Synchronous,
                            Write, Check, None, Write, Core, FileSystem, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(editor::center_piano_roll, "AFC-CATALOG-019", "editor", Command,
                            Synchronous, None, None, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::center_track_panel, "AFC-CATALOG-020", "editor", Command,
                            Synchronous, None, None, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::get_capabilities, "AFC-CATALOG-021", "editor", Query,
                            Synchronous, None, None, None, None, Core, ReadOnly, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::get_state, "AFC-CATALOG-022", "editor", Query, Synchronous, Read,
                            None, None, None, GuiOnly, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(editor::restore_view, "AFC-CATALOG-023", "editor", Command, Synchronous,
                            None, None, None, None, GuiOnly, Reversible, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(editor::reveal, "AFC-CATALOG-024", "editor", Command, Synchronous, Read,
                            Check, None, None, GuiOnly, Reversible, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(editor::set_active_clip, "AFC-CATALOG-025", "editor", Command,
                            Synchronous, Read, Check, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::set_auto_page_turn, "AFC-CATALOG-026", "editor", Command,
                            Synchronous, None, None, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::set_panel_visibility, "AFC-CATALOG-027", "editor", Command,
                            Synchronous, None, None, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::set_piano_roll_edit_mode, "AFC-CATALOG-028", "editor", Command,
                            Synchronous, None, None, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::set_piano_roll_scale, "AFC-CATALOG-029", "editor", Command,
                            Synchronous, None, None, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::set_quantize, "AFC-CATALOG-030", "editor", Command, Synchronous,
                            None, None, None, None, GuiOnly, Reversible, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(editor::set_selection, "AFC-CATALOG-031", "editor", Command, Synchronous,
                            Read, Check, None, None, GuiOnly, Reversible, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(editor::set_track_panel_scale, "AFC-CATALOG-032", "editor", Command,
                            Synchronous, None, None, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(editor::show_bottom_panel_page, "AFC-CATALOG-033", "editor", Command,
                            Synchronous, None, None, None, None, GuiOnly, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(exports::audio::cleanup, "AFC-CATALOG-034", "exports", Command,
                            Synchronous, Read, Check, None, Write, Core, FileSystem, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(exports::audio::preview, "AFC-CATALOG-035", "exports", Query,
                            Synchronous, Read, None, None, Read, Core, ReadOnly, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(exports::audio::start, "AFC-CATALOG-036", "exports", Command,
                            Asynchronous, Read, Check, None, Write, Core, FileSystem, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(exports::midi::start, "AFC-CATALOG-037", "exports", Command, Synchronous,
                            Read, Check, None, Write, Core, FileSystem, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(extract::midi::start, "AFC-CATALOG-038", "extract", Command,
                            Asynchronous, Write, Increment, Record, Read, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(extract::pitch::start, "AFC-CATALOG-039", "extract", Command,
                            Asynchronous, Write, Increment, Record, Read, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(formats::list, "AFC-CATALOG-040", "files", Query, Synchronous, None, None,
                            None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(history::get_state, "AFC-CATALOG-041", "history", Query, Synchronous, Read,
                            None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(history::redo, "AFC-CATALOG-042", "history", Command, Synchronous, Write,
                            Increment, UndoRedo, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(history::undo, "AFC-CATALOG-043", "history", Command, Synchronous, Write,
                            Increment, UndoRedo, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(imports::commit_batch, "AFC-CATALOG-044", "imports", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(inference::apply_acoustic, "AFC-CATALOG-045", "inference", Command,
                            Synchronous, Write, Check, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::apply_duration, "AFC-CATALOG-046", "inference", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::apply_phoneme_names, "AFC-CATALOG-047", "inference", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::apply_pitch, "AFC-CATALOG-048", "inference", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::apply_pronunciations, "AFC-CATALOG-049", "inference", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::apply_variance, "AFC-CATALOG-050", "inference", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::invalidate_clip, "AFC-CATALOG-051", "inference", Command,
                            Synchronous, Write, Check, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::rebuild_original_params, "AFC-CATALOG-052", "inference",
                            Command, Synchronous, Write, Increment, None, None, Core, Reversible,
                            InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(inference::refresh_param_input, "AFC-CATALOG-053", "inference", Command,
                            Synchronous, Write, Check, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::refresh_speaker_mix, "AFC-CATALOG-054", "inference", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::resegment_clip, "AFC-CATALOG-055", "inference", Command,
                            Synchronous, Write, Check, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(inference::reset_stage, "AFC-CATALOG-056", "inference", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(master::set_control, "AFC-CATALOG-057", "master", Command, Synchronous,
                            Write, Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::get, "AFC-CATALOG-058", "notes", Query, Synchronous, Read, None,
                            None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(notes::insert, "AFC-CATALOG-059", "notes", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::move, "AFC-CATALOG-060", "notes", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::quantize, "AFC-CATALOG-061", "notes", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::remove, "AFC-CATALOG-062", "notes", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::resize_left, "AFC-CATALOG-063", "notes", Command, Synchronous,
                            Write, Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::resize_right, "AFC-CATALOG-064", "notes", Command, Synchronous,
                            Write, Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::set_phoneme_offsets, "AFC-CATALOG-065", "notes", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::set_word_properties, "AFC-CATALOG-066", "notes", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(notes::split, "AFC-CATALOG-067", "notes", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(operations::cancel, "AFC-CATALOG-068", "operations", Command,
                            Synchronous, Read, Check, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(operations::get, "AFC-CATALOG-069", "operations", Query, Synchronous,
                            Read, None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(operations::list, "AFC-CATALOG-070", "operations", Query, Synchronous,
                            Read, None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(packages::get_search_paths, "AFC-CATALOG-071", "packages", Query,
                            Synchronous, None, None, None, None, Core, ReadOnly, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(packages::list, "AFC-CATALOG-072", "packages", Query, Synchronous, None,
                            None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(packages::resolve_document_voices, "AFC-CATALOG-073", "packages",
                            Command, Synchronous, Write, Check, None, None, Core, Reversible,
                            InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(packages::set_search_paths, "AFC-CATALOG-074", "packages", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(packages::validate, "AFC-CATALOG-075", "packages", Query, Synchronous,
                            None, None, None, Read, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(parameters::get, "AFC-CATALOG-076", "parameters", Query, Synchronous,
                            Read, None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(parameters::replace, "AFC-CATALOG-077", "parameters", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(playback::clear_loop, "AFC-CATALOG-078", "playback", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(playback::get, "AFC-CATALOG-079", "playback", Query, Synchronous, Read,
                            None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(playback::pause, "AFC-CATALOG-080", "playback", Command, Synchronous,
                            Read, Check, None, None, Core, Reversible, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(playback::play, "AFC-CATALOG-081", "playback", Command, Synchronous, Read,
                            Check, None, None, Core, Reversible, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(playback::set_last_position, "AFC-CATALOG-082", "playback", Command,
                            Synchronous, Read, Check, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(playback::set_loop, "AFC-CATALOG-083", "playback", Command, Synchronous,
                            Write, Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(playback::set_loop_enabled, "AFC-CATALOG-084", "playback", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(playback::set_position, "AFC-CATALOG-085", "playback", Command,
                            Synchronous, Read, Check, None, None, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(playback::stop, "AFC-CATALOG-086", "playback", Command, Synchronous, Read,
                            Check, None, None, Core, Reversible, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(project::get, "AFC-CATALOG-087", "project", Query, Synchronous, Read, None,
                            None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(recent_files::add, "AFC-CATALOG-088", "recent_files", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(recent_files::clear, "AFC-CATALOG-089", "recent_files", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(recent_files::list, "AFC-CATALOG-090", "recent_files", Query, Synchronous,
                            None, None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(recent_files::remove, "AFC-CATALOG-091", "recent_files", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(settings::get, "AFC-CATALOG-092", "settings", Query, Synchronous, None,
                            None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(settings::update_appearance, "AFC-CATALOG-093", "settings", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(settings::update_audio, "AFC-CATALOG-094", "settings", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(settings::update_developer, "AFC-CATALOG-095", "settings", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(settings::update_fill_lyric, "AFC-CATALOG-096", "settings", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(settings::update_g2p_language, "AFC-CATALOG-097", "settings", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(settings::update_general, "AFC-CATALOG-098", "settings", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(settings::update_inference, "AFC-CATALOG-099", "settings", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(settings::update_window, "AFC-CATALOG-100", "settings", Command,
                            Synchronous, None, None, None, Write, Core, Reversible, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(speaker_mix::clip::apply, "AFC-CATALOG-101", "speaker_mix", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(speaker_mix::clip::enable_dynamic, "AFC-CATALOG-102", "speaker_mix",
                            Command, Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(speaker_mix::clip::replace, "AFC-CATALOG-103", "speaker_mix", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(speaker_mix::clip::select_single, "AFC-CATALOG-104", "speaker_mix",
                            Command, Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(speaker_mix::clip::use_track, "AFC-CATALOG-105", "speaker_mix", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(speaker_mix::track::apply, "AFC-CATALOG-106", "speaker_mix", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(speaker_mix::track::replace, "AFC-CATALOG-107", "speaker_mix", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(speaker_mix::track::select_single, "AFC-CATALOG-108", "speaker_mix",
                            Command, Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(speaker_mix_presets::delete_preset, "AFC-CATALOG-109",
                            "speaker_mix_presets", Command, Synchronous, None, None, None, Write, Core,
                            Reversible, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(speaker_mix_presets::list, "AFC-CATALOG-110", "speaker_mix_presets",
                            Query, Synchronous, None, None, None, None, Core, ReadOnly, InternalOnly,
                            Unsupported),
        EXPECTED_DESCRIPTOR(speaker_mix_presets::save, "AFC-CATALOG-111", "speaker_mix_presets",
                            Command, Synchronous, None, None, None, Write, Core, Reversible,
                            InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(tempos::delete_tempo, "AFC-CATALOG-112", "tempos", Command, Synchronous,
                            Write, Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(tempos::set, "AFC-CATALOG-113", "tempos", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(time_signatures::delete_signature, "AFC-CATALOG-114", "time_signatures",
                            Command, Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(time_signatures::set, "AFC-CATALOG-115", "time_signatures", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
        EXPECTED_DESCRIPTOR(timeline::get, "AFC-CATALOG-116", "timeline", Query, Synchronous, Read,
                            None, None, None, Core, ReadOnly, InternalOnly, Unsupported),
        EXPECTED_DESCRIPTOR(tracks::insert, "AFC-CATALOG-117", "tracks", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(tracks::move, "AFC-CATALOG-118", "tracks", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(tracks::remove, "AFC-CATALOG-119", "tracks", Command, Synchronous, Write,
                            Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(tracks::set_color, "AFC-CATALOG-120", "tracks", Command, Synchronous,
                            Write, Increment, Record, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(tracks::set_default_language, "AFC-CATALOG-121", "tracks", Command,
                            Synchronous, Write, Increment, None, None, Core, Reversible, InternalOnly,
                            DocumentGeneration),
        EXPECTED_DESCRIPTOR(tracks::set_properties, "AFC-CATALOG-122", "tracks", Command,
                            Synchronous, Write, Increment, Record, None, Core, Reversible,
                            InternalOnly, DocumentGeneration),
    };

#undef EXPECTED_DESCRIPTOR

    bool expect(const bool condition, const QString &message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    template <class Value>
    bool expectField(const Value &actual, const Value &expected,
                     const ExpectedOperation &operation, const char *field) {
        return expect(actual == expected,
                      QStringLiteral("[%1] %2 descriptor field '%3' differs")
                          .arg(operation.scenarioIds.constFirst(), operation.descriptor.id,
                               QString::fromLatin1(field)));
    }

    bool checkDescriptor(const Automation::OperationDescriptor &actual,
                         const ExpectedOperation &expected) {
        const auto &descriptor = expected.descriptor;
        bool ok = true;
        ok &= expectField(actual.id, descriptor.id, expected, "id");
        ok &= expectField(actual.category, descriptor.category, expected, "category");
        ok &= expectField(actual.kind, descriptor.kind, expected, "kind");
        ok &= expectField(actual.syncMode, descriptor.syncMode, expected, "syncMode");
        ok &= expectField(actual.documentPolicy, descriptor.documentPolicy, expected,
                          "documentPolicy");
        ok &= expectField(actual.revisionPolicy, descriptor.revisionPolicy, expected,
                          "revisionPolicy");
        ok &= expectField(actual.historyPolicy, descriptor.historyPolicy, expected, "historyPolicy");
        ok &= expectField(actual.fileAccess, descriptor.fileAccess, expected, "fileAccess");
        ok &= expectField(actual.hostAvailability, descriptor.hostAvailability, expected,
                          "hostAvailability");
        ok &= expectField(actual.safety, descriptor.safety, expected, "safety");
        ok &= expectField(actual.exposure, descriptor.exposure, expected, "exposure");
        ok &= expectField(actual.idempotency, descriptor.idempotency, expected, "idempotency");
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    AutomationTestSupport::TestRuntime testRuntime;
    const auto &catalog = testRuntime.runtime().catalog();

    bool ok = true;
    ok &= expect(kExpectedOperations.size() == 122,
                 QStringLiteral("the explicit descriptor table must contain 122 operations"));
    ok &= expect(catalog.entries().size() == kExpectedOperations.size(),
                 QStringLiteral("the real Catalog size differs from the explicit table"));

    QStringList expectedIds;
    expectedIds.reserve(kExpectedOperations.size());
    QHash<Automation::OperationId, int> actualIdCounts;
    QSet<Automation::OperationId> expectedIdSet;
    QSet<QString> scenarioIdSet;

    for (const auto &entry : catalog.entries())
        ++actualIdCounts[entry.id];

    for (const auto &expected : kExpectedOperations) {
        const auto &descriptor = expected.descriptor;
        expectedIds.append(descriptor.id);
        ok &= expect(!expectedIdSet.contains(descriptor.id),
                     QStringLiteral("duplicate operation ID in explicit table: %1")
                         .arg(descriptor.id));
        expectedIdSet.insert(descriptor.id);
        ok &= expect(actualIdCounts.value(descriptor.id) == 1,
                     QStringLiteral("operation ID must occur exactly once in real Catalog: %1")
                         .arg(descriptor.id));

        ok &= expect(!expected.scenarioIds.isEmpty(),
                     QStringLiteral("operation has no stable scenario ID: %1").arg(descriptor.id));
        for (const auto &scenarioId : expected.scenarioIds) {
            ok &= expect(!scenarioId.trimmed().isEmpty(),
                         QStringLiteral("operation has an empty scenario ID: %1")
                             .arg(descriptor.id));
            ok &= expect(!scenarioIdSet.contains(scenarioId),
                         QStringLiteral("duplicate stable scenario ID: %1").arg(scenarioId));
            scenarioIdSet.insert(scenarioId);
        }

        const auto *actual = catalog.find(descriptor.id);
        if (!expect(actual != nullptr,
                    QStringLiteral("real Catalog is missing operation: %1").arg(descriptor.id))) {
            continue;
        }
        ok &= checkDescriptor(*actual, expected);
    }

    ok &= expect(expectedIds == Automation::OperationIds::all(),
                 QStringLiteral("explicit descriptor table differs from OperationIds::all()"));
    ok &= expect(catalog.operationIds() == expectedIds,
                 QStringLiteral("real Catalog has an unexpected, missing, or reordered operation"));
    ok &= expect(expectedIdSet.size() == kExpectedOperations.size(),
                 QStringLiteral("explicit descriptor table operation IDs must be unique"));
    ok &= expect(scenarioIdSet.size() >= kExpectedOperations.size(),
                 QStringLiteral("every operation must have at least one unique stable scenario ID"));

    if (ok) {
        QTextStream(stdout) << "Validated " << kExpectedOperations.size()
                            << " Automation Catalog descriptors and scenario associations"
                            << Qt::endl;
    }
    return ok ? 0 : 1;
}
