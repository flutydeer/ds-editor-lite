//
// Created by fluty on 24-2-19.
//

#include "Dialog.h"
#include "Utils/WindowFrameUtils.h"

Dialog::Dialog(QWidget *parent) : QDialog(parent) {
    this->setStyleSheet("QDialog { background: transparent; }");
    WindowFrameUtils::applyFrameEffects(this);
}