//
// Created by fluty on 24-9-10.
//

#ifndef PHONEMES_H
#define PHONEMES_H

#include "Interface/ISerializable.h"

#include <QList>

class PhonemeName : public ISerializable {
public:
    QString language;
    QString name;
    bool isOnset;

    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
    bool operator==(const PhonemeName &other) const;
};

class PhonemeNameSeq : public ISerializable {
public:
    QList<PhonemeName> original;
    QList<PhonemeName> edited;

    bool editedEqualsWith(const QList<PhonemeName> &other) const;
    bool isEdited() const;
    const QList<PhonemeName> &result() const;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class PhonemeOffsetSeq : public ISerializable {
public:
    QList<int> original;
    QList<int> edited;

    void clear();
    bool isEdited() const;
    const QList<int> &result() const;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class Phonemes : public ISerializable {
public:
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    PhonemeNameSeq nameSeq;
    PhonemeOffsetSeq offsetSeq;
};



#endif // PHONEMES_H
