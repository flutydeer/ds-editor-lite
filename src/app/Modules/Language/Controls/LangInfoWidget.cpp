#include "LangInfoWidget.h"

#include "Modules/Language/G2pMgr/IG2pManager.h"
#include "Modules/Language/LangMgr/ILanguageManager.h"

#include <QDebug>

namespace LangMgr {
    LangInfoWidget::LangInfoWidget(QWidget *parent) : QWidget(parent) {
        this->m_mainLayout = new QVBoxLayout();
        this->m_topLayout = new QVBoxLayout();
        this->m_authorLayout = new QHBoxLayout();

        this->m_label = new QLabel(tr("Language Config:"));

        this->m_languageLabel = new QLabel(tr("Language: "));
        this->m_authorLabel = new QLabel(tr("Author: "));
        this->m_authorLayout->addWidget(this->m_languageLabel);
        this->m_authorLayout->addStretch(1);
        this->m_authorLayout->addWidget(this->m_authorLabel);


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

    void LangInfoWidget::setInfo(const QString &id) const {
        const auto langMgr = ILanguageManager::instance();
        const auto config = langMgr->languageConfig(id);
        this->m_languageLabel->setText(tr("Language: ") + config.language);
        this->m_authorLabel->setText(tr("Author: ") + config.author);
        this->m_descriptionLabel->setText(config.description);

        this->removeWidget();
        this->m_mainLayout->addWidget(langMgr->language(id)->configWidget(), 1);

        Q_EMIT g2pSelected(id, langMgr->language(id)->selectedG2p());

        connect(langMgr->language(id), &ILanguageFactory::g2pChanged, this,
                [this, id](const QString &g2pId) { Q_EMIT g2pSelected(id, g2pId); });
    }
} // LangMgr