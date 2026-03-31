//
// Created by fluty on 24-8-5.
//

#ifndef NOTEDIALOGRESULT_H
#define NOTEDIALOGRESULT_H

#include "PhonemeNameItemModel.h"

#include <QString>

class NoteDialogResult {
public:
    QString language;
    QString lyric;
    Pronunciation pronunciation;
    PhonemeNameInfo phonemeNameInfo;
    QList<PhonemeNameItemModel> phonemeNames;
};



#endif // NOTEDIALOGRESULT_H
