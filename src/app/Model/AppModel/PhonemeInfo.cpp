//
// Created by fluty on 24-8-14.
//

#include "PhonemeInfo.h"

#include <QJsonArray>

bool PhonemeInfo::isEdited() const {
    return !edited.isEmpty();
}
QDataStream &operator<<(QDataStream &out, const PhonemeInfo &phonemes) {
    auto serialize = [](QDataStream &out, const QList<Phoneme> &phonemes) {
        out << phonemes.count();
        for (const auto &phoneme : phonemes) {
            out << phoneme.type;
            out << phoneme.name;
            out << phoneme.start;
        }
    };
    serialize(out, phonemes.original);
    serialize(out, phonemes.edited);
    return out;
}
QDataStream &operator>>(QDataStream &in, PhonemeInfo &phonemes) {
    auto deserialize = [](QDataStream &in, QList<Phoneme> &phonemes) {
        int count;
        in >> count;
        for (int i = 0; i < count; i++) {
            Phoneme phoneme;
            in >> phoneme.type;
            in >> phoneme.name;
            in >> phoneme.start;
            phonemes.append(phoneme);
        }
    };
    deserialize(in, phonemes.original);
    deserialize(in, phonemes.edited);
    return in;
}
QJsonObject PhonemeInfo::serialize(const PhonemeInfo &phonemes) {
    QJsonObject objPhonemes;
    auto serializePhoneme = [](QJsonArray &array, const QList<Phoneme> &phonemes) {
        for (const auto &phoneme : phonemes) {
            QJsonObject objPhoneme;
            objPhoneme.insert("type", phoneme.type);
            objPhoneme.insert("name", phoneme.name);
            objPhoneme.insert("start", phoneme.start);
            array.append(objPhoneme);
        }
    };
    QJsonArray arrOriginal;
    QJsonArray arrEdited;
    serializePhoneme(arrOriginal, phonemes.original);
    serializePhoneme(arrEdited, phonemes.edited);
    objPhonemes.insert("original", arrOriginal);
    objPhonemes.insert("edited", arrEdited);
    return objPhonemes;
}