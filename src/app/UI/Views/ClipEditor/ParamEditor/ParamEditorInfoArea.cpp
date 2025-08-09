//
// Created by fluty on 24-10-13.
//

#include "ParamEditorInfoArea.h"

#include "Model/AppModel/ParamProperties.h"

#include <QLabel>
#include <QVBoxLayout>

ParamEditorInfoArea::ParamEditorInfoArea(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    m_lbMax = new QLabel;
    m_lbMax->setAlignment(Qt::AlignRight);

    m_lbMin = new QLabel;
    m_lbMin->setAlignment(Qt::AlignRight);

    const auto mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins({4, 0, 4, 0});
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_lbMax);
    mainLayout->addStretch();
    mainLayout->addWidget(m_lbMin);
    setLayout(mainLayout);
}

void ParamEditorInfoArea::setParamProperties(const ParamProperties &properties) {
    m_paramProperties = &properties;
    m_lbMax->setText(m_paramProperties->valueToString(m_paramProperties->maximum, true, 1));
    m_lbMin->setText(m_paramProperties->valueToString(m_paramProperties->minimum, true, 1));
}