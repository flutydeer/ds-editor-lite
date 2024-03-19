#include "Cantonese.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace G2pMgr {
    Cantonese::Cantonese(QObject *parent) : IG2pFactory("Cantonese", parent) {
    }

    Cantonese::~Cantonese() = default;

    bool Cantonese::initialize(QString &errMsg) {
        m_cantonese = new IKg2p::Cantonese();
        if (m_cantonese->getDefaultPinyin("好").isEmpty()) {
            errMsg = "Failed to initialize Cantonese G2P";
            return false;
        }
        return true;
    }

    QList<Phonic> Cantonese::convert(QStringList &input) const {
        QList<Phonic> result;
        auto g2pRes = m_cantonese->hanziToPinyin(input, tone, convertNum);
        for (int i = 0; i < g2pRes.size(); i++) {
            Phonic phonic;
            phonic.text = input[i];
            phonic.pronunciation = Pronunciation(g2pRes[i], QString());
            phonic.candidates = m_cantonese->getDefaultPinyin(input[i], false);
            result.append(phonic);
        }
        return result;
    }

    QJsonObject Cantonese::config() {
        QJsonObject config;
        config["tone"] = tone;
        config["convertNum"] = convertNum;
        return config;
    }

    QWidget *Cantonese::configWidget(QJsonObject *config) {
        auto *widget = new QWidget();
        auto *layout = new QVBoxLayout(widget);

        auto *toneCheckBox = new QCheckBox(tr("Tone"), widget);
        auto *convertNumCheckBox = new QCheckBox(tr("Convert number"), widget);

        layout->addWidget(toneCheckBox);
        layout->addWidget(convertNumCheckBox);
        layout->addStretch(1);

        if (config && config->keys().contains("tone")) {
            toneCheckBox->setChecked(config->value("tone").toBool());
            convertNumCheckBox->setChecked(config->value("convertNum").toBool());
        } else {
            toneCheckBox->setChecked(tone);
            convertNumCheckBox->setChecked(convertNum);
        }

        connect(toneCheckBox, &QCheckBox::toggled,
                [this, config](const bool checked) { config->insert("tone", checked); });
        connect(convertNumCheckBox, &QCheckBox::toggled,
                [this, config](const bool checked) { config->insert("convertNum", checked); });
        return widget;
    }
} // G2pMgr