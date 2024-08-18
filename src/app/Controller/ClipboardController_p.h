//
// Created by fluty on 24-7-21.
//

#ifndef CLIPBOARDCONTROLLER_P_H
#define CLIPBOARDCONTROLLER_P_H

#include "Global/ControllerGlobal.h"

class ClipboardController;

class ClipboardControllerPrivate {
    Q_DECLARE_PUBLIC(ClipboardController)

public:
    explicit ClipboardControllerPrivate(ClipboardController *q) : q_ptr(q) {
    }

    static void copyCutSelectedItems(ControllerGlobal::ElemType type, bool isCut);
    static void copyCutNoteWithParams(bool isCut);

private:
    ClipboardController *q_ptr;
};

#endif // CLIPBOARDCONTROLLER_P_H
