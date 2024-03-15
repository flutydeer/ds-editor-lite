//
// Created by fluty on 24-3-13.
//

#include "AppOptionsController.h"

#include "Model/AppOptions/AppOptions.h"

AppOptionsController::AppOptionsController() {
    connect(&m_appearanceController, &IOptionsController::modified, this,
            &AppOptionsController::onSubOptionModified);
}
AppearanceOptionController *AppOptionsController::appearanceController() {
    return &m_appearanceController;
}
bool AppOptionsController::apply() {
    qDebug() << "AppOptionsController::apply";
    m_appearanceController.accept();
    bool saved = AppOptions::instance()->save();
    AppOptions::instance()->notifyOptionsChanged();
    if (saved) {
        emit canApplyChanged(false);
        return true;
    }
    return false;
}
void AppOptionsController::cancel() {
    m_appearanceController.cancel();
}
void AppOptionsController::onSubOptionModified() {
    m_modified = true;
    emit canApplyChanged(true);
}