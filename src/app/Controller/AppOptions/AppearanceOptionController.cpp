//
// Created by fluty on 24-3-13.
//

#include "AppearanceOptionController.h"
#include "Model/AppOptions/AppOptions.h"

AppearanceOptionController::AppearanceOptionController() {
    updateTemp();
}
void AppearanceOptionController::setOption(const AppearanceOption &option) {
    m_temp = option;
    emit modified();
}
void AppearanceOptionController::accept() {
    auto option = AppOptions::instance()->appearance();
    option->animationLevel = m_temp.animationLevel;
    option->animationTimeScale = m_temp.animationTimeScale;
}
void AppearanceOptionController::cancel() {
    updateTemp();
}
void AppearanceOptionController::update() {
    updateTemp();
}
void AppearanceOptionController::updateTemp() {
    auto option = AppOptions::instance()->appearance();
    m_temp = *option;
}