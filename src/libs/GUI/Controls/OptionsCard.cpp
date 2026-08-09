#include <lite/GUI/Controls/OptionsCard.h>

#include <QLabel>
#include <QVBoxLayout>

#include <lite/GUI/Controls/CardView.h>

OptionsCard::OptionsCard(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_lbTitle = new QLabel;
    m_lbTitle->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_lbTitle->setContentsMargins(10, 0, 0, 0);

    m_lbTitle->setObjectName("title");

    m_card = new CardView;

    const auto mainLayout = new QVBoxLayout;
    mainLayout->addWidget(m_lbTitle);
    mainLayout->addWidget(m_card);
    // No bottom margin on the card itself: card-to-card spacing belongs to
    // the outer layout (otherwise the last card floats above the page
    // bottom by its own margin).
    mainLayout->setContentsMargins({0, 0, 0, 0});
    mainLayout->setSpacing(6);
    setLayout(mainLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void OptionsCard::setTitle(const QString &title) const {
    m_lbTitle->setText(title);
}

CardView *OptionsCard::card() const {
    return m_card;
}