#include "Modules/FillLyric/Controls/LyricCell.h"

#include <QMenu>

#include <QGraphicsSceneMouseEvent>
#include <QInputDialog>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>

#include "Modules/FillLyric/Controls/EditDialog.h"

namespace FillLyric
{
    LyricCell::LyricCell(const qreal &x, const qreal &y, LangNote *note, QGraphicsView *view, CellQss *qss,
                         QList<LyricCell *> *cells, QGraphicsItem *parent) :
        QGraphicsObject(parent), m_qss(qss), m_cells(cells), m_note(note), m_view(view) {
        this->setX(x);
        this->setY(y);
        setFlag(ItemIsSelectable);
        this->setAcceptHoverEvents(true);
        this->setQss(qss);

        this->updateLyricRect();
    }

    LyricCell::~LyricCell() { delete m_note; }

    void LyricCell::clear() {
        delete m_note;
        m_note = new LangNote();
        Q_EMIT this->updateWidth(width());
        update();
    }

    QPainterPath LyricCell::shape() const {
        QPainterPath path;
        path.addRect({syllablePos().x(), syllablePos().y(), syllableWidth(), static_cast<qreal>(m_sRect.height())});
        path.addRect({rectPos().x(), rectPos().y(), width() - m_padding * 2, m_lRect.height() + m_rectPadding * 2});
        return path;
    }

    QRectF LyricCell::boundingRect() const { return {0, 0, width(), height()}; }

    qreal LyricCell::width() const {
        return std::max(lyricWidth(), syllableWidth()) + m_padding * 2 + m_rectBorder * 2 + m_rectPadding * 2;
    }

    qreal LyricCell::height() const {
        return m_padding + m_sRect.height() + m_lsPadding + m_lRect.height() + m_rectBorder * 2 + m_rectPadding * 2;
    }

    LangNote *LyricCell::note() const { return m_note; }

    void LyricCell::setNote(LangNote *note) {
        delete m_note;
        m_note = note;
    }

    QString LyricCell::lyric() const { return m_note->lyric; }

    void LyricCell::setLyric(const QString &lyric) { Q_EMIT this->updateLyric(this, lyric); }

    QString LyricCell::syllable() const { return m_note->syllable; }

    void LyricCell::setSyllable(const QString &syllable) { Q_EMIT this->changeSyllable(this, syllable); }

    qreal LyricCell::margin() const { return m_rectPadding; }

    void LyricCell::setMargin(const qreal &margin) { m_rectPadding = margin; }

    void LyricCell::setFont(const QFont &font) {
        m_font = font;
        m_syllableFont = font;
        if (m_syllableFont.pointSize() - 3 >= 0)
            m_syllableFont.setPointSize(m_syllableFont.pointSize() - 3);
        m_syllableFontBold = m_syllableFont;
        m_syllableFontBold.setWeight(QFont::Bold);
    }

    void LyricCell::setLyricRect(const QRect &rect) { m_lRect = rect; }

    void LyricCell::setSyllableRect(const QRect &rect) { m_sRect = rect; }

    void LyricCell::handleMouseSelection(QGraphicsSceneMouseEvent *event) {
        if (event->button() == Qt::RightButton && scene()->selectedItems().size() > 1 && this->isSelected()) {
            event->accept();
            return;
        }

        if (!(event->modifiers() & Qt::ControlModifier || event->modifiers() & Qt::ShiftModifier)) {
            for (const auto item : scene()->selectedItems()) {
                item->setSelected(false);
            }
        } else {
            for (const auto item : scene()->selectedItems()) {
                if (!dynamic_cast<LyricCell *>(item))
                    item->setSelected(false);
            }
        }
        this->setSelected(true);
    }

    void LyricCell::mousePressEvent(QGraphicsSceneMouseEvent *event) { handleMouseSelection(event); }

