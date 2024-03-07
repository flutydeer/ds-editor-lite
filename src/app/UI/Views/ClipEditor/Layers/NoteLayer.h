//
// Created by fluty on 24-3-7.
//

#ifndef NOTELAYER_H
#define NOTELAYER_H

#include "UI/Views/Common/CommonGraphicsLayer.h"

class NoteGraphicsItem;

class NoteLayer : public CommonGraphicsLayer {
public:
    [[nodiscard]] QList<NoteGraphicsItem *> noteItems() const;
    NoteGraphicsItem *findNoteById(int id);
    void updateOverlappedState();
};



#endif //NOTELAYER_H
