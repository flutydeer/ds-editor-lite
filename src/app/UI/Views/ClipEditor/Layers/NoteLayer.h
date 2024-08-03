//
// Created by fluty on 24-3-7.
//

#ifndef NOTELAYER_H
#define NOTELAYER_H

#include "UI/Views/Common/CommonGraphicsLayer.h"

class NoteView;

class NoteLayer : public CommonGraphicsLayer {
public:
    [[nodiscard]] QList<NoteView *> noteItems() const;
    [[nodiscard]] NoteView *findNoteById(int id) const;
};



#endif //NOTELAYER_H
