//
// Created by FlutyDeer on 2023/12/1.
//

#ifndef TRACKSMODEL_H
#define TRACKSMODEL_H

#include <QString>
#include <QColor>

#include "DsClip.h"
#include "DsTrackControl.h"
#include "Utils/UniqueObject.h"
#include "Utils/OverlapableSerialList.h"

class DsTrack : public QObject, public UniqueObject {
    Q_OBJECT

public:
    enum ClipChangeType { Inserted, PropertyChanged , Removed };

    QString name() const;
    void setName(const QString &name);
    DsTrackControl control() const;
    void setControl(const DsTrackControl &control);
    OverlapableSerialList<DsClip> clips() const;
    void insertClip(DsClip *clip);
    void removeClip(DsClip *clip);
    QColor color() const;
    void setColor(const QColor &color);

    // void updateClip(DsClip *clip);
    void removeClipQuietly(DsClip *clip);
    void insertClipQuietly(DsClip *clip);
    void notityClipPropertyChanged(DsClip *clip);

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

private:
    QString m_name;
    DsTrackControl m_control = DsTrackControl();
    OverlapableSerialList<DsClip> m_clips;
    QColor m_color;
};



#endif // TRACKSMODEL_H
