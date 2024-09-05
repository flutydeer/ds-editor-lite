//
// Created by fluty on 24-7-25.
//

#ifndef ICLIP_H
#define ICLIP_H

#include "Utils/Macros.h"
#include "Utils/UniqueObject.h"

#include <QString>

interface IClip : public UniqueObject {
    I_DECL(IClip)

    enum ClipType { Audio, Singing, Generic };

    IClip() = default;
    explicit IClip(int id) : UniqueObject(id){};

    I_NODSCD(ClipType clipType() const);
    I_NODSCD(QString name() const);
    I_METHOD(void setName(const QString &text));
    I_NODSCD(int start() const);
    I_METHOD(void setStart(int start));
    I_NODSCD(int length() const);
    I_METHOD(void setLength(int length));
    I_NODSCD(int clipStart() const);
    I_METHOD(void setClipStart(int clipStart));
    I_NODSCD(int clipLen() const);
    I_METHOD(void setClipLen(int clipLen));
    I_NODSCD(double gain() const);
    I_METHOD(void setGain(double gain));
    I_NODSCD(bool mute() const);
    I_METHOD(void setMute(bool mute));
};

#endif // ICLIP_H
