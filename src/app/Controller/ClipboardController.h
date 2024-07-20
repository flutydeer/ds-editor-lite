//
// Created by fluty on 24-2-17.
//

#ifndef CLIPBOARDCONTROLLER_H
#define CLIPBOARDCONTROLLER_H

#define clipboardController ClipboardController::instance()

#include "Utils/Singleton.h"

#include <QObject>

class ClipboardControllerPrivate;
class ClipboardController final : public QObject, public Singleton<ClipboardController> {
    Q_OBJECT

public:
    ClipboardController();

public slots:
    void copy();
    void cut();
    void paste();

private:
    Q_DECLARE_PRIVATE(ClipboardController)
    ClipboardControllerPrivate *d_ptr;
};



#endif // CLIPBOARDCONTROLLER_H
