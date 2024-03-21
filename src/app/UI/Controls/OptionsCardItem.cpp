//
// Created by fluty on 24-3-18.
//

#include "OptionsCardItem.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>

OptionsCardItem::OptionsCardItem(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_checkBox = new QCheckBox;
    m_checkBox->setVisible(false);

    m_lbTitle = new QLabel;
    m_lbTitle->setObjectName("title");
    m_lbTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // m_lbTitle->setWordWrap(true);

    m_lbDesc = new QLabel;
    m_lbDesc->setObjectName("desc");
    m_lbDesc->setVisible(false);
    m_lbDesc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // TODO: word warp
    // m_lbDesc->setWordWrap(true);

    auto titleDescLayout = new QVBoxLayout;
    titleDescLayout->addWidget(m_lbTitle);
    titleDescLayout->addWidget(m_lbDesc);
    titleDescLayout->setSpacing(2);

    m_mainLayout = new QHBoxLayout;
    m_mainLayout->addWidget(m_checkBox);
    m_mainLayout->addLayout(titleDescLayout);
    m_mainLayout->addSpacerItem(new QSpacerItem(8, 4, QSizePolicy::Expanding));
    m_mainLayout->setContentsMargins(10, 5, 10, 5);
    setLayout(m_mainLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}
void OptionsCardItem::setCheckable(bool checkable) {
    m_checkBox->setVisible(checkable);
}
bool OptionsCardItem::isChecked() const {
    return m_checkBox->isChecked();
}
void OptionsCardItem::setChecked(bool checked) {
    m_checkBox->setChecked(checked);
}
void OptionsCardItem::setTitle(const QString &title) {
    m_lbTitle->setText(title);
}
void OptionsCardItem::setDescription(const QString &desc) {
    m_lbDesc->setVisible(true);
    m_lbDesc->setText(desc);
}
void OptionsCardItem::addWidget(QWidget *widget) {
    m_mainLayout->addWidget(widget);
}
void OptionsCardItem::removeWidget(QWidget *widget) {
    m_mainLayout->removeWidget(widget);
}