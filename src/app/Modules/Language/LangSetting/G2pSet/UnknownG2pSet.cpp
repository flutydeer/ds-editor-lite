#include "UnknownG2pSet.h"

namespace LangSetting {
    QWidget *UnknownG2pSet::g2pConfigWidget(const QJsonObject &config) {
        Q_UNUSED(config);
        return new QWidget();
    }

} // LangSetting