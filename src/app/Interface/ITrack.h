//
// Created by fluty on 24-7-25.
//

#ifndef ITRACK_H
#define ITRACK_H

#include "Model/AppModel/TrackControl.h"
#include "Utils/UniqueObject.h"
#include <lite/Support/Macros.h>

#include <QString>

LITE_INTERFACE ITrack : public UniqueObject {
    I_DECL(ITrack)
    ITrack() = default;
    explicit ITrack(const int id) : UniqueObject(id){}

    I_NODSCD(QString name() const);
    I_METHOD(void setName(const QString &name));
    I_NODSCD(TrackControl control() const);
    I_METHOD(void setControl(const TrackControl &control));
    I_NODSCD(int colorIndex() const);
    I_METHOD(void setColorIndex(int colorIndex));
};

#endif // ITRACK_H
