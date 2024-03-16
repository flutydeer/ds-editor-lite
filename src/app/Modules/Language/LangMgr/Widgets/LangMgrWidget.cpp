#include "LangMgrWidget.h"

namespace LangMgr {
    LangMgrWidget::LangMgrWidget(QWidget *parent) : QWidget(parent) {
        m_mainLayout = new QHBoxLayout(this);
        m_mainLayout->setContentsMargins(0, 0, 0, 0);

        m_tableLayout = new QVBoxLayout();
        m_tableLayout->setContentsMargins(0, 0, 0, 0);

        m_langTableWidget = new LangTableWidget(this);

        m_buttonLayout = new QHBoxLayout();
        m_buttonLayout->setContentsMargins(0, 0, 0, 0);
        m_applyButton = new Button(tr("Apply"), this);
        m_buttonLayout->addStretch(1);
        m_buttonLayout->addWidget(m_applyButton);

        m_tableLayout->addWidget(m_langTableWidget);
        m_tableLayout->addLayout(m_buttonLayout);

        m_labelLayout = new QVBoxLayout();
        m_labelLayout->setContentsMargins(0, 0, 0, 0);
        const auto text =
            tr("The original text is analyzed by various language analyzers to identify the "
               "corresponding language in sequence;\n\n"
               "If a certain analyzer is disabled in the first column, it will not participate in "
               "word segmentation;\n\n"
               "If \"Discard Result\" is checked, the results of word segmentation and analysis "
               "will not enter the notes.");
        m_label = new QLabel(this);
        m_label->setText(text);
        m_label->setWordWrap(true);
        m_labelLayout->addWidget(m_label);
        m_labelLayout->addStretch(1);

        m_mainLayout->addLayout(m_tableLayout, 3);
        m_mainLayout->addLayout(m_labelLayout, 1);
    }

    LangMgrWidget::~LangMgrWidget() = default;
} // LangMgr