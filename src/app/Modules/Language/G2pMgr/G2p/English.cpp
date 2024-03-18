#include "English.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace G2pMgr {
    English::English(QObject *parent) : IG2pFactory("English", parent) {
    }

    English::~English() = default;

    QList<Phonic> English::convert(QStringList &input) const {
        QList<Phonic> result;
        for (auto &c : input) {
            const auto syllable = toLower ? c.toLower() : c;

            Phonic phonic;
            phonic.text = c;
            phonic.pronunciation = Pronunciation(syllable, QString());
            phonic.candidates = QStringList() << syllable;
            result.append(phonic);
        }
        return result;
    }

    QJsonObject English::config() {
        QJsonObject config;
        config["toLower"] = toLower;
        return config;
    }

    QWidget *English::configWidget(QJsonObject *config) {
        auto *widget = new QWidget();
        auto *layout = new QVBoxLayout();

        auto *toLowerCheckBox = new QCheckBox(tr("To lower"), widget);

        layout->addWidget(toLowerCheckBox);
        layout->addStretch(1);

        if (config && config->keys().contains("toLower")) {
            toLowerCheckBox->setChecked(config->value("toLower").toBool());
        } else {
            toLowerCheckBox->setChecked(toLower);
        }

        widget->setLayout(layout);

        connect(toLowerCheckBox, &QCheckBox::toggled,
                [this, config](const bool checked) { config->insert("toLower", checked); });
        return widget;
    }
} // G2pMgr