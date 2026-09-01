#include <lite/GUI/Controls/OptionsCard.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <lite/GUI/Controls/CardView.h>

OptionsCard::OptionsCard(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_lbTitle = new QLabel;
    m_lbTitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_lbTitle->setContentsMargins(10, 0, 0, 0);

    m_lbTitle->setObjectName("title");

    m_titleLayout = new QHBoxLayout;
    m_titleLayout->addWidget(m_lbTitle);
    m_titleLayout->addStretch();
    m_titleLayout->setContentsMargins(0, 0, 10, 0);
    m_titleLayout->setSpacing(6);

    m_card = new CardView;

    m_mainLayout = new QVBoxLayout;
    m_mainLayout->addLayout(m_titleLayout);
    m_mainLayout->addWidget(m_card);
    // No bottom margin on the card itself: card-to-card spacing belongs to
    // the outer layout (otherwise the last card floats above the page
    // bottom by its own margin).
    m_mainLayout->setContentsMargins({0, 0, 0, 0});
    m_mainLayout->setSpacing(6);
    setLayout(m_mainLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void OptionsCard::setTitle(const QString &title) const {
    m_lbTitle->setText(title);
}

void OptionsCard::setTitleVisible(const bool visible) const {
    m_lbTitle->setVisible(visible);
    m_titleLayout->setContentsMargins(0, 0, visible ? 10 : 0, 0);
    m_mainLayout->setSpacing(visible ? 6 : 0);
}

void OptionsCard::addTitleWidget(QWidget *widget) const {
    if (widget)
        m_titleLayout->addWidget(widget);
}

CardView *OptionsCard::card() const {
    return m_card;
}
