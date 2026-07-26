//
// Created by fluty on 24-9-18.
//

#ifndef AUDIOCLIP_H
#define AUDIOCLIP_H

#include <lite/ProjectModel/AppModel/AudioInfoModel.h>
#include <lite/ProjectModel/AppModel/Clip.h>

class Timeline;

// Portability locating info of an audio file, stored in workspace["diffscope"]["audio"] of the dspx
// absoluteDir and fileName are derived from AudioClip::path() and not duplicated here
struct AudioPathInfo {
    QString relativeDir; // path relative to the project file directory; non-empty only when the audio is inside it
    QString sha512;      // content digest (lowercase hex); empty means not computed yet
};

class AudioClip final : public Clip {
    Q_OBJECT

public:
    // Path resolution status (transient; not serialized, not in undo history)
    enum class PathStatus {
        Normal,     // file available
        Missing,    // file not found
        Unconfirmed // fallback search matched by file name, but no sha512 to verify against
    };

    class AudioClipProperties final : public ClipCommonProperties {
    public:
        AudioClipProperties() = default;
        explicit AudioClipProperties(const AudioClip &clip);
        explicit AudioClipProperties(const IClip &clip);
        QString path;
    };

    ClipType clipType() const override;
    QString path() const;
    void setPath(const QString &path);

    AudioPathInfo pathInfo() const;
    void setPathInfo(const AudioPathInfo &pathInfo);

    PathStatus pathStatus() const;
    void setPathStatus(PathStatus status);

    // TODO: 将峰值数据保存到其他地方
    const AudioInfoModel &audioInfo() const;
    void setAudioInfo(const AudioInfoModel &audioInfo);

    // Realtime anchoring (multi-tempo): the material trim offset and the audible
    // window length are anchored in real time and never stretch with the tempo
    // map. The visible start tick (start + clipStart) is the tick-anchored
    // position; all four tick fields are caches derived from it and the truth
    // below under the current timeline. The dspx file stores ticks only; the
    // truth is re-derived from ticks on load.
    double trimStartMs() const;
    double playLengthMs() const;
    double materialLengthMs() const;
    bool hasRealTimeAnchor() const;
    void setRealTimeAnchor(double trimStartMs, double playLengthMs, double materialLengthMs);
    // Derive the realtime truth from the current tick fields (load / tick-space edits)
    void syncTruthFromTicks(const Timeline &timeline);
    // Re-derive the tick caches from the truth, keeping the visible start tick
    // fixed. Returns true when any tick field changed. The caller owns list
    // reindexing (Track::removeClip / insertClip) and change notification.
    bool updateTicksFromTruth(const Timeline &timeline);
    // Fill the ms fields of tick-space properties under the given timeline
    static void deriveTruthForProperties(ClipCommonProperties &args, const Timeline &timeline);
    // Apply properties' truth (or adopt its ticks when no ms is carried) and
    // re-derive the tick caches; for undoable actions
    void applyRealTimeAnchorFromProperties(const ClipCommonProperties &args,
                                           const Timeline &timeline);

signals:
    void pathChanged();
    void pathStatusChanged(PathStatus status);

private:
    QString m_path;
    AudioPathInfo m_pathInfo;
    PathStatus m_pathStatus = PathStatus::Normal;
    AudioInfoModel m_info;
    double m_trimStartMs = 0;
    double m_playLengthMs = -1; // negative = truth not established yet
    double m_materialLengthMs = -1;
};



#endif // AUDIOCLIP_H
