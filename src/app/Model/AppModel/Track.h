//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef TRACKSMODEL_H
#define TRACKSMODEL_H

#include <QString>

#include "Utils/OverlappableSerialList.h"
#include "TrackControl.h"
#include "Interface/ISerializable.h"
#include "Interface/ITrack.h"
#include "Modules/Inference/Models/SingerIdentifier.h"
#include "Modules/PackageManager/Models/SingerInfo.h"

class Clip;

class Track final : public QObject, public ITrack, public ISerializable {
    Q_OBJECT
public:
    explicit Track() = default;
    explicit Track(const QString &name, const QList<Clip *> &clips);
    ~Track() override;

    enum ClipChangeType { Inserted, Removed };

    QString name() const override;
    void setName(const QString &name) override;

    QColor color() const override;
    void setColor(const QColor &color) override;

    TrackControl control() const override;
    void setControl(const TrackControl &control) override;

    OverlappableSerialList<Clip> clips() const;
    void insertClip(Clip *clip);
    void insertClips(const QList<Clip *> &clips);
    void removeClip(Clip *clip);

    QString defaultLanguage() const;
    void setDefaultLanguage(const QString &language);
    QString defaultG2pId() const;

    SingerInfo singerInfo() const;
    void setSingerInfo(const SingerInfo &singerInfo);
    SingerIdentifier singerIdentifier() const;

    QString speakerId() const;
    SpeakerInfo speakerInfo() const;
    void setSpeakerInfo(const SpeakerInfo &speakerInfo);

    void notifyClipChanged(ClipChangeType type, Clip *clip);
    Clip *findClipById(int id) const;

    class TrackProperties {
    public:
        explicit TrackProperties() = default;
        explicit TrackProperties(const ITrack &track);

        int id = -1;
        QString name;
        double gain = 1.0;
        double pan = 0;
        bool mute = false;
        bool solo = false;
    };

    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

signals:
    void propertyChanged();
    void clipChanged(Track::ClipChangeType type, Clip *clip);
    void singerChanged(const SingerInfo &singerInfo);
    void speakerChanged(const SpeakerInfo &speaker);

private:
    void updateDefaultG2pId(const QString &language);

    QString m_name;
    TrackControl m_control = TrackControl();
    OverlappableSerialList<Clip> m_clips;
    QColor m_color;

    QString m_defaultLanguage = "unknown";
    QString m_defaultG2pId = "unknown";

    SingerInfo m_singerInfo;
    SpeakerInfo m_speakerInfo;
};


#endif // TRACKSMODEL_H