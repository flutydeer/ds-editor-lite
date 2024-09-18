//
// Created by fluty on 24-9-18.
//

#ifndef AUDIOCLIP_H
#define AUDIOCLIP_H

#include "AudioInfoModel.h"
#include "Clip.h"

class AudioClip final : public Clip {
    Q_OBJECT

public:
    class AudioClipProperties final : public ClipCommonProperties {
    public:
        AudioClipProperties() = default;
        explicit AudioClipProperties(const AudioClip &clip);
        explicit AudioClipProperties(const IClip &clip);
        QString path;
    };

    [[nodiscard]] ClipType clipType() const override;
    [[nodiscard]] QString path() const;
    void setPath(const QString &path);

    // TODO: 将峰值数据保存到其他地方
    [[nodiscard]] const AudioInfoModel &audioInfo() const;
    void setAudioInfo(const AudioInfoModel &audioInfo);

private:
    QString m_path;
    AudioInfoModel m_info;
};



#endif //AUDIOCLIP_H
