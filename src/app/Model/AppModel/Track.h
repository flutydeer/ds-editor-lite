//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef TRACKSMODEL_H
#define TRACKSMODEL_H

#include <QObject>
#include <QString>

#include "Utils/OverlappableSerialList.h"
#include "Utils/ISelectable.h"
#include "TrackControl.h"
#include "Global/AppGlobal.h"
#include "Interface/ITrack.h"

class Clip;

class Track final : public QObject, public ITrack, public ISelectable {
    Q_OBJECT

public:
    ~Track() override;
    enum ClipChangeType { Inserted, Removed };

    [[nodiscard]] QString name() const override;
    void setName(const QString &name) override;
    [[nodiscard]] TrackControl control() const override;
    void setControl(const TrackControl &control) override;
    [[nodiscard]] OverlappableSerialList<Clip> clips() const;
    void insertClip(Clip *clip);
    void removeClip(Clip *clip);
    [[nodiscard]] QColor color() const override;
    void setColor(const QColor &color) override;
    [[nodiscard]] AppGlobal::LanguageType defaultLanguage() const;
    void setDefaultLanguage(AppGlobal::LanguageType language);

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

signals:
    void propertyChanged();
    void clipChanged(Track::ClipChangeType type, Clip *clip);

private:
    QString m_name;
    TrackControl m_control = TrackControl();
    OverlappableSerialList<Clip> m_clips;
    QColor m_color;
    AppGlobal::LanguageType m_defaultLanguage = AppGlobal::Unknown;
};



#endif // TRACKSMODEL_H
