//
// Created by fluty on 24-3-19.
//

#include "Window.h"

#include "Utils/WindowFrameUtils.h"

Window::Window(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {
    WindowFrameUtils::applyFrameEffects(this);
}