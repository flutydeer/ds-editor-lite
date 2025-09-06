//
// Created by fluty on 24-2-17.
//

#ifndef CLIPBOARDCONTROLLER_H
#define CLIPBOARDCONTROLLER_H

#define clipboardController ClipboardController::instance()

#include "Utils/Singleton.h"

#include <QObject>

class ClipboardControllerPrivate;

class ClipboardController final : public QObject {
    Q_OBJECT

private:
    explicit ClipboardController(QObject *parent = nullptr);
    ~ClipboardController() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(ClipboardController)
    Q_DISABLE_COPY_MOVE(ClipboardController)

public slots:
    void copy();
    void cut();
    static void paste();

private:
    Q_DECLARE_PRIVATE(ClipboardController)
    ClipboardControllerPrivate *d_ptr;
};



#endif // CLIPBOARDCONTROLLER_H
