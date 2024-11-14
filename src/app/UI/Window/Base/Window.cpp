//
// Created by fluty on 24-3-19.
//

#include "Window.h"

#include "UI/Utils/ThemeManager.h"

Window::Window(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {
    ThemeManager::instance()->addWindow(this);
}

Window::~Window() {
    ThemeManager::instance()->removeWindow(this);
}