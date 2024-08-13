//
// Created by fluty on 24-8-14.
//

#ifndef PHONEME_H
#define PHONEME_H

#include <QList>
#include <QString>

class Phoneme {
public:
    enum PhonemeType { Ahead, Normal, Final };

    Phoneme() = default;
    Phoneme(PhonemeType type, QString name, int start)
        : type(type), name(std::move(name)), start(start) {
    }

    static QList<PhonemeType> phonemesTypes() {
        return {Ahead, Normal, Final};
    }
    PhonemeType type = Normal;
    QString name;
    int start = 0;
};

#endif //PHONEME_H
