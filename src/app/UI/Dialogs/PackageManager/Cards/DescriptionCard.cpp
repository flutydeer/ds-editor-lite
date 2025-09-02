//
// Created by FlutyDeer on 2025/9/3.
//

#include "DescriptionCard.h"

#include "UI/Controls/CardView.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>

DescriptionCard::DescriptionCard(QWidget *parent) : OptionsCard(parent) {
    setAttribute(Qt::WA_StyledBackground);

    // plainTextEdit = new QPlainTextEdit;
    // plainTextEdit->setObjectName("DescriptionCardPlainTextEdit");
    // plainTextEdit->setReadOnly(true);
    // plainTextEdit->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    // plainTextEdit->setStyleSheet("background: transparent; ");

    lbDescription = new QLabel;
    lbDescription->setObjectName("lbDescription");
    lbDescription->setWordWrap(true);

    auto layout = new QHBoxLayout;
    // layout->addWidget(plainTextEdit);
    layout->addWidget(lbDescription);
    layout->setContentsMargins({16, 16, 16, 16});
    layout->setSpacing(0);

    card()->setLayout(layout);

    setTitle(tr("Description"));
}

void DescriptionCard::onDataContextChanged(const QString &dataContext) {
    // plainTextEdit->setPlainText(dataContext);
    lbDescription->setText(dataContext);
}