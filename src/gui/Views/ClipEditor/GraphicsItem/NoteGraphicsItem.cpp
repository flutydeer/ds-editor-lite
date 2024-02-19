//
// Created by fluty on 2024/1/23.
//

#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QTextOption>

#include "NoteGraphicsItem.h"
#include "Views/Common/CommonGraphicsRectItem.h"
#include "Utils/AppGlobal.h"
#include "Views/ClipEditor/ClipEditorGlobal.h"

using namespace ClipEditorGlobal;

NoteGraphicsItem::NoteGraphicsItem(int itemId, QGraphicsItem *parent)
    : CommonGraphicsRectItem(parent), UniqueObject(itemId) {
    initUi();
}
NoteGraphicsItem::NoteGraphicsItem(int itemId, int start, int length, int keyIndex,
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
QWidget *NoteGraphicsItem::context() const {
    return m_context;
}
void NoteGraphicsItem::setContext(QWidget *context) {
    m_context = context;
    m_menu = new QMenu(m_context);
    auto actionRemove = m_menu->addAction("Remove");
    connect(actionRemove, &QAction::triggered, [&] { emit removeTriggered(id()); });

    m_menu->addSeparator();

    auto actionEditLyric = m_menu->addAction("Edit lyric");
    connect(actionEditLyric, &QAction::triggered, [&] { emit editLyricTriggered(id()); });

    // auto actionProperties = m_menu->addAction("Properties");
    // connect(actionProperties, &QAction::triggered,
    //         [&] { qDebug() << "actionProperties triggered" << id(); });
}
int NoteGraphicsItem::start() const {
    return m_start;
}
void NoteGraphicsItem::setStart(int start) {
    m_start = start;
    updateRectAndPos();
}
int NoteGraphicsItem::length() const {
    return m_length;
}
void NoteGraphicsItem::setLength(int length) {
    m_length = length;
    updateRectAndPos();
}
int NoteGraphicsItem::keyIndex() const {
    return m_keyIndex;
}
void NoteGraphicsItem::setKeyIndex(int keyIndex) {
    m_keyIndex = keyIndex;
    updateRectAndPos();
}
QString NoteGraphicsItem::lyric() const {
    return m_lyric;
}
void NoteGraphicsItem::setLyric(const QString &lyric) {
    m_lyric = lyric;
    update();
}
QString NoteGraphicsItem::pronunciation() const {
    return m_pronunciation;
}
void NoteGraphicsItem::setPronunciation(const QString &pronunciation) {
    m_pronunciation = pronunciation;
    update();
}
bool NoteGraphicsItem::editingPitch() const {
    return m_editingPitch;
}
void NoteGraphicsItem::setEditingPitch(bool on) {
    m_editingPitch = on;
    update();
}
int NoteGraphicsItem::pronunciationTextHeight() const {
    return m_pronunciationTextHeight;
}
int NoteGraphicsItem::startOffset() const {
    return m_startOffset;
}
void NoteGraphicsItem::setStartOffset(int tick) {
    m_startOffset = tick;
    updateRectAndPos();
}
int NoteGraphicsItem::lengthOffset() const {
    return m_lengthOffset;
}
void NoteGraphicsItem::setLengthOffset(int tick) {
    m_lengthOffset = tick;
    updateRectAndPos();
}
int NoteGraphicsItem::keyOffset() const {
    return m_keyOffset;
}
void NoteGraphicsItem::setKeyOffset(int key) {
    m_keyOffset = key;
    updateRectAndPos();
}
void NoteGraphicsItem::resetOffset() {
    m_startOffset = 0;
    m_lengthOffset = 0;
    m_keyOffset = 0;
    updateRectAndPos();
}
void NoteGraphicsItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                             QWidget *widget) {
    const auto backgroundColorNormal = QColor(155, 186, 255);
    const auto backgroundColorEditingPitch = QColor(155, 186, 255, 50);

    const auto borderColorNormal = QColor(112, 156, 255);
    const auto borderColorSelected = QColor(255, 255, 255);
    const auto borderColorOverlapped = AppGlobal::overlappedViewBorder;
    const auto borderColorEditingPitch = QColor(155, 186, 255, 220);

    const auto foregroundColorNormal = QColor(0, 0, 0);
    const auto foregroundColorEditingPitch = QColor(155, 186, 255, 220);

    const auto pronunciationTextColor = QColor(200, 200, 200);

    const auto penWidth = 2.0f;
    const auto radius = 4.0;
    const auto radiusAdjustThreshold = 12;

    QPen pen;
    if (isSelected())
        pen.setColor(borderColorSelected);
    else if (m_overlapped)
        pen.setColor(borderColorOverlapped);
    else if (m_editingPitch)
        pen.setColor(borderColorEditingPitch);
    else
        pen.setColor(borderColorNormal);

    auto rect = boundingRect();
    auto noteBoundingRect =
        QRectF(rect.left(), rect.top(), rect.width(), rect.height() - m_pronunciationTextHeight);
    auto left = noteBoundingRect.left() + penWidth;
    auto top = noteBoundingRect.top() + penWidth;
    auto width = noteBoundingRect.width() - penWidth * 2;
    auto height = noteBoundingRect.height() - penWidth * 2;
    auto paddedRect = QRectF(left, top, width, height);

    pen.setWidthF(penWidth);
    painter->setPen(pen);
    painter->setBrush(m_editingPitch ? backgroundColorEditingPitch : backgroundColorNormal);

    if (paddedRect.width() < 8 || paddedRect.height() < 8) { // draw rect without border
        painter->setRenderHint(QPainter::Antialiasing, false);
        painter->setPen(Qt::NoPen);
        painter->setBrush(isSelected() ? borderColorSelected : backgroundColorNormal);
        auto l = noteBoundingRect.left() + penWidth / 2;
        auto t = noteBoundingRect.top() + penWidth / 2;
        auto w = noteBoundingRect.width() - penWidth < 2 ? 2 : noteBoundingRect.width() - penWidth;
        auto h =
            noteBoundingRect.height() - penWidth < 2 ? 2 : noteBoundingRect.height() - penWidth;
        painter->drawRect(QRectF(l, t, w, h));
    } else {
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
    }

    pen.setColor(m_editingPitch ? foregroundColorEditingPitch : foregroundColorNormal);
    painter->setPen(pen);
    auto font = QFont();
    font.setPointSizeF(10);
    painter->setFont(font);
    int padding = 2;
    // TODO: keep lyric on screen
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
        pen.setColor(pronunciationTextColor);
        painter->setPen(pen);
        painter->drawText(QPointF(textRectLeft, boundingRect().bottom() - 6), m_pronunciation);
    }
}
void NoteGraphicsItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    m_menu->exec(event->screenPos());
}
// void NoteGraphicsItem::mousePressEvent(QGraphicsSceneMouseEvent *event) {
//     CommonGraphicsRectItem::mousePressEvent(event);
//     if (!isSelected())
//         setSelected(true);
// }
void NoteGraphicsItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event) {
    const auto rx = event->pos().rx();
    if (rx >= 0 && rx <= AppGlobal::resizeTolarance ||
        rx >= rect().width() - AppGlobal::resizeTolarance && rx <= rect().width())
        setCursor(Qt::SizeHorCursor);
    else
        setCursor(Qt::ArrowCursor);

    QGraphicsRectItem::hoverMoveEvent(event);
}
void NoteGraphicsItem::updateRectAndPos() {
    const auto x = (m_start + m_startOffset) * scaleX() * pixelsPerQuarterNote / 480;
    const auto y = -(m_keyIndex + m_keyOffset - 127) * noteHeight * scaleY();
    const auto w = (m_length + m_lengthOffset) * scaleX() * pixelsPerQuarterNote / 480;
    const auto h = noteHeight * scaleY() + m_pronunciationTextHeight;
    setPos(x, y);
    setRect(QRectF(0, 0, w, h));
    update();
}
void NoteGraphicsItem::initUi() {
    setAcceptHoverEvents(true);
    setFlag(ItemIsSelectable);
}