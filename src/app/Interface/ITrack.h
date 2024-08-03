//
// Created by fluty on 24-7-25.
//

#ifndef ITRACK_H
#define ITRACK_H

#include "Model/AppModel/TrackControl.h"
#include "Utils/UniqueObject.h"

#include <QColor>
#include <QString>

class ITrack : public UniqueObject {
public:
    ITrack() = default;
    explicit ITrack(int id) : UniqueObject(id) {
    }
    virtual ~ITrack() = default;
    [[nodiscard]] virtual QString name() const = 0;
    virtual void setName(const QString &name) = 0;
    [[nodiscard]] virtual TrackControl control() const = 0;
    virtual void setControl(const TrackControl &control) = 0;
    [[nodiscard]] virtual QColor color() const = 0;
    virtual void setColor(const QColor &color) = 0;
};

#endif // ITRACK_H
