//
// Created by fluty on 24-8-14.
//

#include "Pronunciation.h"

#include <QJsonObject>

bool Pronunciation::isEdited() const {
    return !(edited.isNull() || edited.isEmpty());
}

QJsonObject Pronunciation::serialize(const Pronunciation &pronunciation) {
    QJsonObject objPronunciation;
    objPronunciation.insert("original", pronunciation.original);
    objPronunciation.insert("edited", pronunciation.edited);
    return objPronunciation;
}

QDataStream &operator<<(QDataStream &out, const Pronunciation &pronunciation) {
    out << pronunciation.original;
    out << pronunciation.edited;
    return out;
}

QDataStream &operator>>(QDataStream &in, Pronunciation &pronunciation) {
    in >> pronunciation.original;
    in >> pronunciation.edited;
    return in;
}