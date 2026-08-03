#include <lite/GUI/Controls/OptionListCard.h>

#include <lite/GUI/Controls/CardView.h>
#include <lite/GUI/Controls/DividerLine.h>
#include <lite/GUI/Controls/OptionsCardItem.h>

#include <QVBoxLayout>
#include <utility>

OptionListCard::OptionListCard(QWidget *parent) : OptionsCard(parent) {
    initUi();
}

OptionListCard::OptionListCard(QString title, QWidget *parent)
    : OptionsCard(parent), m_title(std::move(title)) {
    initUi();
}

OptionsCardItem *OptionListCard::addItem(OptionsCardItem *item) {
    DividerLine *divider = nullptr;
    if (m_itemCount > 0) {
        divider = new DividerLine(Qt::Horizontal);
        divider->setObjectName("optionListCardDivider");
        m_cardLayout->addWidget(divider);
    }
    item->setContentsMargins(10, 0, 10, 0);
    m_cardLayout->addWidget(item);
    m_itemDividers.insert(item, divider);
    m_itemCount++;
    return item;
}

OptionsCardItem *OptionListCard::addItem(const QString &title, QWidget *control) {
    const auto item = new OptionsCardItem;
    item->setTitle(title);
    item->addWidget(control);
    return addItem(item);
}

OptionsCardItem *OptionListCard::addItem(const QString &title, const QString &description) {
    const auto item = new OptionsCardItem;
    item->setTitle(title);
    item->setDescription(description);
    return addItem(item);
}

OptionsCardItem *OptionListCard::addItem(const QString &title, const QList<QWidget *> &controls) {
    const auto item = new OptionsCardItem;
    item->setTitle(title);
    for (const auto control : controls)
        item->addWidget(control);
    return addItem(item);
}

OptionsCardItem *OptionListCard::addItem(const QString &title, const QString &description,
                                         QWidget *control) {
    const auto item = new OptionsCardItem;
    item->setTitle(title);
    item->setDescription(description);
    item->addWidget(control);
    return addItem(item);
}

OptionsCardItem *OptionListCard::addItem(const QString &title, const QString &description,
                                         const QList<QWidget *> &controls) {
    const auto item = new OptionsCardItem;
    item->setTitle(title);
    item->setDescription(description);
    for (const auto control : controls)
        item->addWidget(control);
    return addItem(item);
}

void OptionListCard::setItemVisible(OptionsCardItem *item, bool visible) {
    const auto divider = m_itemDividers.constFind(item);
    if (divider == m_itemDividers.cend())
        return;

    if (*divider)
        (*divider)->setVisible(visible);
    item->setVisible(visible);
}

void OptionListCard::initUi() {
    setAttribute(Qt::WA_StyledBackground);

    m_cardLayout = new QVBoxLayout;
    m_cardLayout->setContentsMargins(0, 5, 0, 5);
    m_cardLayout->setSpacing(0);
    card()->setLayout(m_cardLayout);

    setTitle(m_title);
}
