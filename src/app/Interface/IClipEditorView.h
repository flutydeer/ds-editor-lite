//
// Created by fluty on 2024/7/10.
//

#ifndef ICLIPEDITORVIEW_H
#define ICLIPEDITORVIEW_H

#include "Utils/Macros.h"

LITE_INTERFACE IClipEditorView {
    I_DECL(IClipEditorView)
    // View state

    // View operations
    I_METHOD(void centerAt(double tick, double keyIndex));
    I_METHOD(void centerAt(double startTick, double length, double keyIndex));
};



#endif // ICLIPEDITORVIEW_H
