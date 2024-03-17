//
// Created by fluty on 24-3-18.
//

#include "OptionsCard.h"

#include <QLabel>
#include <QVBoxLayout>

#include "CardView.h"

OptionsCard::OptionsCard(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_lbTitle = new QLabel;
    m_lbTitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_lbTitle->setObjectName("title");

    m_card = new CardView;

    auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_lbTitle);
    mainLayout->addWidget(m_card);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins({});
    setLayout(mainLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}
void OptionsCard::setTitle(const QString &title) const {
    m_lbTitle->setText(title);
}
CardView *OptionsCard::card() {
    return m_card;
}