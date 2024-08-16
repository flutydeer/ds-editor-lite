#include "ILangSetFactory.h"
#include "ILangSetFactory_p.h"
#include "G2pMgr/IG2pManager.h"
#include "LangMgr/ILanguageManager.h"

#include <QLabel>
#include <QJsonObject>
#include <QVBoxLayout>

#include "UI/Controls/ComboBox.h"
#include "UI/Controls/SwitchButton.h"
#include "UI/Controls/OptionsCardItem.h"

namespace LangSetting {

    ILangSetFactoryPrivate::ILangSetFactoryPrivate() {
    }

    ILangSetFactoryPrivate::~ILangSetFactoryPrivate() = default;

    void ILangSetFactoryPrivate::init() {
    }

    ILangSetFactory::ILangSetFactory(const QString &id, QObject *parent)
        : ILangSetFactory(*new ILangSetFactoryPrivate(), id, parent) {
    }

    ILangSetFactory::~ILangSetFactory() = default;

    ILangSetFactory::ILangSetFactory(ILangSetFactoryPrivate &d, const QString &id, QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
        d.id = id;

        d.init();
    }

    QString ILangSetFactory::id() const {
        Q_D(const ILangSetFactory);
        return d->id;
    }

    QWidget *ILangSetFactory::configWidget(const QString &langId) {
        Q_D(const ILangSetFactory);

        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto langFactory = langMgr->language(langId);

        auto *widget = new QWidget();
        const auto mainLayout = new QVBoxLayout(widget);

        const auto enabledItem = new OptionsCardItem();
        enabledItem->setTitle(tr("Enabled"));
        const auto enabledCheckBox = new SwitchButton(langFactory->enabled());
        enabledItem->addWidget(enabledCheckBox);

        const auto discardItem = new OptionsCardItem();
        discardItem->setTitle(tr("Discard result"));
        const auto discardResultCheckBox = new SwitchButton(langFactory->discardResult());
        discardItem->addWidget(discardResultCheckBox);

        const auto cateLayout = new QHBoxLayout();
        const auto cateLabel = new QLabel(tr("Analysis results "));
        const auto cateComboBox = new ComboBox();
        cateComboBox->setMinimumContentsLength(8);
        cateComboBox->setMinimumHeight(28);

        const auto languages = langMgr->languages();
        QStringList cateList;
        QStringList cateTrans;

        for (const auto &lang : languages) {
            cateList.append(lang->id());
            cateTrans.append(lang->displayName());
        }

        cateComboBox->addItems(cateTrans);
        if (cateList.contains(langFactory->category())) {
            cateComboBox->setCurrentText(cateTrans.at(cateList.indexOf(langFactory->category())));
        } else {
            cateComboBox->setCurrentText(tr("unknown"));
        }

        const auto g2pMgr = G2pMgr::IG2pManager::instance();

        const auto g2pLayout = new QHBoxLayout();
        const auto g2pLabel = new QLabel(tr("Subordinate G2p"));
        const auto g2pComboBox = new ComboBox();
        g2pComboBox->setMinimumContentsLength(8);
        g2pComboBox->setMinimumHeight(28);

        const auto g2ps = g2pMgr->g2ps();
        QStringList g2pList;
        QStringList g2pTrans;

        for (const auto &g2p : g2ps) {
            g2pList.append(g2p->id());
            g2pTrans.append(g2p->displayName());
        }

        g2pComboBox->addItems(g2pTrans);
        if (g2pList.contains(langFactory->selectedG2p())) {
            g2pComboBox->setCurrentText(g2pTrans.at(g2pList.indexOf(langFactory->selectedG2p())));
        } else {
            g2pComboBox->setCurrentText(tr("unknown"));
        }

        mainLayout->addWidget(enabledItem);
        mainLayout->addWidget(discardItem);

        cateLayout->addWidget(cateLabel);
        cateLayout->addWidget(cateComboBox);
        cateLayout->addStretch();
        mainLayout->addLayout(cateLayout);

        g2pLayout->addWidget(g2pLabel);
        g2pLayout->addWidget(g2pComboBox);
        g2pLayout->addStretch();
        mainLayout->addLayout(g2pLayout);

        widget->setLayout(mainLayout);

        connect(enabledCheckBox, &SwitchButton::clicked, [this, langFactory](const bool &checked) {
            langFactory->setEnabled(checked);
            Q_EMIT langConfigChanged(id());
        });

        connect(discardResultCheckBox, &SwitchButton::clicked,
                [this, langFactory](const bool &checked) {
                    langFactory->setDiscardResult(checked);
                    Q_EMIT langConfigChanged(id());
                });

        connect(cateComboBox, &QComboBox::currentTextChanged,
                [this, cateList, cateTrans, langFactory](const QString &text) {
                    const auto index = cateTrans.indexOf(text);
                    langFactory->setCategory(index >= 0 ? cateList.at(index) : tr("unknown"));
                    Q_EMIT langConfigChanged(id());
                });

        connect(g2pComboBox, &QComboBox::currentTextChanged,
                [this, g2pList, g2pTrans, langFactory](const QString &text) {
                    const auto index = g2pTrans.indexOf(text);
                    langFactory->setG2p(index >= 0 ? g2pList.at(index) : tr("unknown"));
                    Q_EMIT g2pChanged(langFactory->selectedG2p());
                    Q_EMIT langConfigChanged(id());
                });
        return widget;
    }
}