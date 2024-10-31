#include "EnglishSet.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace LangSetting {
    QWidget *EnglishSet::configWidget(const QJsonObject &config, bool editable) {
        auto *widget = new QWidget();
        auto *layout = new QVBoxLayout();

        auto *toLowerCheckBox = new QCheckBox(tr("To lower"), widget);
        toLowerCheckBox->setEnabled(editable);

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
            d->loadConfig(tempConfig);
            Q_EMIT g2pConfigChanged();
        });
        return widget;
    }
} // LangSetting