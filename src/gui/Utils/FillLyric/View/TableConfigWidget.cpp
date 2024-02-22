#include "TableConfigWidget.h"

namespace FillLyric {

    TableConfigWidget::TableConfigWidget(QWidget *parent) : QWidget(parent) {
        m_mainLayout = new QVBoxLayout(this);

        m_fontLayout = new QHBoxLayout(this);
        m_aspectRatioSpinBox = new QDoubleSpinBox(this);
        m_aspectRatioSpinBox->setRange(0.5, 10.0);
        m_aspectRatioSpinBox->setValue(1.5);
        m_aspectRatioSpinBox->setSingleStep(0.1);

        m_fontDiffSpinBox = new QSpinBox(this);
        m_fontDiffSpinBox->setRange(0, 10);
        m_fontDiffSpinBox->setValue(3);

        m_fontLayout->addWidget(m_aspectRatioSpinBox);
        m_fontLayout->addWidget(m_fontDiffSpinBox);
        m_mainLayout->addLayout(m_fontLayout);

        setLayout(m_mainLayout);
    }

    TableConfigWidget::~TableConfigWidget() = default;
} // FillLyric