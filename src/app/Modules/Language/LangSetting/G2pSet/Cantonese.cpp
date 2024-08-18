#include "Cantonese.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace LangSetting {
    QWidget *CantoneseSet::configWidget(QJsonObject *config) {
        auto *widget = new QWidget();
        auto *layout = new QVBoxLayout();

        auto *toneCheckBox = new QCheckBox(tr("Tone"), widget);

        layout->addWidget(toneCheckBox);
        layout->addStretch(1);

        if (config && config->keys().contains("tone")) {
            toneCheckBox->setChecked(config->value("tone").toBool());
        } else {
            toneCheckBox->setChecked(false);
        }

        widget->setLayout(layout);

        connect(toneCheckBox, &QCheckBox::toggled, [this, config](const bool checked) {
            config->insert("tone", checked);
            Q_EMIT g2pConfigChanged();
        });
        return widget;
    }
} // LangSetting