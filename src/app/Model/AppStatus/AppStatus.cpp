//
// Created by OrangeCat on 24-8-24.
//

#include "AppStatus.h"

AppStatus::AppStatus() {
    languageModuleStatus.setNotifyCallback([=](const ModuleStatus value) {
        emit moduleStatusChanged(ModuleType::Language, value);
    });

    projectEditableLength.setNotifyCallback([=](const int value) {
        emit projectEditableLengthChanged(value);
    });
}