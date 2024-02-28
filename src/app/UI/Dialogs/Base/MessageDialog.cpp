//
// Created by fluty on 24-2-20.
//

#include "MessageDialog.h"
#include "Utils/WindowFrameUtils.h"

MessageDialog::MessageDialog(QWidget *parent) : QMessageBox(parent) {
    WindowFrameUtils::applyFrameEffects(this);
}