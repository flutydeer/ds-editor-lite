#include "TableConfigWidget.h"

namespace FillLyric {

    TableConfigWidget::TableConfigWidget(QWidget *parent) : QWidget(parent) {
        m_mainLayout = new QVBoxLayout(this);
        m_mainLayout->setContentsMargins(10, 30, 10, 10);

        m_colWidthRatioSpinBox = new QDoubleSpinBox(this);
        m_colWidthRatioLabel = new QLabel("Column Width Ratio", this);
        m_colWidthRatioSpinBox->setRange(1.0, 20.0);
        m_colWidthRatioSpinBox->setValue(7.8);
        m_colWidthRatioSpinBox->setSingleStep(0.1);

        m_rowHeightSpinBox = new QDoubleSpinBox(this);
        m_rowHeightLabel = new QLabel("Row Height Ratio", this);
        m_rowHeightSpinBox->setRange(1.0, 10.0);
        m_rowHeightSpinBox->setValue(3);
        m_rowHeightSpinBox->setSingleStep(0.1);

        m_fontDiffSpinBox = new QSpinBox(this);
        m_fontDiffLabel = new QLabel("Font Size Diff", this);
        m_fontDiffSpinBox->setRange(0, 10);
        m_fontDiffSpinBox->setValue(3);

        m_mainLayout->addWidget(m_colWidthRatioLabel);
        m_mainLayout->addWidget(m_colWidthRatioSpinBox);
        m_mainLayout->addWidget(m_rowHeightLabel);
        m_mainLayout->addWidget(m_rowHeightSpinBox);
        m_mainLayout->addWidget(m_fontDiffLabel);
        m_mainLayout->addWidget(m_fontDiffSpinBox);
        m_mainLayout->addStretch(1);

        setLayout(m_mainLayout);
    }

    TableConfigWidget::~TableConfigWidget() = default;
} // FillLyric