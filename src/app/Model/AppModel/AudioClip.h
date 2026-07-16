//
// Created by fluty on 24-9-18.
//

#ifndef AUDIOCLIP_H
#define AUDIOCLIP_H

#include "AudioInfoModel.h"
#include "Clip.h"

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

signals:
    void pathChanged();
    void pathStatusChanged(PathStatus status);

private:
    QString m_path;
    AudioPathInfo m_pathInfo;
    PathStatus m_pathStatus = PathStatus::Normal;
    AudioInfoModel m_info;
};



#endif // AUDIOCLIP_H
