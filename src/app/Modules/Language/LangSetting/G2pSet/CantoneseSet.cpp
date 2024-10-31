#include "CantoneseSet.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace LangSetting {
    QWidget *CantoneseSet::configWidget(const QJsonObject &config, bool editable) {
        auto *widget = new QWidget();
        auto *layout = new QVBoxLayout();

        auto *toneCheckBox = new QCheckBox(tr("Tone"), widget);
        toneCheckBox->setEnabled(editable);

        layout->addWidget(toneCheckBox);
        layout->addStretch(1);

        if (config.keys().contains("tone")) {
            toneCheckBox->setChecked(config.value("tone").toBool());
        } else {
            toneCheckBox->setChecked(false);
        }

        widget->setLayout(layout);

        connect(toneCheckBox, &QCheckBox::toggled, [this](const bool checked) {
            QJsonObject tempConfig = d->config();
            tempConfig.insert("tone", checked);
            d->loadConfig(tempConfig);
            Q_EMIT g2pConfigChanged();
        });
        return widget;
    }
} // LangSetting