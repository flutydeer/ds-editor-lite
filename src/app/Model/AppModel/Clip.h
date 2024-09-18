//
// Created by fluty on 2024/1/27.
//

#ifndef DSCLIP_H
#define DSCLIP_H

#include "Params.h"
#include "Global/AppGlobal.h"
#include "Interface/IClip.h"
#include "Model/AppModel/AudioInfoModel.h"
#include "Model/ClipboardDataModel/NotesParamsInfo.h"
#include "Utils/Overlappable.h"
#include "Utils/OverlappableSerialList.h"
#include "Utils/Property.h"

class DrawCurve;
class Params;
class Note;
class InferPiece;

class Clip : public QObject, public IClip, public Overlappable {
    Q_OBJECT

public:
    ~Clip() override = default;

    [[nodiscard]] ClipType clipType() const override {
        return Generic;
    }

    [[nodiscard]] QString name() const override;
    void setName(const QString &text) override;
    [[nodiscard]] int start() const override;
    void setStart(int start) override;
    [[nodiscard]] int length() const override;
    void setLength(int length) override;
    [[nodiscard]] int clipStart() const override;
    void setClipStart(int clipStart) override;
    [[nodiscard]] int clipLen() const override;
    void setClipLen(int clipLen) override;
    [[nodiscard]] double gain() const override;
    void setGain(double gain) override;
    [[nodiscard]] bool mute() const override;
    void setMute(bool mute) override;
    void notifyPropertyChanged();

    [[nodiscard]] int endTick() const;

    int compareTo(const Clip *obj) const;
    bool isOverlappedWith(Clip *obj) const;
    [[nodiscard]] std::tuple<qsizetype, qsizetype> interval() const override;

    class ClipCommonProperties {
    public:
        ClipCommonProperties() = default;
        explicit ClipCommonProperties(const IClip &clip);
        virtual ~ClipCommonProperties() = default;
        int id = -1;

        QString name;
        int start = 0;
        int length = 0;
        int clipStart = 0;
        int clipLen = 0;
        double gain = 0;
        bool mute = false;
    };

signals:
    void propertyChanged();

protected:
    QString m_name;
    int m_start = 0;
    int m_length = 0;
    int m_clipStart = 0;
    int m_clipLen = 0;
    double m_gain = 0;
    bool m_mute = false;

    static void applyPropertiesFromClip(ClipCommonProperties &args, const IClip &clip);
};

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

    ~AudioClip() override;

    [[nodiscard]] ClipType clipType() const override {
        return Audio;
    }

    [[nodiscard]] QString path() const {
        return m_path;
    }

    void setPath(const QString &path) {
        m_path = path;
        emit propertyChanged();
    }

    [[nodiscard]] const AudioInfoModel &audioInfo() const {
        return m_info;
    }

    void setAudioInfo(const AudioInfoModel &audioInfo) {
        m_info = audioInfo;
        emit propertyChanged();
    }

private:
    QString m_path;
    AudioInfoModel m_info;
};

#endif // DSCLIP_H
