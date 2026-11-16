#include "TempoComboBox.h"

#include "TempoPopupWidget.h"

#include <lite/MusicBase/Tempo.h>

#include <QLocale>

TempoComboBox::TempoComboBox(QWidget *parent) : PopupInlineEditLabel(parent) {
    m_popup = new TempoPopupWidget(this);
    setPopupWidget(m_popup);

    connect(m_popup, &TempoPopupWidget::tempoSelected, this, [this](double tempo) {
        if (m_tempo == tempo)
            return;
        setTempo(tempo);
        emit tempoChanged(tempo);
    });
    connect(this, &InlineEditLabel::editCompleted, this, [this](const QString &text) {
        bool ok = false;
        const double tempo = QLocale().toDouble(text.trimmed(), &ok);
        if (!ok || m_tempo == tempo)
            return;
        m_tempo = tempo;
        emit tempoChanged(tempo);
    });
}

void TempoComboBox::setTempo(const double tempo) {
    m_tempo = tempo;
    setText(Tempo::formatValue(tempo));
}

void TempoComboBox::preparePopup() {
    m_popup->setTempo(m_tempo);
}

void TempoComboBox::presentPopup(const QPoint &globalPos) {
    m_popup->showAt(globalPos);
}
