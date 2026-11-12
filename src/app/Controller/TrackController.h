#ifndef TRACKCONTROLLER_H
#define TRACKCONTROLLER_H

#define trackController TrackController::instance()

#include "Model/ClipboardDataModel/ClipsInfo.h"
#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include "Tasks/DecodeAudioTask.h"
#include <lite/Core/Singleton.h>

#include <QObject>


class SingingClip;
class AudioClip;
class ComputeAudioHashTask;
class QWidget;

namespace talcs {
    class AbstractAudioFormatIO;
}

// Final geometry of one clip after an arrangement gesture. `trackIndex` is the target row, or -1 to
// keep the clip on the track it currently belongs to.
struct ClipGeometryEdit {
    int clipId = -1;
    int trackIndex = -1;
    Clip::ClipCommonProperties properties;
};

class TrackController final : public QObject {
    Q_OBJECT

private:
    explicit TrackController(QObject *parent = nullptr);
    ~TrackController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(TrackController)
    Q_DISABLE_COPY_MOVE(TrackController)

public:
    void setParentWidget(QWidget *view);

public slots:
    static void onNewTrack();
    static void onInsertNewTrack(qsizetype index);
    static void onAppendTrack(Track *track);
    static void onRemoveTrack(int id);
    static void onMoveTrack(qsizetype fromIndex, qsizetype toIndex);
    static void addAudioClipToNewTrack(const QString &filePath);
    static void setActiveClip(int clipId);
    static void setSelectedTrackIndex(int trackIndex);
    static void setSelectedClips(const QList<int> &clipIds);
    static void changeTrackProperty(const Track::TrackProperties &args);
    static void changeTrackColor(int trackId, int colorIndex);
    static void changeTrackDefaultLanguage(int trackId, const QString &language);
    void onAddAudioClip(const QString &path, talcs::AbstractAudioFormatIO *io,
                        const QJsonObject &workspace, int id, int tick);
    void onRelocateAudioClip(int clipId, const QString &path, talcs::AbstractAudioFormatIO *io,
                             const QJsonObject &workspace);
    // User confirms a "matched by name but no hash to verify" result: normalize status and compute
    // sha512. Not undoable: it only promotes the transient path status and backfills the digest, no
    // project content changes (the path was already adopted at load)
    static void confirmAudioClipPath(int clipId);
    static void onClipPropertyChanged(const Clip::ClipCommonProperties &args);
    static void onClipPropertyChanged(const Clip::ClipCommonProperties &args, int newTrackIndex);
    static void onRemoveClips(const QList<int> &clipsId);
    // Commits one arrangement gesture over the whole selection as a single history entry. Both
    // entry points ignore clips that disappeared between the gesture and the commit, and skip the
    // command entirely when nothing is left to edit.
    static void onMoveClips(const QList<ClipGeometryEdit> &edits);
    static void onResizeClips(const QList<ClipGeometryEdit> &edits);
    // Moves the current clip selection by whole steps and clamps it so no clip leaves the timeline
    // or the track range. Both deltas are in ticks and track rows.
    static void nudgeSelectedClips(int deltaTicks, int deltaTracks);
    static SingingClip *onNewSingingClip(int trackIndex, int tick);
    static void copySelectedClips();
    static void cutSelectedClips();
    // `tick` must already be snapped by the caller (view layer applies the
    // current WYSIWYG grid step; global paste passes the playhead tick as-is).
    static void pasteClips(const ClipsInfo &info, int tick, int trackIndex);
    // `tick` must already be snapped by the caller; the facade rejects a tick
    // outside the clip's visible range.
    static void onSplitClip(int clipId, int tick);

    // Schedules the sha512/audio-hash computation for an audio clip (used
    // after import/relink so the project tracks the file).
    static void scheduleHashUpdate(const AudioClip *clip);

private:
    void handleDecodeAudioTaskFinished(DecodeAudioTask *task);
    void handleComputeAudioHashTaskFinished(ComputeAudioHashTask *task);

    // TODO: refactor
    QWidget *m_parentWidget = nullptr;
};

#endif // TRACKCONTROLLER_H
