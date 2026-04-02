//
// Created by fluty on 24-10-7.
//

#ifndef PHONEMENAMERESULT_H
#define PHONEMENAMERESULT_H

#include "Model/AppModel/Phonemes.h"
#include <QStringList>

class PhonemeNameResult {
public:
    bool success = false;
    QList<PhonemeName> phonemeNames;
};

#endif //PHONEMENAMERESULT_H
