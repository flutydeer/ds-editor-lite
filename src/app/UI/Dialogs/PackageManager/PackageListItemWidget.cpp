//
// Created by FlutyDeer on 2025/7/31.
//

#include "PackageListItemWidget.h"

#include <QLabel>
#include <QVBoxLayout>

PackageListItemWidget::PackageListItemWidget(QWidget *parent): QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 4, 6, 4);

    m_idLabel = new QLabel;

    m_vendorLabel = new QLabel;
    m_vendorLabel->setStyleSheet("color: gray;");

    layout->addWidget(m_idLabel);
    layout->addWidget(m_vendorLabel);
    layout->setSpacing(4);
    layout->setContentsMargins(8,6,8,7);

    setFixedHeight(48);
}

void PackageListItemWidget::setContent(const QString &id, const QString &vendor) {
    m_idLabel->setText(id);
    m_vendorLabel->setText(vendor);
}

QSize PackageListItemWidget::sizeHint() const {
    // 基础高度 + 文本行高度 + 边距
    return QSize(100, 48);  // remove
}