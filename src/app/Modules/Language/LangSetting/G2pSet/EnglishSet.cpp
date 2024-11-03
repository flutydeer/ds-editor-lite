#include "EnglishSet.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace LangSetting {
    QWidget *EnglishSet::g2pConfigWidget(const QJsonObject &config) {
        auto *widget = new QWidget();
        auto *layout = new QVBoxLayout();

        auto *toLowerCheckBox = new QCheckBox(tr("To lower"), widget);

        layout->addWidget(toLowerCheckBox);
        layout->addStretch(1);

        if (config.keys().contains("toLower")) {
            toLowerCheckBox->setChecked(config.value("toLower").toBool());
        } else {
            toLowerCheckBox->setChecked(false);
        }

        connect(toLowerCheckBox, &QCheckBox::toggled, [this](const bool checked) {
            QJsonObject tempConfig = d->config();
            tempConfig.insert("toLower", checked);
            d->loadG2pConfig(tempConfig);
            Q_EMIT g2pConfigChanged();
        });
        return widget;
    }
} // LangSetting