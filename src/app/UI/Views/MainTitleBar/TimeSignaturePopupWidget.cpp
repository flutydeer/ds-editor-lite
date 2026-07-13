//
// Created by FlutyDeer on 2026/7/13.
//

#include "TimeSignaturePopupWidget.h"

#include "UI/Controls/ComboBox.h"
#include "UI/Controls/SvsExpressionSpinBox.h"
#include "Utils/SystemUtils.h"

#ifdef Q_OS_WIN
#  include <Windows.h>
#  include <dwmapi.h>
#endif

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include <array>
#include <limits>

namespace {

    constexpr std::array kDenominators = {1, 2, 4, 8, 16, 32, 64, 128};

}

TimeSignaturePopupWidget::TimeSignaturePopupWidget(QWidget *parent) : QFrame(parent) {
    setObjectName("timeSignaturePopup");
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_WindowPropagation);
    setProperty("dwmBorder", false);

#ifdef Q_OS_WIN
    if (SystemUtils::isWindows11())
        setProperty("dwmBorder", true);
#endif

    auto *titleLabel = new QLabel(tr("Time Signature"));
    titleLabel->setObjectName("popupTitle");

    m_spinNumerator = new SVS::ExpressionSpinBox;
    m_spinNumerator->setObjectName("spinNumerator");
    m_spinNumerator->setRange(1, std::numeric_limits<int>::max());
    m_spinNumerator->setValue(m_numerator);
    m_spinNumerator->setFixedWidth(120);
    m_spinNumerator->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto *slashLabel = new QLabel(QStringLiteral("/"));
    slashLabel->setObjectName("slashLabel");
    slashLabel->setAlignment(Qt::AlignCenter);

    m_cbDenominator = new ComboBox;
    m_cbDenominator->setObjectName("cbDenominator");
    m_cbDenominator->setFixedWidth(80);
    for (const int denominator : kDenominators)
        m_cbDenominator->addItem(QString::number(denominator), denominator);

    auto *editorRow = new QHBoxLayout;
    editorRow->setContentsMargins(0, 0, 0, 0);
    editorRow->setSpacing(6);
    editorRow->addWidget(m_spinNumerator);
    editorRow->addWidget(slashLabel);
    editorRow->addWidget(m_cbDenominator);

    auto *presetsRow = new QHBoxLayout;
    presetsRow->setContentsMargins(0, 0, 0, 0);
    presetsRow->setSpacing(6);

    const auto createPresetButton = [this](int numerator, int denominator) {
        auto *button = new QPushButton(QStringLiteral("%1/%2").arg(numerator).arg(denominator));
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

    auto *surface = new QFrame;
    surface->setObjectName("timeSignaturePopupSurface");
    surface->setAttribute(Qt::WA_StyledBackground);
    auto *surfaceLayout = new QVBoxLayout(surface);
    surfaceLayout->setContentsMargins(12, 12, 12, 12);
    surfaceLayout->setSpacing(8);
    surfaceLayout->addWidget(titleLabel);
    surfaceLayout->addLayout(editorRow);
    surfaceLayout->addLayout(presetsRow);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->setSizeConstraint(QLayout::SetFixedSize);
    outerLayout->addWidget(surface);

    setEditors(m_numerator, m_denominator);

    connect(m_spinNumerator, &SVS::ExpressionSpinBox::valueChanged, this, [this](int numerator) {
        m_numerator = numerator;
        emit timeSignatureSelected(m_numerator, m_denominator);
    });
    connect(m_cbDenominator, &ComboBox::currentIndexChanged, this, [this](int index) {
        if (index < 0)
            return;
        m_denominator = m_cbDenominator->itemData(index).toInt();
        emit timeSignatureSelected(m_numerator, m_denominator);
    });
}

void TimeSignaturePopupWidget::setTimeSignature(int numerator, int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
    setEditors(numerator, denominator);
}

void TimeSignaturePopupWidget::showAt(const QPoint &globalPos) {
    QPoint topLeft = globalPos;
    if (const auto screen = QApplication::screenAt(globalPos)) {
        const QRect available = screen->availableGeometry();
        const QRect popupRect(topLeft, sizeHint());
        if (popupRect.right() > available.right())
            topLeft.setX(available.right() - popupRect.width());
        if (popupRect.bottom() > available.bottom())
            topLeft.setY(globalPos.y() - popupRect.height());
        if (topLeft.x() < available.left())
            topLeft.setX(available.left());
        if (topLeft.y() < available.top())
            topLeft.setY(available.top());
    }

    move(topLeft);
    show();
    raise();
    applyWindowEffects();
}

void TimeSignaturePopupWidget::applyWindowEffects() {
#ifdef Q_OS_WIN
    if (!SystemUtils::isWindows11() || !winId())
        return;

    HWND hwnd = reinterpret_cast<HWND>(winId());
    constexpr int DWMWA_USE_IMMERSIVE_DARK_MODE_ = 20;
    constexpr int DWMWA_WINDOW_CORNER_PREFERENCE_ = 33;
    constexpr int DWMWA_NCRENDERING_POLICY_ = 2;
    DWMNCRENDERINGPOLICY ncrp = DWMNCRP_ENABLED;
    INT dwcp = 2;
    UINT dark = 1;
    DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY_, &ncrp, sizeof(ncrp));
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_, &dark, sizeof(dark));
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE_, &dwcp, sizeof(dwcp));
#endif
}

void TimeSignaturePopupWidget::onPresetClicked(int numerator, int denominator) {
    if (m_numerator == numerator && m_denominator == denominator)
        return;
    m_numerator = numerator;
    m_denominator = denominator;
    setEditors(numerator, denominator);
    emit timeSignatureSelected(numerator, denominator);
}

void TimeSignaturePopupWidget::setEditors(int numerator, int denominator) {
    const QSignalBlocker numeratorBlocker(m_spinNumerator);
    const QSignalBlocker denominatorBlocker(m_cbDenominator);
    m_spinNumerator->setValue(numerator);
    int index = m_cbDenominator->findData(denominator);
    if (index < 0) {
        m_cbDenominator->addItem(QString::number(denominator), denominator);
        index = m_cbDenominator->count() - 1;
    }
    m_cbDenominator->setCurrentIndex(index);
}
