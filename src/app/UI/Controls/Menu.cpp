//
// Created by FlutyDeer on 2026/4/25.
//

#include "Menu.h"

#include "Utils/SystemUtils.h"

Menu::Menu(QWidget *parent) : CMenu(parent) {
    initUi();
}

Menu::Menu(const QString &title, QWidget *parent) : CMenu(title, parent) {
    initUi();
}

void Menu::initUi() {
#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11()) {
        setProperty("dwmBorder", true);
    }
#endif
}
