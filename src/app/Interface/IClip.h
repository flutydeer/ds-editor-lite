//
// Created by fluty on 24-7-25.
//

#ifndef ICLIP_H
#define ICLIP_H

#include "Utils/UniqueObject.h"

#include <QString>

class IClip : public UniqueObject {
public:
    enum ClipType { Audio, Singing, Generic };

    IClip() = default;
    explicit IClip(int id) : UniqueObject(id) {
    }
    virtual ~IClip() = default;
    [[nodiscard]] virtual ClipType clipType() const = 0;
    [[nodiscard]] virtual QString name() const = 0;
    virtual void setName(const QString &text) = 0;
    [[nodiscard]] virtual int start() const = 0;
    virtual void setStart(int start) = 0;
    [[nodiscard]] virtual int length() const = 0;
    virtual void setLength(int length) = 0;
    [[nodiscard]] virtual int clipStart() const = 0;
    virtual void setClipStart(int clipStart) = 0;
    [[nodiscard]] virtual int clipLen() const = 0;
    virtual void setClipLen(int clipLen) = 0;
    [[nodiscard]] virtual double gain() const = 0;
    virtual void setGain(double gain) = 0;
    [[nodiscard]] virtual bool mute() const = 0;
    virtual void setMute(bool mute) = 0;
};

#endif // ICLIP_H
