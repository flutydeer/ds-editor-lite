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
    QString g2pId;
    Pronunciation pronunciation;
    PhonemeNameInfo phonemeNameInfo;
};



#endif // NOTEDIALOGRESULT_H
