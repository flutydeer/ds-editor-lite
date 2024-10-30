//
// Created by fluty on 24-10-31.
//

#include "OptionListCard.h"

#include "CardView.h"
#include "DividerLine.h"
#include "OptionsCardItem.h"

#include <QVBoxLayout>
#include <utility>

OptionListCard::OptionListCard(QWidget *parent) : OptionsCard(parent) {
    initUi();
}

OptionListCard::OptionListCard(QString title, QWidget *parent)
    : OptionsCard(parent), m_title(std::move(title)) {
    initUi();
}

void OptionListCard::addItem(OptionsCardItem *item) {
    if (m_itemCount > 0)
        m_cardLayout->addWidget(new DividerLine(Qt::Horizontal));
    m_cardLayout->addWidget(item);
    m_itemCount++;
}

void OptionListCard::addItem(const QString &title, QWidget *control) {
    auto item = new OptionsCardItem;
    item->setTitle(title);
    item->addWidget(control);
    addItem(item);
}

void OptionListCard::addItem(const QString &title, const QList<QWidget *> &controls) {
    auto item = new OptionsCardItem;
    item->setTitle(title);
    for (const auto control : controls)
        item->addWidget(control);
    addItem(item);
}

void OptionListCard::addItem(const QString &title, const QString &description, QWidget *control) {
    auto item = new OptionsCardItem;
    item->setTitle(title);
    item->setDescription(description);
    item->addWidget(control);
    addItem(item);
}

void OptionListCard::addItem(const QString &title, const QString &description,
                             const QList<QWidget *> &controls) {
    auto item = new OptionsCardItem;
    item->setTitle(title);
    item->setDescription(description);
    for (const auto control : controls)
        item->addWidget(control);
    addItem(item);
}

void OptionListCard::initUi() {
    setAttribute(Qt::WA_StyledBackground);

    m_cardLayout = new QVBoxLayout;
    m_cardLayout->setContentsMargins(10, 5, 10, 5);
    m_cardLayout->setSpacing(0);
    card()->setLayout(m_cardLayout);

    setTitle(m_title);
}