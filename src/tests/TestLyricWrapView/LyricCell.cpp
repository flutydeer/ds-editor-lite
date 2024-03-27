#include "LyricCell.h"

#include <QDebug>
#include <QFontMetrics>

#include <QGraphicsRectItem>
#include <QStyleOptionGraphicsItem>

#include <QGraphicsSceneMouseEvent>
#include <QLineEdit>

namespace LyricWrap {
    LyricCell::LyricCell(const qreal &x, const qreal &y, LangNote *note, QGraphicsItem *parent)
        : mX(x), mY(y), m_note(note), QGraphicsItem(parent) {
        setFlag(ItemIsSelectable);
        this->setAcceptHoverEvents(true);
        this->updateWidth();
        this->updateHeight();
    }

    LyricCell::~LyricCell() = default;


    QRectF LyricCell::boundingRect() const {
        return QRectF(mX, mY, m_width, m_height);
    }

    void LyricCell::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
        const QPointF clickPos = event->scenePos();
        const auto lyricTextRect =
            QRectF(rectPos().x(), rectPos().y(), m_width - m_padding * 2, lyricHeight() * 1.6);

        if (lyricTextRect.contains(clickPos)) {
            m_lyricEdit->setText(m_note->lyric);
            m_lyricEdit->setFocus();
            isLyricEditing = true;
            m_lyricWidget->setWidget(m_lyricEdit.data());
            m_lyricWidget->setPos(lyricPos().x(), rectPos().y() + m_padding / 2);
            m_lyricWidget->widget()->setFocus();
            scene()->addItem(m_lyricWidget.data());
            connect(m_lyricEdit.data(), &QLineEdit::editingFinished, this, &LyricCell::updateLyric);
            update();
            event->accept();
        }
        return QGraphicsItem::mouseDoubleClickEvent(event);
    }

    void LyricCell::updateLyric() {
        this->setLyric(m_lyricEdit->text());
        isLyricEditing = false;
        disconnect(m_lyricEdit.data(), &QLineEdit::editingFinished, this, &LyricCell::updateLyric);
        scene()->removeItem(m_lyricWidget.data());
    }

    qreal LyricCell::width() const {
        return m_width;
    }

    qreal LyricCell::height() const {
        return m_height;
    }

    void LyricCell::setPos(const qreal &x, const qreal &y) {
        mX = x;
        mY = y;
        update();
    }

    void LyricCell::setLyric(const QString &lyric) {
        m_note->lyric = lyric;
        this->updateWidth();
        update();
    }


    void LyricCell::updateWidth() {
        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        m_width = std::max(lyricWidth(), syllableWidth()) + m_padding * 2 + m_rectPadding * 2;
        Q_EMIT this->updateWidthSignal(m_width);
    }

    void LyricCell::updateHeight() {
        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);

        const auto lyricHeight = QFontMetrics(m_font).height();
        const auto syllableHeight = QFontMetrics(syllableFont).height();

        m_height = m_padding * 2 + syllableHeight * 1.2 + m_lsPadding + lyricHeight * 1.6;
    }

    int LyricCell::syllableWidth() const {
        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        return QFontMetrics(syllableFont).boundingRect(m_note->syllable).width();
    }

    int LyricCell::syllableHeight() const {
        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        return QFontMetrics(syllableFont).boundingRect(m_note->syllable).height();
    }

    int LyricCell::lyricWidth() const {
        const auto lyricxHeight = QFontMetrics(m_font).xHeight();
        return std::max(lyricxHeight, QFontMetrics(m_font).boundingRect(m_note->lyric).width());
    }

    int LyricCell::lyricHeight() const {
        return QFontMetrics(m_font).height();
    }

    QPointF LyricCell::syllablePos() const {
        return {mX + m_width / 2 - syllableWidth() / 2, mY + m_padding + syllableHeight() * 0.6};
    }

    QPointF LyricCell::lyricPos() const {
        const auto rPos = rectPos();
        return {mX + m_width / 2 - lyricWidth() / 2, rPos.y() + m_lsPadding + lyricHeight() * 0.8};
    }

    QPointF LyricCell::rectPos() const {
        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        const auto syllableHeight = QFontMetrics(syllableFont).height();
        return {mX + m_padding, mY + m_padding + syllableHeight + m_lsPadding};
    }

    void LyricCell::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
        int flag = 0;
        if (option->state & QStyle::State_MouseOver)
            flag = LyricCell::Hovered;
        if (option->state & QStyle::State_Selected)
            flag = LyricCell::Selected;

        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);

        painter->setFont(syllableFont);
        painter->setPen(m_syllablePen[flag]);
        painter->drawText(syllablePos(), m_note->syllable);

        const auto rPos = rectPos();
        const auto boxRect =
            QRectF(rPos.x(), rPos.y(), m_width - m_padding * 2, lyricHeight() * 1.6);

        painter->setBrush(m_backgroundBrush[flag]);
        painter->setPen(m_borderPen[flag]);
        painter->drawRoundedRect(boxRect, m_padding * 0.5, m_padding * 0.5);

        painter->setFont(m_font);
        painter->setPen(m_lyricPen[flag]);
        if (!isLyricEditing)
            painter->drawText(lyricPos(), m_note->lyric);
    }
}