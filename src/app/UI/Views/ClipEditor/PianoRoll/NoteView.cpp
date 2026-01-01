//
// Created by fluty on 2024/1/23.
//

#include "NoteView.h"

#include "PronunciationView.h"
#include "Global/AppGlobal.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/AbstractGraphicsRectItem.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QPainter>
#include <QTextOption>
#include <QMWidgets/cmenu.h>
#include <QDebug>
#include <QElapsedTimer>
#include <QGraphicsProxyWidget>
#include <QLineEdit>
#include <QKeyEvent>
#include <QFocusEvent>

using namespace ClipEditorGlobal;

NoteView::NoteView(const int itemId, QGraphicsItem *parent)
    : AbstractGraphicsRectItem(parent), UniqueObject(itemId) {
    initUi();
}

NoteView::~NoteView() {
    delete m_pronView;
    if (m_lineEditProxy) {
        delete m_lineEditProxy;
    }
}

int NoteView::rStart() const {
    return m_rStart;
}

void NoteView::setRStart(const int rStart) {
    m_rStart = rStart;
    updateRectAndPos();
}

int NoteView::length() const {
    return m_length;
}

void NoteView::setLength(const int length) {
    m_length = length;
    updateRectAndPos();
}

int NoteView::keyIndex() const {
    return m_keyIndex;
}

void NoteView::setKeyIndex(const int keyIndex) {
    m_keyIndex = keyIndex;
    updateRectAndPos();
}

QString NoteView::lyric() const {
    return m_lyric;
}

void NoteView::setLyric(const QString &lyric) {
    m_lyric = lyric;
    update();
}

void NoteView::setPronunciation(const QString &pronunciation, const bool edited) {
    m_pronunciation = pronunciation;
    m_pronunciationEdited = edited;
    if (m_pronView)
        m_pronView->setPronunciation(pronunciation, edited);
    update();
}

bool NoteView::editingPitch() const {
    return m_editingPitch;
}

void NoteView::setEditingPitch(const bool on) {
    m_editingPitch = on;
    update();
}

PronunciationView *NoteView::pronunciationView() const {
    return m_pronView;
}

void NoteView::setPronunciationView(PronunciationView *view) {
    m_pronView = view;
    updateRectAndPos();
}

int NoteView::startOffset() const {
    return m_startOffset;
}

void NoteView::setStartOffset(const int tick) {
    m_startOffset = tick;
    updateRectAndPos();
}

int NoteView::lengthOffset() const {
    return m_lengthOffset;
}

void NoteView::setLengthOffset(const int tick) {
    m_lengthOffset = tick;
    updateRectAndPos();
}

int NoteView::keyOffset() const {
    return m_keyOffset;
}

void NoteView::setKeyOffset(const int key) {
    m_keyOffset = key;
    updateRectAndPos();
}

void NoteView::resetOffset() {
    m_startOffset = 0;
    m_lengthOffset = 0;
    m_keyOffset = 0;
    updateRectAndPos();
}

