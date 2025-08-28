//
// Created by fluty on 24-9-10.
//

#ifndef PHONEMES_H
#define PHONEMES_H

#include "Interface/ISerializable.h"

#include <QList>

class PhonemeNameSeq : public ISerializable {
public:
    QList<QString> original;
    QList<QString> edited;

    bool editedEqualsWith(const PhonemeNameSeq &other) const;
    bool isEdited() const;
    const QList<QString> &result() const;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class PhonemeNameInfo : public ISerializable {
public:
    PhonemeNameSeq ahead;
    PhonemeNameSeq normal;

    bool isEmpty() const;
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

class PhonemeOffsetInfo : public ISerializable {
public:
    PhonemeOffsetSeq ahead;
    PhonemeOffsetSeq normal;

    bool isEmpty() const;
    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class Phonemes : public ISerializable {
public:
    enum Type { Ahead, Normal };

    QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    PhonemeNameInfo nameInfo;
    PhonemeOffsetInfo offsetInfo;
};



#endif // PHONEMES_H
