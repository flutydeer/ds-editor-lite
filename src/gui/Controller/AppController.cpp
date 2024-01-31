//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>

#include "AppController.h"

void AppController::openProject(const QString &filePath) {
    AppModel::instance()->loadAProject(filePath);
}