    void LyricCell::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) { handleMouseSelection(event); }

    void LyricCell::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) {
        if (event->button() == Qt::LeftButton && lyricRect().contains(event->scenePos())) {
            const auto lRect = lyricRect();
            const int horizontalOffset = m_view->horizontalScrollBar()->value();
            const int verticalOffset = m_view->verticalScrollBar()->value();
            const auto editRect =
                QRectF(lRect.x() - horizontalOffset, lRect.y() - verticalOffset, lRect.width(), lRect.height());
            EditDialog dlg(lyric(), editRect, m_font, m_view);
            dlg.show();
            dlg.activateWindow();
            dlg.exec();
            if (dlg.result() == QDialog::Accepted && dlg.text() != lyric()) {
                this->setLyric(dlg.text());
                Q_EMIT this->updateWidth(width());
            }

            update();
            event->accept();
        }
        return QGraphicsItem::mouseDoubleClickEvent(event);
    }

    void LyricCell::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
        auto *menu = new QMenu(m_view);
        menu->setAttribute(Qt::WA_TranslucentBackground);
        menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        this->changeSyllableMenu(menu);
        this->changePhonicMenu(menu);

        const auto lRect = QRectF{x() + rectPos().x(), y() + rectPos().y(), width() - m_padding * 2,
                                  m_lRect.height() + m_rectPadding * 2};

        if (lRect.contains(event->scenePos())) {
            menu->addSeparator();
            menu->addAction(tr("clear cell"), this, [this] { this->clear(); });
            if (m_cells->size() == 1)
                menu->addAction(tr("delete line"), this, [this] { Q_EMIT this->deleteLine(this); });
            else
                menu->addAction(tr("delete cell"), this, [this] { Q_EMIT this->deleteCell(this); });
            menu->addAction(tr("add prev cell"), this, [this] { Q_EMIT this->addPrevCell(this); });
            menu->addAction(tr("add next cell"), this, [this] { Q_EMIT this->addNextCell(this); });
            if (m_cells->indexOf(this) != 0)
                menu->addAction(tr("linebreak"), this, [this] { Q_EMIT this->linebreak(this); });
        }

        menu->exec(event->screenPos());
        event->accept();
        delete menu;
        return QGraphicsItem::contextMenuEvent(event);
    }

    void LyricCell::updateLyricRect() {
        const auto lyric = m_note->lyric.isEmpty() ? "0" : m_note->lyric;
        const auto syllable = m_note->syllable.isEmpty() ? "0" : m_note->syllable;

        m_lRect = QFontMetrics(m_font).boundingRect(lyric);
        m_sRect = QFontMetrics(m_syllableFont).boundingRect(syllable);
    }

    qreal LyricCell::lyricWidth() const { return m_lRect.width() + 10; }

    qreal LyricCell::syllableWidth() const { return m_sRect.width() + 10; }

    QPointF LyricCell::lyricPos() const {
        const auto rPos = rectPos();
        return {width() / 2 - lyricWidth() / 2, rPos.y() + m_rectPadding};
    }

    QPointF LyricCell::rectPos() const { return {m_padding, m_padding + m_sRect.height() + m_lsPadding}; }

    QPointF LyricCell::syllablePos() const { return {width() / 2 - syllableWidth() / 2, m_padding}; }

    QRectF LyricCell::lyricRect() const {
        return {x() + rectPos().x(), y() + rectPos().y(), width() - m_padding * 2,
                m_lRect.height() + m_rectPadding * 2};
    }

    QRectF LyricCell::syllableRect() const {
        return {x() + syllablePos().x(), y() + syllablePos().y(), static_cast<qreal>(m_sRect.width()),
                static_cast<qreal>(m_sRect.height())};
    }

    void LyricCell::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
        int flag = 0;
        if (option->state & QStyle::State_MouseOver)
            flag = Hovered;
        if (option->state & QStyle::State_Selected)
            flag = Selected;

        int lyricFlag = 0;
        const auto syllable = m_note->revised ? m_note->syllableRevised : m_note->syllable;
        if (m_note->revised) {
            lyricFlag = Revised;
        } else if (m_note->candidates.size() > 1) {
            lyricFlag = MultiTone;
        }

        const auto &syllableFont = (lyricFlag != 0) ? m_syllableFontBold : m_syllableFont;

        const auto sPos = syllablePos();
        painter->setFont(syllableFont);
        painter->setPen(m_syllablePen[lyricFlag]);
        painter->drawText(QRectF(sPos.x() + 5, sPos.y(), syllableWidth(), m_sRect.height() + 0.5), syllable);

        const auto rPos = rectPos();
        const auto boxRect = QRectF(rPos.x(), rPos.y(), width() - m_padding * 2, m_lRect.height() + m_rectPadding * 2);

        painter->setBrush(m_backgroundBrush[flag]);
        painter->setPen(m_borderPen[flag]);
        painter->drawRoundedRect(boxRect, m_padding * 0.5, m_padding * 0.5);

        painter->setFont(m_font);
        painter->setPen(m_lyricPen[lyricFlag]);

        const auto lPos = lyricPos();
        painter->drawText(QRectF(lPos.x() + 5, lPos.y(), lyricWidth(), m_lRect.height() + 0.5), m_note->lyric);
    }

    void LyricCell::changePhonicMenu(QMenu *menu) {
        auto *inputAction = new QAction(tr("Custom Syllables"), this);
        menu->addAction(inputAction);
        connect(inputAction, &QAction::triggered, this,
                [this]
                {
                    bool ok;
                    const QString syllable = QInputDialog::getText(
                        m_view, tr("Custom Syllables"), tr("Please input syllables"), QLineEdit::Normal, "", &ok);
                    if (ok && !syllable.isEmpty()) {
                        this->setSyllable(syllable);
                    }
                });
    }

    void LyricCell::changeSyllableMenu(QMenu *menu) {
        QStringList candidateSyllables = m_note->candidates;

        for (const auto &syllable : candidateSyllables) {
            if (candidateSyllables.size() > 1) {
                menu->addAction(syllable, [this, syllable] { this->setSyllable(syllable); });
            }
        }
    }

    void LyricCell::setQss(const CellQss *qss) {
        m_backgroundBrush = qss->cellBackgroundBrush;
        m_borderPen = qss->cellBorderPen;
        m_lyricPen = qss->cellLyricPen;
        m_syllablePen = qss->cellSyllablePen;
    }
} // namespace FillLyric
