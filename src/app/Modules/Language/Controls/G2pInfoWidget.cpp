#include "G2pInfoWidget.h"

#include "Modules/Language/G2pMgr/IG2pManager.h"
#include "Modules/Language/LangMgr/ILanguageManager.h"

namespace LangMgr {
    G2pInfoWidget::G2pInfoWidget(QWidget *parent) : QWidget(parent) {
        this->m_mainLayout = new QVBoxLayout();
        this->m_topLayout = new QVBoxLayout();

        this->m_langueLabel = new QLabel(tr("Language: "));
        this->m_authorLabel = new QLabel(tr("Author: "));

        this->m_descriptionGroupBox = new QGroupBox(tr("Description "));
        this->m_descriptionLayout = new QVBoxLayout();
        this->m_descriptionLabel = new QLabel();
        this->m_descriptionLabel->setWordWrap(true);
        this->m_descriptionLayout->addWidget(this->m_descriptionLabel);

        this->m_descriptionGroupBox->setLayout(this->m_descriptionLayout);

        this->m_topLayout->addWidget(this->m_langueLabel);
        this->m_topLayout->addWidget(this->m_authorLabel);
        this->m_topLayout->addWidget(this->m_descriptionGroupBox);
        this->m_topLayout->addStretch(1);

        this->m_mainLayout->addLayout(this->m_topLayout, 1);
        this->setLayout(this->m_mainLayout);
    }

    G2pInfoWidget::~G2pInfoWidget() = default;

    void G2pInfoWidget::removeWidget() const {
        QLayoutItem *child;
        while ((child = this->m_mainLayout->takeAt(1)) != nullptr) {
            delete child->widget();
            delete child;
        }
    }

    void G2pInfoWidget::setInfo(const QString &language, const QString &g2pId) const {
        const auto langMgr = ILanguageManager::instance()->language(language);
        const auto g2pFactory = G2pMgr::IG2pManager::instance()->g2p(g2pId);

        m_langueLabel->setText(tr("Language: ") + g2pId);
        m_authorLabel->setText(tr("Author: ") + g2pId);
        m_descriptionLabel->setText(g2pId);

        removeWidget();
        this->m_mainLayout->addWidget(g2pFactory->configWidget(langMgr->g2pConfig()), 1);
    }
} // LangMgr