#include "InfoLaneHeaderView.h"

#include <QHBoxLayout>
#include <QLabel>

InfoLaneHeaderView::InfoLaneHeaderView(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);

    m_titleLabel = new QLabel;
    m_titleLabel->setObjectName("laneTitleLabel");

    const auto mainLayout = new QHBoxLayout;
    mainLayout->setContentsMargins(12, 0, 4, 0);
    mainLayout->setSpacing(4);
    mainLayout->addWidget(m_titleLabel);
    mainLayout->addStretch();
    setLayout(mainLayout);
}

void InfoLaneHeaderView::setTitle(const QString &title) {
    m_titleLabel->setText(title);
}
