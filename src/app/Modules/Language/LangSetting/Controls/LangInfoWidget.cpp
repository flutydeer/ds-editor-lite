#include "LangInfoWidget.h"

#include "Modules/Language/LangSetting/ILangSetManager.h"

#include <G2pMgr/IG2pManager.h>
#include <LangMgr/ILanguageManager.h>

#include <QDebug>

namespace LangSetting {
    LangInfoWidget::LangInfoWidget(QWidget *parent) : QWidget(parent) {
        this->setContentsMargins(0, 0, 0, 0);

        this->m_mainLayout = new QVBoxLayout();
        this->m_mainLayout->setContentsMargins(0, 0, 0, 0);
        this->m_topLayout = new QVBoxLayout();
        this->m_authorLayout = new QHBoxLayout();

        this->m_label = new QLabel(tr("Language Config"));

        this->m_languageLabel = new QLabel(tr("Language: "));
        this->m_authorLabel = new QLabel(tr("Author: "));
        this->m_authorLayout->addWidget(this->m_languageLabel);
        this->m_authorLayout->addStretch();
        this->m_authorLayout->addWidget(this->m_authorLabel);
        this->m_authorLayout->addStretch();

        this->m_descriptionGroupBox = new QGroupBox(tr("Description "));
        this->m_descriptionLayout = new QVBoxLayout();
        this->m_descriptionLabel = new QLabel();
        this->m_descriptionLabel->setWordWrap(true);
        this->m_descriptionLayout->addWidget(this->m_descriptionLabel);
        this->m_descriptionLayout->setContentsMargins(0, 0, 0, 0);

        this->m_descriptionGroupBox->setLayout(this->m_descriptionLayout);

        this->m_topLayout->addWidget(this->m_label);
        this->m_topLayout->addLayout(this->m_authorLayout);
        this->m_topLayout->addWidget(this->m_descriptionGroupBox);
        this->m_topLayout->addStretch(1);

        this->m_mainLayout->addLayout(this->m_topLayout, 1);
        this->setLayout(this->m_mainLayout);
    }

    LangInfoWidget::~LangInfoWidget() = default;

    void LangInfoWidget::removeWidget() const {
        QLayoutItem *child;
        while ((child = this->m_mainLayout->takeAt(1)) != nullptr) {
            delete child->widget();
            delete child;
        }
    }

    void LangInfoWidget::setInfo(const QString &langId) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto langSetFactory = LangSetting::ILangSetManager::instance()->langSet(langId);
        const auto langFactory = langMgr->language(langId);
        this->m_languageLabel->setText(tr("Language: ") + langFactory->displayName());
        this->m_authorLabel->setText(tr("Author: ") + langFactory->author());
        this->m_descriptionLabel->setText(langFactory->description());

        this->removeWidget();
        this->m_mainLayout->addWidget(langSetFactory->configWidget(langId), 1);

        Q_EMIT g2pSelected(langId, langFactory->selectedG2p());

        connect(langSetFactory, &LangSetting::ILangSetFactory::g2pChanged, this,
                [this, langId](const QString &g2pId) {
                    Q_EMIT g2pSelected(langId, g2pId);
                });

        connect(langSetFactory, &LangSetting::ILangSetFactory::langConfigChanged, this,
                [this] {
                    Q_EMIT langConfigChanged();
                });
    }
} // LangMgr