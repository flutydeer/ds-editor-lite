#include "UnknownG2pSet.h"

namespace LangSetting {
    QWidget *UnknownG2pSet::configWidget(QJsonObject *config) {
        Q_UNUSED(config);
        return new QWidget();
    }

} // LangSetting