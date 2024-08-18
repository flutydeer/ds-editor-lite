//
// Created by fluty on 2024/7/10.
//

#ifndef ICLIPEDITORVIEW_H
#define ICLIPEDITORVIEW_H

class IClipEditorView {

public:
    virtual ~IClipEditorView() = default;

    // View state


    // View operations
    virtual void centerAt(double tick, double keyIndex) = 0;
    virtual void centerAt(double startTick, double length, double keyIndex) = 0;
};



#endif // ICLIPEDITORVIEW_H
