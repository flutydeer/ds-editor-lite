//
// Created by fluty on 24-8-14.
//

#ifndef PHONEMEINFO_H
#define PHONEMEINFO_H

#include "Phoneme.h"

#include <QJsonObject>
#include <QList>

class PhonemeInfo {
public:
    enum PhonemeType { Original, Edited };

    QList<Phoneme> original;
    QList<Phoneme> edited;

    PhonemeInfo() = default;
    PhonemeInfo(QList<Phoneme> original, QList<Phoneme> edited)
        : original(std::move(original)), edited(std::move(edited)){};

    bool isEdited() const;

    friend QDataStream &operator<<(QDataStream &out, const PhonemeInfo &phonemes);
    friend QDataStream &operator>>(QDataStream &in, PhonemeInfo &phonemes);

    static QJsonObject serialize(const PhonemeInfo &phonemes);
};



#endif //PHONEMEINFO_H
