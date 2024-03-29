#include "LyricCell.h"

#include <QDebug>
#include <QMenu>

#include <QScrollBar>
#include <QInputDialog>
#include <QGraphicsRectItem>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>

#include "EditDialog.h"

namespace FillLyric {
    LyricCell::LyricCell(const qreal &x, const qreal &y, LangNote *note, QGraphicsView *view,
                         QGraphicsItem *parent)
        : QGraphicsObject(parent), m_note(note), m_view(view) {
        this->setX(x);
        this->setY(y);
        setFlag(ItemIsSelectable);
        this->setAcceptHoverEvents(true);

        this->updateLyricRect();
    }

    LyricCell::~LyricCell() = default;

    QRectF LyricCell::boundingRect() const {
        return {0, 0, width(), height()};
    }

    qreal LyricCell::width() const {
        return std::max(lyricWidth(), syllableWidth()) + m_padding * 2 + m_reckBorder * 2 +
               m_rectPadding * 2;
    }

    qreal LyricCell::height() const {
        return m_padding * 2 + m_sRect.height() + m_lsPadding + m_lRect.height() +
               m_reckBorder * 2 + m_rectPadding * 2;
    }

    LangNote *LyricCell::note() const {
        return m_note;
    }

    void LyricCell::setNote(LangNote *note) {
        m_note = note;
    }

    QString LyricCell::lyric() const {
        return m_note->lyric;
    }

    void LyricCell::setLyric(const QString &lyric) const {
        Q_EMIT this->updateLyric(lyric);
    }

    QString LyricCell::syllable() const {
        return m_note->syllable;
    }

    void LyricCell::setSyllable(const QString &syllable) const {
        Q_EMIT this->changeSyllable(syllable);
    }

    void LyricCell::setFont(const QFont &font) {
        m_font = font;
    }

    void LyricCell::setLyricRect(const QRect &rect) {
        m_lRect = rect;
    }

    void LyricCell::setSyllableRect(const QRect &rect) {
        m_sRect = rect;
    }

    void LyricCell::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
        if (lyricRect().contains(event->scenePos())) {
            const auto lRect = lyricRect();
            const int horizontalOffset = m_view->horizontalScrollBar()->value();
            const int verticalOffset = m_view->verticalScrollBar()->value();
            const auto editRect = QRectF(lRect.x() - horizontalOffset, lRect.y() - verticalOffset,
                                         lRect.width(), lRect.height());
            EditDialog dlg(lyric(), editRect, m_view);
            dlg.show();
            dlg.activateWindow();
            dlg.exec();
            if (dlg.text != lyric()) {
                this->setLyric(dlg.text);
                Q_EMIT this->updateWidth(width());
            }

            update();
            event->accept();
        }
        return QGraphicsItem::mouseDoubleClickEvent(event);
    }

    void LyricCell::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
        if (lyricRect().contains(event->scenePos())) {
            auto *menu = new QMenu(m_view);
            this->changeSyllableMenu(menu);
            this->changePhonicMenu(menu);
            menu->addSeparator();
            menu->addAction("clear cell", [this] { Q_EMIT this->clearCell(); });
            if (x() != 0)
                menu->addAction("delete cell", [this] { Q_EMIT this->deleteCell(); });
            menu->addAction("add prev cell", [this] { Q_EMIT this->addPrevCell(); });
            menu->addAction("add next cell", [this] { Q_EMIT this->addNextCell(); });
            menu->addSeparator();
            menu->addAction("delete line", [this] { Q_EMIT this->deleteLine(); });
            menu->addAction("add prev line", [this] { Q_EMIT this->addPrevLine(); });
            menu->addAction("add next line", [this] { Q_EMIT this->addNextLine(); });
            menu->exec(event->screenPos());
            event->accept();
        }
        return QGraphicsItem::contextMenuEvent(event);
    }

    void LyricCell::updateLyricRect() {
        const auto lyric = m_note->lyric.isEmpty() ? " " : m_note->lyric;
        const auto syllable = m_note->syllable.isEmpty() ? " " : m_note->syllable;

        m_lRect = QFontMetrics(m_font).boundingRect(lyric);

        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        m_sRect = QFontMetrics(syllableFont).boundingRect(syllable);
    }


    qreal LyricCell::lyricWidth() const {
        return m_lRect.width() + 10;
    }

    qreal LyricCell::syllableWidth() const {
        return m_sRect.width() + 10;
    }

    QPointF LyricCell::lyricPos() const {
        const auto rPos = rectPos();
        return {width() / 2 - lyricWidth() / 2, rPos.y() + m_rectPadding};
    }

    QPointF LyricCell::rectPos() const {
        return {m_padding, m_padding + m_sRect.height() + m_lsPadding};
    }

    QPointF LyricCell::syllablePos() const {
        return {width() / 2 - syllableWidth() / 2, m_padding};
    }

    QRectF LyricCell::lyricRect() const {
        return {x() + rectPos().x(), y() + rectPos().y(), width() - m_padding * 2,
                m_lRect.height() + m_rectPadding * 2};
    }

    void LyricCell::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                          QWidget *widget) {
        int flag = 0;
        if (option->state & QStyle::State_MouseOver)
            flag = LyricCell::Hovered;
        if (option->state & QStyle::State_Selected)
            flag = LyricCell::Selected;

        int lyricFlag = 0;
        const auto syllable = m_note->revised ? m_note->syllableRevised : m_note->syllable;
        if (m_note->revised) {
            lyricFlag = PenType::Revised;
        } else if (m_note->g2pError) {
            lyricFlag = PenType::G2pError;
        } else if (m_note->candidates.size() > 1) {
            lyricFlag = PenType::Multitone;
        }

        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);

        const auto sPos = syllablePos();
        painter->setFont(syllableFont);
        painter->setPen(m_syllablePen[lyricFlag]);
        painter->drawText(QRectF(sPos.x() + 5, sPos.y(), syllableWidth(), m_sRect.height()),
                          syllable);

        const auto rPos = rectPos();
        const auto boxRect = QRectF(rPos.x(), rPos.y(), width() - m_padding * 2,
                                    m_lRect.height() + m_rectPadding * 2);

        painter->setBrush(m_backgroundBrush[flag]);
        painter->setPen(m_borderPen[flag]);
        painter->drawRoundedRect(boxRect, m_padding * 0.5, m_padding * 0.5);

        painter->setFont(m_font);
        painter->setPen(m_lyricPen[lyricFlag]);

        const auto lPos = lyricPos();
        painter->drawText(QRectF(lPos.x() + 5, lPos.y(), lyricWidth(), m_lRect.height()),
                          m_note->lyric);
    }

    void LyricCell::changePhonicMenu(QMenu *menu) {
        auto *inputAction = new QAction(tr("Custom Syllables"), this);
        menu->addAction(inputAction);
        connect(inputAction, &QAction::triggered, this, [this]() {
            bool ok;
            const QString syllable =
                QInputDialog::getText(m_view, tr("Custom Syllables"), tr("Please input syllables"),
                                      QLineEdit::Normal, "", &ok);
            if (ok && !syllable.isEmpty()) {
                this->setSyllable(syllable);
            }
        });
    }

    void LyricCell::changeSyllableMenu(QMenu *menu) const {
        QStringList candidateSyllables = m_note->candidates;

        for (const auto &syllable : candidateSyllables) {
            if (candidateSyllables.size() > 1) {
                menu->addAction(syllable, [this, syllable]() { this->setSyllable(syllable); });
            }
        }
    }

}