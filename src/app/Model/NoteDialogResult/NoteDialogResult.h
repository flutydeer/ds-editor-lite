//
// Created by fluty on 24-8-5.
//

#ifndef NOTEDIALOGRESULT_H
#define NOTEDIALOGRESULT_H

#include <QString>

class NoteDialogResult {
public:
    QString language;
    QString lyric;
    Pronunciation pronunciation;
    PhonemeInfo phonemes;
};



#endif //NOTEDIALOGRESULT_H
