#include "LangInfoWidget.h"

#include "Modules/Language/LangMgr/ILanguageManager.h"

namespace LangMgr {
    LangInfoWidget::LangInfoWidget(QWidget *parent) : QWidget(parent) {
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

    LangInfoWidget::~LangInfoWidget() = default;

    void LangInfoWidget::setInfo(const QString &id) const {
        const auto langMgr = ILanguageManager::instance();
        const auto config = langMgr->languageConfig(id);
        this->m_langueLabel->setText(tr("Language: ") + config.language);
        this->m_authorLabel->setText(tr("Author: ") + config.author);
        this->m_descriptionLabel->setText(config.description);
        // 删除m_mainLayout中的所有widget
        QLayoutItem *child;
        while ((child = this->m_mainLayout->takeAt(1)) != nullptr) {
            delete child->widget();
            delete child;
        }
        this->m_mainLayout->addWidget(langMgr->language(id)->configWidget(), 1);
    }
} // LangMgr