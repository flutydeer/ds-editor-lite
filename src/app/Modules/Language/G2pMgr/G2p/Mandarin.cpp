#include "Mandarin.h"

#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>

namespace G2pMgr {
    Mandarin::Mandarin(QObject *parent) : IG2pFactory("Mandarin", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("Mandarin"));
        setDescription(tr("Using Pinyin as the phonetic notation method."));
    }

    Mandarin::~Mandarin() = default;

    bool Mandarin::initialize(QString &errMsg) {
        m_mandarin = new IKg2p::Mandarin();
        if (m_mandarin->getDefaultPinyin("好").isEmpty()) {
            errMsg = tr("Failed to initialize Mandarin G2P");
            return false;
        }
        return true;
    }

    QList<Phonic> Mandarin::convert(const QStringList &input, const QJsonObject *config) const {
        const auto tone =
            config && config->keys().contains("tone") ? config->value("tone").toBool() : this->tone;
        const auto convertNum = config && config->keys().contains("convertNum")
                                    ? config->value("convertNum").toBool()
                                    : this->convertNum;

        QList<Phonic> result;
        auto g2pRes = m_mandarin->hanziToPinyin(input, tone, convertNum);
        for (int i = 0; i < g2pRes.size(); i++) {
            Phonic phonic;
            phonic.text = input[i];
            phonic.pronunciation = Pronunciation(g2pRes[i], QString());
            phonic.candidates = m_mandarin->getDefaultPinyin(input[i], false);
            if (input[i] == g2pRes[i])
                phonic.g2pError = true;
            result.append(phonic);
        }
        return result;
    }

    QJsonObject Mandarin::config() {
        QJsonObject config;
        config["tone"] = tone;
        config["convertNum"] = convertNum;
        return config;
    }

    QWidget *Mandarin::configWidget(QJsonObject *config) {
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
            toneCheckBox->setChecked(tone);
            convertNumCheckBox->setChecked(convertNum);
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
} // G2pMgr