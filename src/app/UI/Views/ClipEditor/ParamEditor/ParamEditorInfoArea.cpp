//
// Created by fluty on 24-10-13.
//

#include "ParamEditorInfoArea.h"

#include <QLabel>
#include <QVBoxLayout>

ParamEditorInfoArea::ParamEditorInfoArea(QWidget *parent): QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    auto lbMax = new QLabel("0 dB");
    lbMax->setAlignment(Qt::AlignRight);

    auto lbMin = new QLabel("-96 dB");
    lbMin->setAlignment(Qt::AlignRight);

    auto mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins({4,0,4,0});
    mainLayout->setSpacing(0);
    mainLayout->addWidget(lbMax);
    mainLayout->addStretch();
    mainLayout->addWidget(lbMin);
    setLayout(mainLayout);
}