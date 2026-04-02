//
// Created by fluty on 24-8-5.
//

#ifndef NOTEDIALOGRESULT_H
#define NOTEDIALOGRESULT_H

#include "Model/AppModel/Phonemes.h"
#include "Model/AppModel/Pronunciation.h"

#include <QString>

class NoteDialogResult {
public:
    QString language;
    QString lyric;
    Pronunciation pronunciation;
    PhonemeNameSeq phonemeNameSeq;
    bool isPhonemeNameEdited = false;
};



#endif // NOTEDIALOGRESULT_H
