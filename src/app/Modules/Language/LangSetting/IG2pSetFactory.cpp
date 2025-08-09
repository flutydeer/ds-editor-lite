#include "IG2pSetFactory.h"
#include "IG2pSetFactory_p.h"

#include <QCheckBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace LangSetting {

    IG2pSetFactoryPrivate::IG2pSetFactoryPrivate() {
    }

    IG2pSetFactoryPrivate::~IG2pSetFactoryPrivate() = default;

    void IG2pSetFactoryPrivate::init() {
    }

    IG2pSetFactory::IG2pSetFactory(const QString &id, QObject *parent)
        : IG2pSetFactory(*new IG2pSetFactoryPrivate(), id, parent) {
    }

    IG2pSetFactory::~IG2pSetFactory() = default;

    IG2pSetFactory::IG2pSetFactory(IG2pSetFactoryPrivate &d, const QString &id, QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
        d.id = id;

        d.init();
    }

    QString IG2pSetFactory::id() const {
        Q_D(const IG2pSetFactory);
        return d->id;
    }

    QString IG2pSetFactory::displayName() const {
        Q_D(const IG2pSetFactory);
        return d->displayName;
    }

    void IG2pSetFactory::setDisplayName(const QString &displayName) {
        Q_D(IG2pSetFactory);
        d->displayName = displayName;
    }

    QString IG2pSetFactory::author() const {
        Q_D(const IG2pSetFactory);
        return d->author;
    }

    void IG2pSetFactory::setAuthor(const QString &author) {
        Q_D(IG2pSetFactory);
        d->author = author;
    }

    QString IG2pSetFactory::description() const {
        Q_D(const IG2pSetFactory);
        return d->description;
    }

    void IG2pSetFactory::setDescription(const QString &description) {
        Q_D(IG2pSetFactory);
        d->description = description;
    }

    QJsonObject IG2pSetFactory::config() {
        return {};
    }

    QWidget *IG2pSetFactory::langConfigWidget(const QJsonObject &config) {
        auto *widget = new QWidget();
        auto *mainlayout = new QVBoxLayout();
        widget->setLayout(mainlayout);
        if (config.contains("languageConfig")) {
            const QJsonObject languageConfig = config.value("languageConfig").toObject();
            m_languageConfigState = languageConfig;

            for (auto it = languageConfig.constBegin(); it != languageConfig.constEnd(); ++it) {
                const QString key = it.key();
                QJsonObject valueObj = it.value().toObject();

                const bool discardResult = valueObj.value("discardResult").toBool(false);
                const bool enabled = valueObj.value("enabled").toBool(true);

                auto *enableCheckBox = new QCheckBox(tr("enabled"));
                enableCheckBox->setChecked(enabled);

                auto *hlayout2 = new QHBoxLayout();
                hlayout2->addStretch();
                hlayout2->addWidget(enableCheckBox);

                auto *discardCheckBox = new QCheckBox(tr("discardResult"));
                discardCheckBox->setChecked(discardResult);

                auto *hlayout3 = new QHBoxLayout();
                hlayout3->addStretch();
                hlayout3->addWidget(discardCheckBox);

                auto *label = new QLabel(key);

                auto *hlayout = new QHBoxLayout();
                hlayout->addWidget(label);
                hlayout->addLayout(hlayout2);
                hlayout->addLayout(hlayout3);

                mainlayout->addLayout(hlayout);

                // 初始化 languageConfigState，确保每个 key 的初始值正确
                m_languageConfigState.insert(key, valueObj);

                // Lambda function to update JSON based on checkbox state
                auto updateJson = [this, enableCheckBox, discardCheckBox, key] {
                    // 更新 languageConfigState 中对应项的状态
                    QJsonObject langConfig;
                    langConfig.insert("discardResult", discardCheckBox->isChecked());
                    langConfig.insert("enabled", enableCheckBox->isChecked());
                    m_languageConfigState.insert(key, langConfig);

                    // Emit the signal with the complete JSON string
                    emit langConfigChanged(m_languageConfigState);
                };

                // Connect checkStateChanged (Qt >= 6.7) or stateChanged (Qt < 6.7)
                // signals to the updateJson lambda function
                constexpr auto kStateChangedSignal =
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
                    &QCheckBox::checkStateChanged;
#else
                    &QCheckBox::stateChanged;
#endif
                QObject::connect(enableCheckBox, kStateChangedSignal, updateJson);
                QObject::connect(discardCheckBox, kStateChangedSignal, updateJson);
            }
        } else {
            qDebug() << "Key 'languageConfig' does not exist.";
        }
        return widget;
    }

    QWidget *IG2pSetFactory::g2pConfigWidget(const QJsonObject &config) {
        Q_UNUSED(config);
        return new QWidget();
    }
}
