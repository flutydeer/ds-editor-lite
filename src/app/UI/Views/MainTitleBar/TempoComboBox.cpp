#include "TempoComboBox.h"

#include "TempoPopupWidget.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyle>
#include <QTimer>

TempoComboBox::TempoComboBox(QWidget *parent) : InlineEditLabel(parent) {
    m_popup = new TempoPopupWidget(this);
    m_popup->installEventFilter(this);

    connect(m_popup, &TempoPopupWidget::tempoSelected, this, [this](double tempo) {
        if (m_tempo == tempo)
            return;
        setTempo(tempo);
        emit tempoChanged(tempo);
    });
    connect(this, &InlineEditLabel::editCompleted, this, [this](const QString &text) {
        bool ok = false;
        const double tempo = text.trimmed().toDouble(&ok);
        if (!ok || m_tempo == tempo)
            return;
        m_tempo = tempo;
        emit tempoChanged(tempo);
    });

    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    connect(m_clickTimer, &QTimer::timeout, this, &TempoComboBox::showPopup);
}

void TempoComboBox::setTempo(double tempo) {
    m_tempo = tempo;
    setText(QString::number(tempo));
}

void TempoComboBox::mousePressEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        InlineEditLabel::mousePressEvent(event);
        return;
    }

    if (m_popup->isVisible()) {
        m_hidingPopupFromCombo = true;
        hidePopup();
        m_hidingPopupFromCombo = false;
    } else {
        m_clickTimer->start(QApplication::doubleClickInterval());
    }
    event->accept();
}

void TempoComboBox::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        InlineEditLabel::mouseDoubleClickEvent(event);
        return;
    }

    m_clickTimer->stop();
    hidePopup();
    InlineEditLabel::mouseDoubleClickEvent(event);
}

bool TempoComboBox::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_popup && (event->type() == QEvent::Hide || event->type() == QEvent::Close)) {
        if (!m_hidingPopupFromCombo && (QGuiApplication::mouseButtons() & Qt::LeftButton) &&
            rect().contains(mapFromGlobal(QCursor::pos())))
            m_ignoreNextShow = true;
        setPopupVisible(false);
    }
    return InlineEditLabel::eventFilter(watched, event);
}

void TempoComboBox::showPopup() {
    if (m_ignoreNextShow) {
        m_ignoreNextShow = false;
        setPopupVisible(false);
        return;
    }
    if (m_popup->isVisible())
        return;

    m_popup->setTempo(m_tempo);
    setPopupVisible(true);
    m_popup->showAt(mapToGlobal(QPoint(0, height())));
}

void TempoComboBox::hidePopup() {
    if (m_popup->isVisible())
        m_popup->close();
    setPopupVisible(false);
}

void TempoComboBox::setPopupVisible(bool visible) {
    if (property("popupVisible").toBool() == visible)
        return;
    setProperty("popupVisible", visible);
    style()->unpolish(this);
    style()->polish(this);
    update();
}
