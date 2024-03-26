#include "Cantonese.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace LangSetting {
    QWidget *CantoneseSet::configWidget(QJsonObject *config) {
        auto *widget = new QWidget();
        auto *layout = new QVBoxLayout();

        auto *toneCheckBox = new QCheckBox(tr("Tone"), widget);
        auto *convertNumCheckBox = new QCheckBox(tr("Convert number"), widget);

        layout->addWidget(toneCheckBox);
        layout->addWidget(convertNumCheckBox);
        layout->addStretch(1);

        if (config && config->keys().contains("tone")) {
            toneCheckBox->setChecked(config->value("tone").toBool());
            convertNumCheckBox->setChecked(config->value("convertNum").toBool());
        } else {
            toneCheckBox->setChecked(d->tone());
            convertNumCheckBox->setChecked(d->convertNum());
        }

        widget->setLayout(layout);

        connect(toneCheckBox, &QCheckBox::toggled, [this, config](const bool checked) {
            config->insert("tone", checked);
            Q_EMIT g2pConfigChanged();
        });
        connect(convertNumCheckBox, &QCheckBox::toggled, [this, config](const bool checked) {
            config->insert("convertNum", checked);
            Q_EMIT g2pConfigChanged();
        });
        return widget;
    }
} // LangSetting