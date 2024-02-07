//
// Created by fluty on 2024/2/7.
//

#ifndef PIANOROLLVIEWCONTROLLER_H
#define PIANOROLLVIEWCONTROLLER_H

#include <QObject>

#include "Utils/Singleton.h"

class PianoRollViewController final : public QObject, public Singleton<PianoRollViewController>{
    Q_OBJECT

// public slots:
//     void onAddNote(int start, int length, int keyIndex);
//     void onRemoveNote(int noteId);
};



#endif //PIANOROLLVIEWCONTROLLER_H
