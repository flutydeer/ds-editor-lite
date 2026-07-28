#include "TimeSignatureEditWidget.h"

#include <lite/GUI/Controls/ComboBox.h>
#include <lite/GUI/Controls/SvsExpressionSpinBox.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <array>
#include <limits>

namespace {

    constexpr std::array kDenominators = {1, 2, 4, 8, 16, 32, 64, 128};

}

TimeSignatureEditWidget::TimeSignatureEditWidget(QWidget *parent) : QWidget(parent) {
    m_spinNumerator = new SVS::ExpressionSpinBox;
    m_spinNumerator->setObjectName("spinNumerator");
    m_spinNumerator->setRange(1, std::numeric_limits<int>::max());
    m_spinNumerator->setValue(m_numerator);
    auto numeratorSizePolicy = m_spinNumerator->sizePolicy();
    numeratorSizePolicy.setHorizontalPolicy(QSizePolicy::Ignored);
    m_spinNumerator->setSizePolicy(numeratorSizePolicy);

    auto *slashLabel = new QLabel(QStringLiteral("/"));
    slashLabel->setObjectName("slashLabel");
    slashLabel->setAlignment(Qt::AlignCenter);
    auto slashSizePolicy = slashLabel->sizePolicy();
    slashSizePolicy.setHorizontalPolicy(QSizePolicy::Fixed);
    slashLabel->setSizePolicy(slashSizePolicy);

    m_cbDenominator = new ComboBox;
    m_cbDenominator->setObjectName("cbDenominator");
    auto denominatorSizePolicy = m_cbDenominator->sizePolicy();
    denominatorSizePolicy.setHorizontalPolicy(QSizePolicy::Ignored);
    m_cbDenominator->setSizePolicy(denominatorSizePolicy);
    for (const int denominator : kDenominators)
        m_cbDenominator->addItem(QLocale().toString(denominator), denominator);

    auto *editorRow = new QHBoxLayout;
    editorRow->setContentsMargins(0, 0, 0, 0);
    editorRow->setSpacing(6);
    editorRow->addWidget(m_spinNumerator, 1);
    editorRow->addWidget(slashLabel);
    editorRow->addWidget(m_cbDenominator, 1);

    auto *presetsRow = new QHBoxLayout;
    presetsRow->setContentsMargins(0, 0, 0, 0);
    presetsRow->setSpacing(6);

    const auto createPresetButton = [this](int numerator, int denominator) {
        auto *button =
            new QPushButton(QStringLiteral("%L1/%L2").arg(numerator).arg(denominator));
        button->setObjectName("btnPreset");
        button->setFixedHeight(28);
        connect(button, &QPushButton::clicked, this,
                [this, numerator, denominator] { onPresetClicked(numerator, denominator); });
        return button;
    };
    presetsRow->addWidget(createPresetButton(4, 4));
    presetsRow->addWidget(createPresetButton(2, 4));
    presetsRow->addWidget(createPresetButton(3, 4));
    presetsRow->addWidget(createPresetButton(6, 8));

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);
    mainLayout->addLayout(editorRow);
    mainLayout->addLayout(presetsRow);

    setEditors(m_numerator, m_denominator);

    connect(m_spinNumerator, &SVS::ExpressionSpinBox::valueChanged, this, [this](int numerator) {
        m_numerator = numerator;
        emit timeSignatureEdited(m_numerator, m_denominator);
    });
    connect(m_cbDenominator, &ComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0)
            return;
        m_denominator = m_cbDenominator->itemData(index).toInt();
        emit timeSignatureEdited(m_numerator, m_denominator);
    });
}

void TimeSignatureEditWidget::setTimeSignature(int numerator, int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
    setEditors(numerator, denominator);
}

int TimeSignatureEditWidget::numerator() const {
    return m_numerator;
}

int TimeSignatureEditWidget::denominator() const {
    return m_denominator;
}

void TimeSignatureEditWidget::onPresetClicked(int numerator, int denominator) {
    if (m_numerator == numerator && m_denominator == denominator)
        return;
    m_numerator = numerator;
    m_denominator = denominator;
    setEditors(numerator, denominator);
    emit timeSignatureEdited(numerator, denominator);
}

void TimeSignatureEditWidget::setEditors(int numerator, int denominator) {
    const QSignalBlocker numeratorBlocker(m_spinNumerator);
    const QSignalBlocker denominatorBlocker(m_cbDenominator);
    m_spinNumerator->setValue(numerator);
    int index = m_cbDenominator->findData(denominator);
    if (index < 0) {
        m_cbDenominator->addItem(QLocale().toString(denominator), denominator);
        index = m_cbDenominator->count() - 1;
    }
    m_cbDenominator->setCurrentIndex(index);
}
