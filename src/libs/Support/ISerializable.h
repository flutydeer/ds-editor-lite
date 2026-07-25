//
// Created by fluty on 24-9-10.
//

#ifndef ISERIALIZABLE_H
#define ISERIALIZABLE_H

#include <lite/Support/Macros.h>

#include <QJsonObject>

LITE_INTERFACE ISerializable {
    I_DECL(ISerializable)
    I_NODSCD(QJsonObject serialize() const);
    I_METHOD(bool deserialize(const QJsonObject &obj));
};

#endif // ISERIALIZABLE_H
