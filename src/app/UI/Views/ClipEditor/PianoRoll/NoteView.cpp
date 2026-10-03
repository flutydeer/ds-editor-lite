#include "NoteView.h"

#include "NoteLyricPresentation.h"
#include "PianoRollGraphicsViewHelper.h"
#include "PronunciationView.h"
#include "Global/AppGlobal.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/AbstractGraphicsRectItem.h"
#include "UI/Views/Common/EditorItemGeometry.h"
#include "UI/Utils/AppColorPalette.h"
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Utils/IconUtils.h>

#include <QGraphicsSceneContextMenuEvent>
#include <QPainter>
#include <QTextOption>
#include <QMWidgets/cmenu.h>
#include <QDebug>
#include <QElapsedTimer>

using namespace ClipEditorGlobal;

// The inference-error badge hanging above the note, mirrored from the
// pronunciation view below. A child item so the area outside the note's
// bounding rect repaints correctly
class NoteErrorBadgeItem final : public QGraphicsItem {
public:
    explicit NoteErrorBadgeItem(QGraphicsItem *parent) : QGraphicsItem(parent) {
        // Never take part in item-level event delivery; the view hit-tests
        // the badge itself through noteErrorBadgeRect
        setAcceptedMouseButtons(Qt::NoButton);
        setPos(badgeOffset());
    }

    [[nodiscard]] QRectF boundingRect() const override {
        return {QPointF(0, 0), PianoRollGraphicsViewHelper::noteErrorBadgeSize()};
    }

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget) override {
        const auto markColor =
            ThemeManager::instance()->semanticColor(QStringLiteral("piano.roll.noteErrorMark"));
        if (!markColor.isValid() || markColor.alpha() == 0)
            return;
        const auto badgeSize = PianoRollGraphicsViewHelper::noteErrorBadgeSize();
        const auto dpr = widget ? widget->devicePixelRatioF() : 1.0;
        const auto pixmap =
            IconUtils::renderTintedSvgPixmap(QStringLiteral(":svg/icons/dismiss_circle_16_regular.svg"),
                                             badgeSize.toSize(), markColor, dpr);
        if (pixmap.isNull())
            return;
        painter->drawPixmap(QRectF(QPointF(0, 0), badgeSize), pixmap,
                            QRectF(QPointF(), QSizeF(pixmap.size())));
    }

private:
    [[nodiscard]] static QPointF badgeOffset() {
        constexpr double margin = 2.0;
        return {EditorItemGeometry::noteBorderWidth + margin,
                -PianoRollGraphicsViewHelper::noteErrorBadgeSize().height() - margin};
    }
};

int NoteView::s_trackColorIndex = 0;
QColor NoteView::s_selectedBorderColor = {255, 255, 255};

int NoteView::trackColorIndex() {
    return s_trackColorIndex;
}

void NoteView::setTrackColorIndex(int index) {
    s_trackColorIndex = index;
}

QColor NoteView::selectedBorderColor() {
    return s_selectedBorderColor;
}

void NoteView::setSelectedBorderColor(const QColor &color) {
    s_selectedBorderColor = color;
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

bool NoteView::isLyricElided(const QRectF &visibleSceneRect) const {
    if (m_editingLyric)
        return false;
    const auto font = lyricFont();
    const auto layout = NoteLyricPresentation::layout(boundingRect(), m_lyric, font, scaleX());
    return NoteLyricPresentation::isElidedInRect(layout, m_lyric, font,
                                                 mapRectFromScene(visibleSceneRect));
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

void NoteView::setInferenceError(const bool on) {
    if (m_inferenceError == on)
        return;
    m_inferenceError = on;
    m_errorBadge->setVisible(on);
}

bool NoteView::hasInferenceError() const {
    return m_inferenceError;
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
    const auto borderColorSelected = s_selectedBorderColor;
    const auto borderColorOverlapped = p.noteBorderOverlapped(ci);
    const auto borderColorEditingPitch = p.noteBorderEditingPitch(ci);

    const auto foregroundColorNormal = p.noteForeground(ci);
    const auto foregroundColorSelected = p.noteForeground(ci);
    const auto foregroundColorEditingPitch = p.noteForegroundEditingPitch(ci);
    const auto foregroundColorOverlapped = p.noteForegroundOverlapped(ci);

    constexpr auto penWidth = EditorItemGeometry::noteBorderWidth;
    QPen pen;

    const auto rect = boundingRect();
    const auto paddedRect = EditorItemGeometry::notePaintRect(rect);
    const auto cornerRadius =
        EditorItemGeometry::adaptiveCornerRadius(paddedRect, EditorItemGeometry::noteCornerRadius);

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
        painter->drawRoundedRect(paddedRect, cornerRadius, cornerRadius);

        pen.setColor(foregroundColor);
        painter->setPen(pen);
        const auto font = lyricFont();
        painter->setFont(font);
        const auto layout = NoteLyricPresentation::layout(rect, m_lyric, font, scaleX());

        auto fontMetrics = painter->fontMetrics();
        auto textHeight = fontMetrics.height();
        auto pronTextWidth = fontMetrics.horizontalAdvance(m_pronunciation);
        QTextOption textOption(Qt::AlignVCenter);
        textOption.setWrapMode(QTextOption::NoWrap);

        if (!m_editingLyric && layout.isVisible())
            painter->drawText(layout.textRect, layout.displayText, textOption);
        if (m_pronView)
            m_pronView->setTextVisible(!m_editingLyric && pronTextWidth < layout.textRect.width() &&
                                       textHeight < layout.textRect.height());
    };

    if (NoteLyricPresentation::usesCompactRendering(scaleX()))
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
    m_errorBadge = new NoteErrorBadgeItem(this);
    m_errorBadge->setVisible(false);
    fontPixelSize.onChanged([this](int) { update(); });
}

QFont NoteView::lyricFont() const {
    QFont font;
    font.setPixelSize(fontPixelSize);
    return font;
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
