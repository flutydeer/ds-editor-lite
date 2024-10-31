#include "UnknownG2pSet.h"

namespace LangSetting {
    QWidget *UnknownG2pSet::configWidget(const QJsonObject &config, bool editable) {
        Q_UNUSED(config);
        Q_UNUSED(editable);
        return new QWidget();
    }

} // LangSetting