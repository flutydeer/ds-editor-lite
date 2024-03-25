#include "English.h"

#include <QCheckBox>
#include <QVBoxLayout>

namespace G2pMgr {
    English::English(QObject *parent) : IG2pFactory("English", parent) {
        setAuthor(tr("Xiao Lang"));
        setDisplayName(tr("English"));
        setDescription(tr("Greedy matching of consecutive English letters."));
    }

    English::~English() = default;

    QList<LangNote> English::convert(const QStringList &input, const QJsonObject *config) const {
        const auto toLower = config && config->keys().contains("toLower")
                                 ? config->value("toLower").toBool()
                                 : this->toLower;

        QList<LangNote> result;
        for (auto &c : input) {
            const auto syllable = toLower ? c.toLower() : c;

            LangNote langNote;
            langNote.lyric = c;
            langNote.syllable = syllable;
            langNote.candidates = QStringList() << syllable;
            result.append(langNote);
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

        connect(toLowerCheckBox, &QCheckBox::toggled, [this, config](const bool checked) {
            config->insert("toLower", checked);
            Q_EMIT g2pConfigChanged();
        });
        return widget;
    }
} // G2pMgr