//
// Created by fluty on 2024/1/23.
//

#include "NoteView.h"

#include "Global/AppGlobal.h"
#include "Global/ClipEditorGlobal.h"
#include "UI/Views/Common/CommonGraphicsRectItem.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QPainter>
#include <QTextOption>
#include <QMWidgets/cmenu.h>

using namespace ClipEditorGlobal;

NoteView::NoteView(int itemId, QGraphicsItem *parent)
    : CommonGraphicsRectItem(parent), UniqueObject(itemId) {
    initUi();
}
NoteView::NoteView(int itemId, int start, int length, int keyIndex,
                                   const QString &lyric, const QString &pronunciation,
                                   QGraphicsItem *parent)
    : CommonGraphicsRectItem(parent), UniqueObject(itemId) {
    m_start = start;
    m_length = length;
    m_keyIndex = keyIndex;
    m_lyric = lyric;
    m_pronunciation = pronunciation;
    initUi();
}
QWidget *NoteView::context() const {
    return m_context;
}
void NoteView::setContext(QWidget *context) {
    m_context = context;
    m_menu = new CMenu(m_context);
    auto actionRemove = m_menu->addAction("Remove");
    connect(actionRemove, &QAction::triggered, [&] { emit removeTriggered(id()); });

    m_menu->addSeparator();

    auto actionEditLyric = m_menu->addAction("Edit lyric");
    connect(actionEditLyric, &QAction::triggered, [&] { emit editLyricTriggered(id()); });

    // auto actionProperties = m_menu->addAction("Properties");
    // connect(actionProperties, &QAction::triggered,
    //         [&] { qDebug() << "actionProperties triggered" << id(); });
}
int NoteView::start() const {
    return m_start;
}
void NoteView::setStart(int start) {
    m_start = start;
    updateRectAndPos();
}
int NoteView::length() const {
    return m_length;
}
void NoteView::setLength(int length) {
    m_length = length;
    updateRectAndPos();
}
int NoteView::keyIndex() const {
    return m_keyIndex;
}
void NoteView::setKeyIndex(int keyIndex) {
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
QString NoteView::pronunciation() const {
    return m_pronunciation;
}
void NoteView::setPronunciation(const QString &pronunciation, bool edited) {
    m_pronunciation = pronunciation;
    m_pronunciationEdited = edited;
    update();
}
bool NoteView::editingPitch() const {
    return m_editingPitch;
}
void NoteView::setEditingPitch(bool on) {
    m_editingPitch = on;
    update();
}
int NoteView::pronunciationTextHeight() const {
    return m_pronunciationTextHeight;
}
int NoteView::startOffset() const {
    return m_startOffset;
}
void NoteView::setStartOffset(int tick) {
    m_startOffset = tick;
    updateRectAndPos();
}
int NoteView::lengthOffset() const {
    return m_lengthOffset;
}
void NoteView::setLengthOffset(int tick) {
    m_lengthOffset = tick;
    updateRectAndPos();
}
int NoteView::keyOffset() const {
    return m_keyOffset;
}
void NoteView::setKeyOffset(int key) {
    m_keyOffset = key;
    updateRectAndPos();
}
void NoteView::resetOffset() {
    m_startOffset = 0;
    m_lengthOffset = 0;
    m_keyOffset = 0;
    updateRectAndPos();
}
void NoteView::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
    const auto backgroundColorNormal = QColor(155, 186, 255);
    const auto backgroundColorEditingPitch = QColor(53, 59, 74);

    const auto borderColorNormal = QColor(112, 156, 255);
    const auto borderColorSelected = QColor(255, 255, 255);
    const auto borderColorOverlapped = AppGlobal::overlappedViewBorder;
    const auto borderColorEditingPitch = QColor(126, 149, 199);

    const auto foregroundColorNormal = QColor(0, 0, 0);
    const auto foregroundColorEditingPitch = QColor(126, 149, 199);

    const auto pronunciationTextColorOriginal = QColor(200, 200, 200);
    const auto pronunciationTextColorEdited = backgroundColorNormal;

    const auto penWidth = 1.5f;
    // const auto radius = 4.0;
    // const auto radiusAdjustThreshold = 12;

    QPen pen;

    auto rect = boundingRect();
    auto noteBoundingRect =
        QRectF(rect.left(), rect.top(), rect.width(), rect.height() - m_pronunciationTextHeight);
    auto left = noteBoundingRect.left() + penWidth;
    auto top = noteBoundingRect.top() + penWidth;
    auto width = noteBoundingRect.width() - penWidth * 2;
    auto height = noteBoundingRect.height() - penWidth * 2;
    auto paddedRect = QRectF(left, top, width, height);

    auto drawRectOnly = [&] {
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(Qt::NoPen);
        QColor brushColor;
        if (isSelected())
            brushColor = borderColorSelected;
        else if (m_editingPitch)
            brushColor = borderColorEditingPitch;
        else
            brushColor = backgroundColorNormal;
        painter->setBrush(brushColor);
        auto l = noteBoundingRect.left() + penWidth / 2;
        auto t = noteBoundingRect.top() + penWidth / 2;
        auto w = noteBoundingRect.width() - penWidth < 2 ? 2 : noteBoundingRect.width() - penWidth;
        auto h =
            noteBoundingRect.height() - penWidth < 2 ? 2 : noteBoundingRect.height() - penWidth;
        painter->drawRect(QRectF(l, t, w, h));
    };

    auto drawFullNote = [&] {
        QColor borderColor;
        if (isSelected())
            borderColor = borderColorSelected;
        else if (overlapped())
            borderColor = borderColorOverlapped;
        else if (m_editingPitch)
            borderColor = borderColorEditingPitch;
        else
            borderColor = borderColorNormal;
        pen.setColor(borderColor);
        pen.setWidthF(penWidth);
        painter->setPen(pen);
        painter->setBrush(m_editingPitch ? backgroundColorEditingPitch : backgroundColorNormal);
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

        pen.setColor(m_editingPitch ? foregroundColorEditingPitch : foregroundColorNormal);
        painter->setPen(pen);
        auto font = QFont();
        font.setPointSizeF(10);
        painter->setFont(font);
        int padding = 2;
        auto textRectLeft = paddedRect.left() + padding;
        // auto textRectTop = paddedRect.top() + padding;
        auto textRectTop = paddedRect.top();
        auto textRectWidth = paddedRect.width() - 2 * padding;
        // auto textRectHeight = paddedRect.height() - 2 * padding;
        auto textRectHeight = paddedRect.height();
        auto textRect = QRectF(textRectLeft, textRectTop, textRectWidth, textRectHeight);

        auto fontMetrics = painter->fontMetrics();
        auto textHeight = fontMetrics.height();
        auto text = m_lyric;
        auto textWidth = fontMetrics.horizontalAdvance(text);
        QTextOption textOption(Qt::AlignVCenter);
        textOption.setWrapMode(QTextOption::NoWrap);

        if (textWidth < textRectWidth && textHeight < textRectHeight) {
            // draw lryic
            painter->drawText(textRect, text, textOption);
            // draw pronunciation
            pen.setColor(m_pronunciationEdited ? pronunciationTextColorEdited
                                               : pronunciationTextColorOriginal);
            painter->setPen(pen);
            painter->drawText(QPointF(textRectLeft, boundingRect().bottom() - 6), m_pronunciation);
        }
    };

    if (scaleX() < 0.3)
        drawRectOnly();
    else
        drawFullNote();
}
void NoteView::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    m_menu->exec(event->screenPos());
}
// void NoteGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
//     CommonGraphicsRectItem::mousePressEvent(event);
//     if (!isSelected())
//         setSelected(true);
// }
void NoteView::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    // qDebug() << "NoteGraphicsItem::hoverMoveEvent" << event->pos().rx();
    if (!m_editingPitch) {
        const auto rx = event->pos().rx();
        if (rx >= 0 && rx <= AppGlobal::resizeTolarance ||
            rx >= rect().width() - AppGlobal::resizeTolarance && rx <= rect().width())
            setCursor(Qt::SizeHorCursor);
        else
            setCursor(Qt::ArrowCursor);
    }
    QGraphicsRectItem::hoverMoveEvent(event);
}
void NoteView::updateRectAndPos() {
    const auto x = (m_start + m_startOffset) * scaleX() * pixelsPerQuarterNote / 480;
    const auto y = -(m_keyIndex + m_keyOffset - 127) * noteHeight * scaleY();
    const auto w = (m_length + m_lengthOffset) * scaleX() * pixelsPerQuarterNote / 480;
    const auto h = noteHeight * scaleY() + m_pronunciationTextHeight;
    setPos(x, y);
    setRect(QRectF(0, 0, w, h));
    update();
}
void NoteView::initUi() {
    setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable);
}