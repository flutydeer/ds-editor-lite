//
// Created by fluty on 24-10-27.
//

#include "PronunciationView.h"
#include "NoteView.h"
#include "UI/Utils/TrackColorPalette.h"

#include <QElapsedTimer>
#include <QPainter>
#include <QGraphicsProxyWidget>
#include <QKeyEvent>
#include <QLineEdit>

PronunciationView::PronunciationView(const int noteId, QGraphicsItem *parent)
    : AbstractGraphicsRectItem(parent), UniqueObject(noteId) {
}

PronunciationView::~PronunciationView() {
    if (m_lineEditProxy) {
        delete m_lineEditProxy;
    }
}

void PronunciationView::setPronunciation(const QString &pronunciation, const bool edited) {
    m_pronunciation = pronunciation;
    m_pronunciationEdited = edited;
    update();
}

void PronunciationView::setTextVisible(const bool visible) {
    if (m_textVisible != visible) {
        // qDebug() << "setTextVisible:" << visible;
        m_textVisible = visible;
        update();
    }
}

void PronunciationView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                              QWidget *widget) {
    if (!m_textVisible)
        return;

    QElapsedTimer timer;
    timer.start();
    constexpr auto pronColorOriginal = QColor(200, 200, 200);
    const auto pronColorEdited =
        TrackColorPalette::instance()->phonemeEdited(NoteView::trackColorIndex());
    constexpr auto penWidth = 1.5f;
    constexpr int padding = 2;

    auto rect = boundingRect();
    auto left = rect.left() + penWidth;
    auto width = rect.width() - penWidth * 2;
    auto paddedRect = QRectF(left, rect.top(), width, rect.height());
    auto textRectLeft = paddedRect.left() + padding;
    auto textRectTop = paddedRect.top();
    auto textRectWidth = paddedRect.width() - 2 * padding;
    auto textRectHeight = paddedRect.height();
    auto textRect = QRectF(textRectLeft, textRectTop, textRectWidth, textRectHeight);

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(m_pronunciationEdited ? pronColorEdited : pronColorOriginal);
    painter->setPen(pen);
    painter->drawText(textRect, m_pronunciation);

    // const auto time = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
    // qDebug() << "PronunciationView painted in" << time << "ms";
}

void PronunciationView::updateRectAndPos() {
    // update();
}

void PronunciationView::startEditingPronunciation() {
    if (m_editingPronunciation)
        return;

    m_editingPronunciation = true;

    if (!m_lineEditProxy) {
        m_lineEdit = new QLineEdit();
        m_lineEdit->setFrame(false);
        m_lineEdit->installEventFilter(this);
        m_lineEditProxy = new QGraphicsProxyWidget(this);
        m_lineEditProxy->setWidget(m_lineEdit);

        connect(m_lineEdit, &QLineEdit::editingFinished, this, &PronunciationView::finishEditingPronunciation);
    }

    m_lineEdit->setText(m_pronunciation);
    updateLineEditGeometry();
    m_lineEditProxy->show();
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
    update();
}

void PronunciationView::finishEditingPronunciation() {
    if (!m_editingPronunciation)
        return;

    m_editingPronunciation = false;

    if (m_lineEditProxy) {
        const QString newPronunciation = m_lineEdit->text().trimmed();
        m_lineEditProxy->hide();

        if (m_lineEdit) {
            m_lineEdit->clearFocus();
        }

        if (newPronunciation != m_pronunciation && !newPronunciation.isEmpty()) {
            emit pronunciationEditingFinished(newPronunciation);
        }
    }

    update();
}

bool PronunciationView::isEditingPronunciation() const {
    return m_editingPronunciation;
}

void PronunciationView::updateLineEditGeometry() {
    if (!m_lineEditProxy || !m_lineEdit)
        return;

    auto rect = boundingRect();
    constexpr auto penWidth = 1.5;
    constexpr int padding = 2;

    auto left = rect.left() + penWidth + padding;
    auto top = rect.top();
    auto width = rect.width() - penWidth * 2 - padding * 2;
    auto height = rect.height();

    if (width < 10)
        width = 10;

    m_lineEditProxy->setPos(left, top);
    m_lineEdit->setFixedSize(static_cast<int>(width), static_cast<int>(height));
}

bool PronunciationView::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_lineEdit && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            finishEditingPronunciation();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            if (m_lineEdit) {
                m_lineEdit->setText(m_pronunciation);
            }
            finishEditingPronunciation();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}