void NoteView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
    QElapsedTimer timer;
    timer.start();

    constexpr auto backgroundColorNormal = QColor(155, 186, 255);
    constexpr auto backgroundColorEditingPitch = QColor(53, 59, 74);
    constexpr auto backgroundColorOverlapped = QColor(110, 129, 171);

    constexpr auto borderColorNormal = QColor(112, 156, 255);
    constexpr auto borderColorSelected = QColor(255, 255, 255);
    constexpr auto borderColorOverlapped = QColor(110, 129, 171);
    constexpr auto borderColorEditingPitch = QColor(126, 149, 199);

    constexpr auto foregroundColorNormal = QColor(0, 0, 0);
    constexpr auto foregroundColorEditingPitch = QColor(126, 149, 199);
    constexpr auto foregroundColorOverlapped = QColor(0, 0, 0, 127);

    constexpr auto penWidth = 1.5f;
    constexpr int padding = 2;
    // const auto radius = 4.0;
    // const auto radiusAdjustThreshold = 12;

    QPen pen;

    auto rect = boundingRect();
    auto left = rect.left() + penWidth;
    auto top = rect.top() + penWidth;
    auto width = rect.width() - penWidth * 2;
    auto height = rect.height() - penWidth * 2;
    auto paddedRect = QRectF(left, top, width, height);

    auto drawRectOnly = [&] {
        if (m_pronView)
            m_pronView->setTextVisible(false);

        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(Qt::NoPen);
        QColor brushColor;
        if (isSelected())
            brushColor = borderColorSelected;
        else if (overlapped())
            brushColor = borderColorOverlapped;
        else if (m_editingPitch)
            brushColor = borderColorEditingPitch;
        else
            brushColor = backgroundColorNormal;
        painter->setBrush(brushColor);
        auto l = rect.left() + penWidth / 2;
        auto t = rect.top() + penWidth / 2;
        auto w = rect.width() - penWidth < 2 ? 2 : rect.width() - penWidth;
        auto h = rect.height() - penWidth < 2 ? 2 : rect.height() - penWidth;
        painter->drawRect(QRectF(l, t, w, h));
    };

    auto drawFullNote = [&] {
        QColor borderColor;
        QColor backgroundColor;
        QColor foregroundColor;
        if (isSelected()) {
            borderColor = borderColorSelected;
            backgroundColor = backgroundColorNormal;
            foregroundColor = foregroundColorNormal;
        } else if (overlapped()) {
            borderColor = borderColorOverlapped;
            backgroundColor = backgroundColorOverlapped;
            foregroundColor = foregroundColorOverlapped;
        } else if (m_editingPitch) {
            borderColor = borderColorEditingPitch;
            backgroundColor = backgroundColorEditingPitch;
            foregroundColor = foregroundColorEditingPitch;
        } else {
            borderColor = borderColorNormal;
            backgroundColor = backgroundColorNormal;
            foregroundColor = foregroundColorNormal;
        }
        pen.setColor(borderColor);
        pen.setWidthF(penWidth);
        painter->setPen(pen);
        painter->setBrush(backgroundColor);
        // auto straightX = paddedRect.width() - radius * 2;
        // auto straightY = paddedRect.height() - radius * 2;
        // auto xRadius = radius;
        // auto yRadius = radius;
        // if (straightX < radiusAdjustThreshold)
        //     xRadius = radius * (straightX - radius) / (radiusAdjustThreshold - radius);
        // if (straightY < radiusAdjustThreshold)
        //     yRadius = radius * (straightY - radius) / (radiusAdjustThreshold - radius);
        // painter->drawRoundedRect(paddedRect, xRadius, yRadius);
        painter->drawRoundedRect(paddedRect, 2, 2);

        pen.setColor(foregroundColor);
        painter->setPen(pen);
        auto font = QFont();
        font.setPixelSize(fontPixelSize);
        painter->setFont(font);
        auto textRectLeft = paddedRect.left() + padding;
        // auto textRectTop = paddedRect.top() + padding;
        auto textRectTop = paddedRect.top();
        auto textRectWidth = paddedRect.width() - 2 * padding;
        // auto textRectHeight = paddedRect.height() - 2 * padding;
        auto textRectHeight = paddedRect.height();
        auto textRect = QRectF(textRectLeft, textRectTop, textRectWidth, textRectHeight);

        auto fontMetrics = painter->fontMetrics();
        auto textHeight = fontMetrics.height();
        auto lyricTextWidth = fontMetrics.horizontalAdvance(m_lyric);
        auto pronTextWidth = fontMetrics.horizontalAdvance(m_pronunciation);
        QTextOption textOption(Qt::AlignVCenter);
        textOption.setWrapMode(QTextOption::NoWrap);

        if (!m_editingLyric && qMax(lyricTextWidth, pronTextWidth) < textRectWidth && textHeight < textRectHeight) {
            // auto time1 = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
            painter->drawText(textRect, m_lyric, textOption);
            // auto time2 = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
            // qDebug() << "Lyric painted in" << time2 - time1 << "ms";
            if (m_pronView) {
                // adjustPronView();
                m_pronView->setTextVisible(true);
            }
        } else {
            if (m_pronView)
                m_pronView->setTextVisible(false);
        }
    };

    if (scaleX() < 0.3)
        drawRectOnly();
    else
        drawFullNote();

    // const auto time = static_cast<double>(timer.nsecsElapsed()) / 1000000.0;
    // qDebug() << "NoteView painted in" << time << "ms";
}

