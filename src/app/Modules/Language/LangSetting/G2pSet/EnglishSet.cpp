#include "EnglishSet.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace LangSetting {
    QWidget *EnglishSet::configWidget(QJsonObject *config) {
        auto *widget = new QWidget();
        auto *layout = new QVBoxLayout();

        auto *toLowerCheckBox = new QCheckBox(tr("To lower"), widget);

        layout->addWidget(toLowerCheckBox);
        layout->addStretch(1);

        if (config && config->keys().contains("toLower")) {
            toLowerCheckBox->setChecked(config->value("toLower").toBool());
        } else {
            toLowerCheckBox->setChecked(false);
        }
        return widget;
    }
} // LangSetting