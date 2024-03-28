#include "LyricCell.h"

#include <QDebug>
#include <QMenu>

#include <QGraphicsRectItem>
#include <QStyleOptionGraphicsItem>

#include <QGraphicsSceneMouseEvent>
#include <QLineEdit>

namespace LyricWrap {
    LyricCell::LyricCell(const qreal &x, const qreal &y, LangNote *note, QGraphicsItem *parent)
        : QGraphicsObject(parent), mX(x), mY(y), m_note(note) {
        setFlag(ItemIsSelectable);
        this->setAcceptHoverEvents(true);
    }

    LyricCell::~LyricCell() = default;


    QRectF LyricCell::boundingRect() const {
        return {mX, mY, width(), height()};
    }

    void LyricCell::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
        if (lyricRect().contains(event->scenePos())) {
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

    void LyricCell::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
        if (lyricRect().contains(event->scenePos())) {
            QMenu menu;
            QAction *action = menu.addAction("Do Something");
            menu.exec(event->screenPos());
            event->accept();
        }
        return QGraphicsItem::contextMenuEvent(event);
    }

    void LyricCell::updateLyric() {
        this->setLyric(m_lyricEdit->text());
        isLyricEditing = false;
        disconnect(m_lyricEdit.data(), &QLineEdit::editingFinished, this, &LyricCell::updateLyric);
        scene()->removeItem(m_lyricWidget.data());
        Q_EMIT this->updateWidthSignal(width());
    }

    qreal LyricCell::width() const {
        return std::max(lyricWidth(), syllableWidth()) + m_padding * 2 + m_reckBorder * 2 +
               m_rectPadding * 2;
    }

    qreal LyricCell::height() const {
        return m_padding * 2 + m_syllableHeight + m_lsPadding + m_lyricHeight + m_reckBorder * 2 +
               m_rectPadding * 2;
    }

    void LyricCell::setPos(const qreal &x, const qreal &y) {
        mX = x;
        mY = y;
    }

    void LyricCell::setLyric(const QString &lyric) {
        m_note->lyric = lyric;
        Q_EMIT this->updateWidthSignal(width());
        update();
    }

    void LyricCell::setFontSize(const QFont &font, const qreal &lw, const qreal &lh,
                                const qreal &sw, const qreal &sh) {
        m_font = font;
        m_lyricXHeight = lw;
        m_lyricHeight = lh;
        m_syllableXHeight = sw;
        m_syllableHeight = sh;
        Q_EMIT this->updateWidthSignal(width());
    }

    qreal LyricCell::syllableWidth() const {
        return m_syllableXHeight * (static_cast<qreal>(m_note->syllable.size()) + 2);
    }

    qreal LyricCell::lyricWidth() const {
        return m_lyricXHeight * (static_cast<qreal>(m_note->lyric.size()) + 2);
    }

    QPointF LyricCell::syllablePos() const {
        return {mX + width() / 2 - syllableWidth() / 2, mY + m_padding};
    }

    QPointF LyricCell::lyricPos() const {
        const auto rPos = rectPos();
        return {mX + width() / 2 - lyricWidth() / 2, rPos.y() + m_rectPadding};
    }

    QRectF LyricCell::lyricRect() const {
        return {rectPos().x(), rectPos().y(), width() - m_padding * 2,
                m_lyricHeight + m_rectPadding * 2};
    }

    QPointF LyricCell::rectPos() const {
        return {mX + m_padding, mY + m_padding + m_syllableHeight + m_lsPadding};
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

        const auto sPos = syllablePos();
        painter->setFont(syllableFont);
        painter->setPen(m_syllablePen[flag]);
        painter->drawText(QRectF(sPos.x(), sPos.y(), syllableWidth(), m_syllableHeight),
                          m_note->syllable);

        const auto rPos = rectPos();
        const auto boxRect =
            QRectF(rPos.x(), rPos.y(), width() - m_padding * 2, m_lyricHeight + m_rectPadding * 2);

        painter->setBrush(m_backgroundBrush[flag]);
        painter->setPen(m_borderPen[flag]);
        painter->drawRoundedRect(boxRect, m_padding * 0.5, m_padding * 0.5);

        painter->setFont(m_font);
        painter->setPen(m_lyricPen[flag]);
        if (!isLyricEditing) {
            const auto lPos = lyricPos();
            painter->drawText(QRectF(lPos.x(), lPos.y(), lyricWidth(), m_lyricHeight),
                              m_note->lyric);
        }
    }
}