//
// Created by FlutyDeer on 2026/7/13.
//

#include "TimeSignatureComboBox.h"

#include "TimeSignaturePopupWidget.h"

#include <QApplication>
#include <QCursor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyle>
#include <QTimer>

TimeSignatureComboBox::TimeSignatureComboBox(QWidget *parent) : InlineEditLabel(parent) {
    m_popup = new TimeSignaturePopupWidget(this);
    m_popup->installEventFilter(this);

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
        const int numerator = parts.at(0).toInt(&numeratorOk);
        const int denominator = parts.at(1).toInt(&denominatorOk);
        if (!numeratorOk || !denominatorOk ||
            (m_numerator == numerator && m_denominator == denominator))
            return;

        m_numerator = numerator;
        m_denominator = denominator;
        emit timeSignatureChanged(numerator, denominator);
    });

    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    connect(m_clickTimer, &QTimer::timeout, this, &TimeSignatureComboBox::showPopup);
}

void TimeSignatureComboBox::setTimeSignature(int numerator, int denominator) {
    m_numerator = numerator;
    m_denominator = denominator;
    setText(QString::number(numerator) + QStringLiteral("/") + QString::number(denominator));
}

void TimeSignatureComboBox::mousePressEvent(QMouseEvent *event) {
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

void TimeSignatureComboBox::mouseDoubleClickEvent(QMouseEvent *event) {
    if (event->button() != Qt::LeftButton) {
        InlineEditLabel::mouseDoubleClickEvent(event);
        return;
    }

    m_clickTimer->stop();
    hidePopup();
    InlineEditLabel::mouseDoubleClickEvent(event);
}

bool TimeSignatureComboBox::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_popup && (event->type() == QEvent::Hide || event->type() == QEvent::Close)) {
        if (!m_hidingPopupFromCombo && (QGuiApplication::mouseButtons() & Qt::LeftButton) &&
            rect().contains(mapFromGlobal(QCursor::pos())))
            m_ignoreNextShow = true;
        setPopupVisible(false);
    }
    return InlineEditLabel::eventFilter(watched, event);
}

void TimeSignatureComboBox::showPopup() {
    if (m_ignoreNextShow) {
        m_ignoreNextShow = false;
        setPopupVisible(false);
        return;
    }
    if (m_popup->isVisible())
        return;

    m_popup->setTimeSignature(m_numerator, m_denominator);
    setPopupVisible(true);
    m_popup->showAt(mapToGlobal(QPoint(0, height())));
}

void TimeSignatureComboBox::hidePopup() {
    if (m_popup->isVisible())
        m_popup->close();
    setPopupVisible(false);
}

void TimeSignatureComboBox::setPopupVisible(bool visible) {
    if (property("popupVisible").toBool() == visible)
        return;
    setProperty("popupVisible", visible);
    style()->unpolish(this);
    style()->polish(this);
    update();
}
