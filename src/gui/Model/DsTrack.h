//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef TRACKSMODEL_H
#define TRACKSMODEL_H

#include <QString>
#include <QObject>

#include "DsClip.h"
#include "DsTrackControl.h"
#include "Utils/UniqueObject.h"
#include "Utils/OverlapableSerialList.h"

class DsTrack : public QObject, public UniqueObject {
    Q_OBJECT

public:
    enum ClipChangeType { Insert, Remove };

    QString name() const;
    void setName(const QString &name);
    DsTrackControl control() const;
    void setControl(const DsTrackControl &control);
    OverlapableSerialList<DsClip> clips() const;
    void insertClip(DsClip *clip);
    void removeClip(DsClip *clip);
    DsClip *findClipById(int id);

    class TrackPropertyChangedArgs {
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
    void clipChanged(ClipChangeType type, int id, DsClip *clip);

    // Color color() const;
    // void setColor(const Color &color);

private:
    QString m_name;
    DsTrackControl m_control = DsTrackControl();
    OverlapableSerialList<DsClip> m_clips;
};



#endif // TRACKSMODEL_H
