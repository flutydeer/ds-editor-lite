//
// Created by fluty on 2024/1/23.
//

#include "NoteView.h"

#include "PronunciationView.h"
#include "Global/AppGlobal.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/AbstractGraphicsRectItem.h"
#include "UI/Utils/AppColorPalette.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QPainter>
#include <QTextOption>
#include <QMWidgets/cmenu.h>
#include <QDebug>
#include <QElapsedTimer>

using namespace ClipEditorGlobal;

int NoteView::s_trackColorIndex = 0;

int NoteView::trackColorIndex() {
    return s_trackColorIndex;
}

void NoteView::setTrackColorIndex(int index) {
    s_trackColorIndex = index;
}

NoteView::NoteView(const int itemId, QGraphicsItem *parent)
    : AbstractGraphicsRectItem(parent), UniqueObject(itemId) {
    initUi();
}

NoteView::~NoteView() {
    delete m_pronView;
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

    const auto &p = *AppColorPalette::instance();
    const int ci = s_trackColorIndex;
    const auto backgroundColorNormal = p.noteBackground(ci);
    const auto backgroundColorSelected = p.noteBackgroundSelected(ci);
    const auto backgroundColorEditingPitch = p.noteBackgroundEditingPitch(ci);
    const auto backgroundColorOverlapped = p.noteBackgroundOverlapped(ci);

    const auto borderColorNormal = p.noteBorder(ci);
    constexpr auto borderColorSelected = QColor(255, 255, 255);
    const auto borderColorOverlapped = p.noteBorderOverlapped(ci);
    const auto borderColorEditingPitch = p.noteBorderEditingPitch(ci);

    const auto foregroundColorNormal = p.noteForeground(ci);
    const auto foregroundColorSelected = p.noteForeground(ci);
    const auto foregroundColorEditingPitch = p.noteForegroundEditingPitch(ci);
    const auto foregroundColorOverlapped = p.noteForegroundOverlapped(ci);

    constexpr auto penWidth = 1.5f;
    constexpr int padding = 2;

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
            brushColor = backgroundColorSelected;
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
            backgroundColor = backgroundColorSelected;
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
        painter->drawRoundedRect(paddedRect, 2, 2);

        pen.setColor(foregroundColor);
        painter->setPen(pen);
        auto font = QFont();
        font.setPixelSize(fontPixelSize);
        painter->setFont(font);
        auto textRectLeft = paddedRect.left() + padding;
        auto textRectTop = paddedRect.top();
        auto textRectWidth = paddedRect.width() - 2 * padding;
        auto textRectHeight = paddedRect.height();
        auto textRect = QRectF(textRectLeft, textRectTop, textRectWidth, textRectHeight);

        auto fontMetrics = painter->fontMetrics();
        auto textHeight = fontMetrics.height();
        auto lyricTextWidth = fontMetrics.horizontalAdvance(m_lyric);
        auto pronTextWidth = fontMetrics.horizontalAdvance(m_pronunciation);
        QTextOption textOption(Qt::AlignVCenter);
        textOption.setWrapMode(QTextOption::NoWrap);

        if (!m_editingLyric && qMax(lyricTextWidth, pronTextWidth) < textRectWidth &&
            textHeight < textRectHeight) {
            painter->drawText(textRect, m_lyric, textOption);
            if (m_pronView) {
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
}

void NoteView::updateRectAndPos() {
    const auto x = (m_rStart + m_startOffset) * scaleX() * pixelsPerQuarterNote /
                   AppGlobal::ticksPerQuarterNote;
    const auto y = -(m_keyIndex + m_keyOffset - 127) * noteHeight * scaleY();
    const auto w = (m_length + m_lengthOffset) * scaleX() * pixelsPerQuarterNote /
                   AppGlobal::ticksPerQuarterNote;
    const auto h = noteHeight * scaleY();
    setPos(x, y);
    setRect(QRectF(0, 0, w, h));
    if (m_pronView)
        adjustPronView();

    update();
}

void NoteView::adjustPronView() const {
    m_pronView->setPos(pos().x(), pos().y() + boundingRect().height());
    m_pronView->setRect(QRectF(0, 0, boundingRect().width(), m_pronView->textHeight));
}

void NoteView::initUi() {
    setFlag(ItemIsSelectable);
    fontPixelSize.onChanged([this](int) { update(); });
}

void NoteView::setEditingLyric(const bool editing) {
    if (m_editingLyric == editing)
        return;
    m_editingLyric = editing;
    update();
}

bool NoteView::isEditingLyric() const {
    return m_editingLyric;
}
