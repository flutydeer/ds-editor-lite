#include "LangInfoWidget.h"

namespace LangMgr {
    LangInfoWidget::LangInfoWidget(QWidget *parent) : QWidget(parent) {
        this->m_mainLayout = new QVBoxLayout(this);
        this->m_langueLabel = new QLabel(tr("Language: "));
        this->m_authorLabel = new QLabel(tr("Author: "));

        this->m_descriptionGroupBox = new QGroupBox(tr("Description "));
        this->m_descriptionLayout = new QVBoxLayout();
        this->m_descriptionLabel = new QLabel();
        this->m_descriptionLabel->setWordWrap(true);
        this->m_descriptionLayout->addWidget(this->m_descriptionLabel);

        this->m_descriptionGroupBox->setLayout(this->m_descriptionLayout);

        this->m_mainLayout->addWidget(this->m_langueLabel);
        this->m_mainLayout->addWidget(this->m_authorLabel);
        this->m_mainLayout->addWidget(this->m_descriptionGroupBox);
        this->m_mainLayout->addStretch(1);
    }

    LangInfoWidget::~LangInfoWidget() = default;

    void LangInfoWidget::setInfo(const LangConfig &config) const {
        this->m_langueLabel->setText(tr("Language: ") + config.language);
        this->m_authorLabel->setText(tr("Author: ") + config.author);
        this->m_descriptionLabel->setText(config.description);
    }
} // LangMgr