//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef TRACKSMODEL_H
#define TRACKSMODEL_H

#include <QObject>
#include <QString>
#include <QColor>

#include "Utils/UniqueObject.h"
#include "Utils/OverlapableSerialList.h"
#include "Utils/ISelectable.h"
#include "TrackControl.h"

class Clip;

class Track final : public QObject, public UniqueObject, public ISelectable {
    Q_OBJECT

public:
    ~Track() override;
    enum ClipChangeType { Inserted, Removed };

    [[nodiscard]] QString name() const;
    void setName(const QString &name);
    [[nodiscard]] TrackControl control() const;
    void setControl(const TrackControl &control);
    [[nodiscard]] OverlapableSerialList<Clip> clips() const;
    void insertClip(Clip *clip);
    void removeClip(Clip *clip);
    [[nodiscard]] QColor color() const;
    void setColor(const QColor &color);

    void notifyClipChanged(ClipChangeType type, Clip *clip);
    Clip *findClipById(int id);

    class TrackProperties {
    public:
        QString name;
        double gain = 1.0;
        double pan = 0;
        bool mute = false;
        bool solo = false;

        int index = 0;
    };

signals:
    void propertyChanged();
    void clipChanged(Track::ClipChangeType type, int id, Clip *clip);

private:
    QString m_name;
    TrackControl m_control = TrackControl();
    OverlapableSerialList<Clip> m_clips;
    QColor m_color;
};



#endif // TRACKSMODEL_H
