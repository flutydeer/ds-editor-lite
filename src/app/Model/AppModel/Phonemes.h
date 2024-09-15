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

    [[nodiscard]] bool editedEqualsWith(const PhonemeNameSeq &other) const;
    [[nodiscard]] bool isEdited() const;
    [[nodiscard]] const QList<QString> &result() const;
    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class PhonemeNameInfo : public ISerializable {
public:
    PhonemeNameSeq ahead;
    PhonemeNameSeq normal;
    PhonemeNameSeq final;

    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class PhonemeOffsetSeq : public ISerializable {
public:
    QList<int> original;
    QList<int> edited;

    void clear();
    [[nodiscard]] bool isEdited() const;
    [[nodiscard]] const QList<int> &result() const;
    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class PhonemeOffsetInfo : public ISerializable {
public:
    PhonemeOffsetSeq ahead;
    PhonemeOffsetSeq normal;
    PhonemeOffsetSeq final;

    [[nodiscard]] bool isEmpty() const;
    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;
};

class Phonemes : public ISerializable {
public:
    enum Type { Ahead, Normal, Final };

    [[nodiscard]] QJsonObject serialize() const override;
    bool deserialize(const QJsonObject &obj) override;

    PhonemeNameInfo nameInfo;
    PhonemeOffsetInfo offsetInfo;
};



#endif // PHONEMES_H
