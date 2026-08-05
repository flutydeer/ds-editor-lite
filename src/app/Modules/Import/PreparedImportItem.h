#ifndef DS_EDITOR_LITE_PREPAREDIMPORTITEM_H
#define DS_EDITOR_LITE_PREPAREDIMPORTITEM_H

#include <QJsonObject>
#include <QString>

#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectModel/AppModel/AudioInfoModel.h>

// File kind classification shared by all external drop/import entry points.
enum class ExternalFileKind { Unsupported, Project, Midi, Audio };

// A fully prepared audio clip ready to be committed to the model. Preparing
// never touches AppModel or the history stack.
struct PreparedAudioItem {
    QString path;
    // Format data in the workspace JSON shape the model expects
    // ({"userData": ..., "entryClassName": ...}).
    QJsonObject workspace;
    AudioInfoModel audioInfo;
    // Real-time duration derived from the decoded sample rate and frame count.
    double durationMs = 0.0;
};

// One item of a drag/import batch. A batch is prepared first (no model
// mutation), then committed as a single history entry (Phase 4).
struct PreparedImportItem {
    enum class Kind { Audio, Midi, Failed };
    Kind kind = Kind::Failed;
    QString path;
    QString errorMessage;    // Set when kind == Failed.
    PreparedAudioItem audio; // Set when kind == Audio.
    MidiParseData midi;      // Set when kind == Midi (parsed, not yet generated).
};

#endif // DS_EDITOR_LITE_PREPAREDIMPORTITEM_H
