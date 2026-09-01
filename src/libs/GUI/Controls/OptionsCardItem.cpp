#include <lite/GUI/Controls/OptionsCardItem.h>

#include <QHBoxLayout>
#include <QLabel>

OptionsCardItem::OptionsCardItem(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_lbTitle = new QLabel;
    m_lbTitle->setObjectName("title");
    m_lbTitle->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_lbDesc = new QLabel;
    m_lbDesc->setObjectName("desc");
    m_lbDesc->setVisible(false);
    m_lbDesc->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_lbDesc->setWordWrap(true);

    m_textLayout = new QVBoxLayout;
    m_textLayout->addWidget(m_lbTitle);
    m_textLayout->addWidget(m_lbDesc);
    m_textLayout->setSpacing(2);

    m_mainLayout = new QHBoxLayout;
    m_mainLayout->addLayout(m_textLayout, 1);
    m_mainLayout->setContentsMargins(0, 3, 0, 3);
    setLayout(m_mainLayout);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void OptionsCardItem::setTitle(const QString &title) const {
    m_lbTitle->setText(title);
}

void OptionsCardItem::setDescription(const QString &desc) const {
    m_lbDesc->setText(desc);
    m_lbDesc->setVisible(!desc.isEmpty());
}

void OptionsCardItem::setTextAreaAlignment(const Qt::Alignment alignment) const {
    m_mainLayout->setAlignment(m_textLayout, alignment);
}

void OptionsCardItem::addWidget(QWidget *widget) const {
    m_mainLayout->addWidget(widget);
    m_lbTitle->setBuddy(widget);
}

void OptionsCardItem::removeWidget(QWidget *widget) const {
    m_mainLayout->removeWidget(widget);
}
