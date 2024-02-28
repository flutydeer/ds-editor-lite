//
// Created by fluty on 24-2-17.
//

#ifndef CLIPBOARDCONTROLLER_H
#define CLIPBOARDCONTROLLER_H

#include <QObject>

#include "Utils/Singleton.h"
#include "Global/ControllerGlobal.h"

class ClipboardController final : public QObject, public Singleton<ClipboardController> {
    Q_OBJECT

public slots:
    void copy();
    void cut();
    void paste();

private:
    static void copyCutSelectedItems(ControllerGlobal::ElemType type, bool isCut);
    static void copyCutNoteWithParams(bool isCut);
};



#endif // CLIPBOARDCONTROLLER_H
