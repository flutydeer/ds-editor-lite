//
// Created by fluty on 2024/2/7.
//

#ifndef NOTE_H
#define NOTE_H

#include <QString>

class Note {
public:
    int start = 0;
    int length = 480;
    int keyIndex = 60;
    QString lyric;
    QString pronunciation;
};



#endif //NOTE_H
