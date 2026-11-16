#include "TimeSignatureComboBox.h"

#include "UI/Views/Common/TimeSignaturePopupWidget.h"

#include <QLocale>

TimeSignatureComboBox::TimeSignatureComboBox(QWidget *parent) : PopupInlineEditLabel(parent) {
    m_popup = new TimeSignaturePopupWidget(this);
    setPopupWidget(m_popup);

    connect(m_popup, &TimeSignaturePopupWidget::timeSignatureSelected, this,
            [this](int numerator, int denominator) {
                if (m_numerator == numerator && m_denominator == denominator)
                    return;
                setTimeSignature(numerator, denominator);
                emit timeSignatureChanged(numerator, denominator);
            });
    connect(this, &InlineEditLabel::editCompleted, this, [this](const QString &text) {
        const auto parts = text.split(QLatin1Char('/'));
        if (parts.size() != 2)
            return;

        bool numeratorOk = false;
        bool denominatorOk = false;
        const int numerator = QLocale().toInt(parts.at(0).trimmed(), &numeratorOk);
        const int denominator = QLocale().toInt(parts.at(1).trimmed(), &denominatorOk);
        if (!numeratorOk || !denominatorOk ||
            (m_numerator == numerator && m_denominator == denominator))
            return;

        m_numerator = numerator;
        m_denominator = denominator;
        emit timeSignatureChanged(numerator, denominator);
    });
}

void TimeSignatureComboBox::setTimeSignature(const int numerator, const int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
    const QLocale locale;
    setText(locale.toString(numerator) + QStringLiteral("/") + locale.toString(denominator));
}

void TimeSignatureComboBox::preparePopup() {
    m_popup->setTimeSignature(m_numerator, m_denominator);
}

void TimeSignatureComboBox::presentPopup(const QPoint &globalPos) {
    m_popup->showAt(globalPos);
}
