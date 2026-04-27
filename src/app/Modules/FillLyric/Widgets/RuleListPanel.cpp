#include "RuleListPanel.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "RuleListWidget.h"

namespace FillLyric
{
    RuleListPanel::RuleListPanel(QWidget *parent) : QWidget(parent) {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(4);

        // Toolbar
        auto *toolbar = new QHBoxLayout;
        toolbar->setSpacing(4);

        m_addBtn = new QPushButton(QStringLiteral("+"));
        m_addBtn->setObjectName("ruleToolBtn");
        m_addBtn->setFixedSize(28, 28);
        m_addBtn->setToolTip(tr("Add rule"));
        toolbar->addWidget(m_addBtn);

        m_removeBtn = new QPushButton(QStringLiteral("-"));
        m_removeBtn->setObjectName("ruleToolBtn");
        m_removeBtn->setFixedSize(28, 28);
        m_removeBtn->setToolTip(tr("Remove rule"));
        m_removeBtn->setEnabled(false);
        toolbar->addWidget(m_removeBtn);

        toolbar->addStretch();
        layout->addLayout(toolbar);

        // List
        m_listWidget = new RuleListWidget(this);
        layout->addWidget(m_listWidget, 1);

        connect(m_addBtn, &QPushButton::clicked, this, &RuleListPanel::addRequested);
        connect(m_removeBtn, &QPushButton::clicked, this, &RuleListPanel::removeRequested);
    }

    RuleListWidget *RuleListPanel::listWidget() const {
        return m_listWidget;
    }

    void RuleListPanel::setRemoveEnabled(bool enabled) {
        m_removeBtn->setEnabled(enabled);
    }

} // namespace FillLyric
