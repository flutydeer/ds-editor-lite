#pragma once

#include <QJsonObject>

class AppModel;

namespace TestSupport {
    QJsonObject projectSnapshot(const AppModel &model);
}