// void NoteView::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
//     // qDebug() << "NoteGraphicsItem::hoverMoveEvent" << event->pos().rx();
//     if (!m_editingPitch) {
//         const auto rx = event->pos().rx();
//         const auto ry = event->pos().ry();
//         const bool xInFilledRect =
//             rx >= 0 && rx <= AppGlobal::resizeTolerance ||
//             rx >= rect().width() - AppGlobal::resizeTolerance && rx <= rect().width();
//         const bool yInFilledRect = ry >= 0 && ry <= rect().height() - pronunciationTextHeight();
//         if (yInFilledRect && xInFilledRect)
//             setCursor(Qt::SizeHorCursor);
//         else
//             setCursor(Qt::ArrowCursor);
//     }
//     QGraphicsRectItem::hoverMoveEvent(event);
// }

void NoteView::updateRectAndPos() {
    // qDebug() << "updateRectAndPos";
    const auto x = (m_rStart + m_startOffset) * scaleX() * pixelsPerQuarterNote / 480;
    const auto y = -(m_keyIndex + m_keyOffset - 127) * noteHeight * scaleY();
    const auto w = (m_length + m_lengthOffset) * scaleX() * pixelsPerQuarterNote / 480;
    const auto h = noteHeight * scaleY();
    setPos(x, y);
    setRect(QRectF(0, 0, w, h));
    if (m_pronView)
        adjustPronView();
    
    if (m_editingLyric)
        updateLineEditGeometry();

    update();
}

void NoteView::adjustPronView() const {
    // qDebug() << "adjustPronView";
    m_pronView->setPos(pos().x(), pos().y() + boundingRect().height());
    m_pronView->setRect(QRectF(0, 0, boundingRect().width(), m_pronView->textHeight));
}

void NoteView::initUi() {
    // setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable);
    fontPixelSize.onChanged([this](int) { update(); });
}

void NoteView::startEditingLyric() {
    if (m_editingLyric)
        return;
    
    m_editingLyric = true;
    
    if (!m_lineEditProxy) {
        m_lineEdit = new QLineEdit();
        m_lineEdit->setFrame(false);
        m_lineEdit->installEventFilter(this);
        m_lineEditProxy = new QGraphicsProxyWidget(this);
        m_lineEditProxy->setWidget(m_lineEdit);
        
        // Note: editingFinished signal is also emitted when losing focus, but we handle Tab key through eventFilter
        connect(m_lineEdit, &QLineEdit::editingFinished, this, &NoteView::finishEditingLyric);
    }
    
    m_lineEdit->setText(m_lyric);
    updateLineEditGeometry();
    m_lineEditProxy->show();
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
    update();
}

void NoteView::finishEditingLyric() {
    if (!m_editingLyric)
        return;

    m_editingLyric = false;

    if (m_lineEditProxy) {
        const QString newLyric = m_lineEdit->text();
        m_lineEditProxy->hide();

        // Clear focus to allow keyboard shortcuts to work again
        if (m_lineEdit) {
            m_lineEdit->clearFocus();
        }

        // Only emit signal if lyric actually changed
        if (newLyric != m_lyric) {
            emit lyricEditingFinished(newLyric);
        }
    }

    update();
}

bool NoteView::isEditingLyric() const {
    return m_editingLyric;
}

void NoteView::updateLineEditGeometry() {
    if (!m_lineEditProxy || !m_lineEdit)
        return;
    
    constexpr auto penWidth = 1.5f;
    constexpr int padding = 2;
    
    auto rect = boundingRect();
    auto left = rect.left() + penWidth + padding;
    auto top = rect.top() + penWidth;
    auto width = rect.width() - penWidth * 2 - 2 * padding;
    auto height = rect.height() - penWidth * 2;
    
    auto font = QFont();
    font.setPixelSize(fontPixelSize);
    m_lineEdit->setFont(font);
    
    m_lineEditProxy->setPos(left, top);
    m_lineEdit->setFixedSize(static_cast<int>(width), static_cast<int>(height));
}

bool NoteView::eventFilter(QObject *obj, QEvent *event) {
    if (obj == m_lineEdit && event->type() == QEvent::KeyPress) {
        auto keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Tab) {
            // Block Tab key default behavior and emit our signal
            // Note: we need to block editingFinished signal first
            m_lineEdit->blockSignals(true);
            emit tabKeyPressed();
            m_lineEdit->blockSignals(false);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            // Enter key finishes editing
            finishEditingLyric();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            // Escape key cancels editing
            if (m_lineEdit) {
                m_lineEdit->setText(m_lyric); // Restore original text
            }
            finishEditingLyric();
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}