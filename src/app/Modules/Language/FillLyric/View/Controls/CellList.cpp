#include "CellList.h"

#include <QGraphicsOpacityEffect>
#include <QStyleOptionGraphicsItem>

#include "../../Commands/Cell/EditCellCmd.h"
#include "../../Commands/Cell/ChangeSyllableCmd.h"

#include "../../Commands/Cell/ClearCellCmd.h"
#include "../../Commands/Cell/DeleteCellCmd.h"
#include "../../Commands/Cell/AddPrevCellCmd.h"
#include "../../Commands/Cell/AddNextCellCmd.h"

namespace FillLyric {
    CellList::CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList,
                       QGraphicsScene *scene, QGraphicsView *view, QUndoStack *undoStack)
        : m_view(view), m_scene(scene), m_history(undoStack) {
        this->setPos(x, y);
        m_scene->addItem(this);
        setFlag(ItemIsSelectable);

        m_splitter = new SplitterItem(0, 0, m_curWidth, this);

        m_handle = new HandleItem(this);
        m_handle->setPos(0, m_splitter->margin());

        for (const auto &note : noteList) {
            const auto lyricCell = new LyricCell(deltaX(), this->y() + deltaY(), note, m_view);
            m_cells.append(lyricCell);
            m_scene->addItem(lyricCell);
            this->connectCell(lyricCell);
        }
        this->updateCellPos();
        m_handle->setHeight(m_height);
        connect(m_handle, &HandleItem::selectAll, this, &CellList::selectList);
    }

    QRectF CellList::boundingRect() const {
        return {0, deltaY(), m_curWidth, m_height};
    }

    void CellList::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                         QWidget *widget) {
        painter->setPen(Qt::NoPen);
        if (option->state & QStyle::State_Selected) {
            painter->setBrush(QColor(255, 255, 255, 10));
        } else {
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawRect(boundingRect());
    }

    void CellList::clear() {
        for (auto &m_cell : m_cells) {
            delete m_cell;
            m_cell = nullptr;
        }
        delete m_handle;
        m_handle = nullptr;
        delete m_splitter;
        m_splitter = nullptr;
    }

    void CellList::setAutoWrap(const bool &autoWrap) {
        m_autoWarp = autoWrap;
    }

    qreal CellList::deltaX() const {
        return m_handle->width() + 3;
    }

    qreal CellList::deltaY() const {
        return m_splitter->deltaY();
    }

    void CellList::setBaseY(const qreal &y) {
        this->setPos(x(), y);
        this->updateCellPos();
    }

    qreal CellList::height() const {
        return m_height;
    }

    qreal CellList::cellWidth() const {
        if (m_autoWarp) {
            return m_curWidth;
        }
        qreal width = 0;
        for (const auto &cell : m_cells) {
            width += cell->width();
        }
        return width + deltaX();
    }

    QGraphicsView *CellList::view() const {
        return m_view;
    }

    LyricCell *CellList::createNewCell() const {
        const auto lyricCell = new LyricCell(0, this->y() + deltaY(), new LangNote(), m_view);
        this->connectCell(lyricCell);
        return lyricCell;
    }

    void CellList::appendCell(LyricCell *cell) {
        m_cells.append(cell);
        m_scene->addItem(cell);
        this->updateCellPos();
    }

    void CellList::removeCell(LyricCell *cell) {
        const auto index = m_cells.indexOf(cell);
        if (0 <= index && index < m_cells.size()) {
            m_cells.remove(index);
            m_scene->removeItem(cell);
            this->updateCellPos();
        }
    }

    void CellList::insertCell(const int &index, LyricCell *cell) {
        if (0 <= index && index < m_cells.size()) {
            m_cells.insert(index, cell);
            m_scene->addItem(cell);
            this->updateCellPos();
        }
    }

    void CellList::addToScene() {
        for (const auto &cell : m_cells) {
            m_scene->addItem(cell);
        }
        if (!m_scene->items().contains(m_splitter)) {
            m_splitter->setParentItem(this);
        }
        if (!m_scene->items().contains(m_handle)) {
            m_handle->setParentItem(this);
        }
        if (!m_scene->items().contains(this))
            m_scene->addItem(this);
    }

    void CellList::removeFromScene() {
        for (const auto &cell : m_cells) {
            m_scene->removeItem(cell);
        }
        m_scene->removeItem(m_splitter);
        m_scene->removeItem(m_handle);
        m_scene->removeItem(this);
    }

    void CellList::setWidth(const qreal &width) {
        m_curWidth = width;
        m_splitter->setWidth(width);
        this->updateCellPos();
    }

    void CellList::updateSplitter(const qreal &width) {
        m_curWidth = width;
        m_splitter->setWidth(width);
    }

    void CellList::setFont(const QFont &font) {
        m_font = font;
        const auto lMetric = QFontMetrics(m_font);
        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        const auto sMetric = QFontMetrics(syllableFont);

        for (const auto &cell : m_cells) {
            const auto lRect = lMetric.boundingRect(cell->lyric());
            const auto sRect = sMetric.boundingRect(cell->syllable());

            cell->setLyricRect(lRect);
            cell->setSyllableRect(sRect);
            cell->setFont(font);
        }
        this->updateCellPos();
    }

    void CellList::updateRect(LyricCell *cell) {
        const auto lMetric = QFontMetrics(m_font);
        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        const auto sMetric = QFontMetrics(syllableFont);

        const auto lyric = cell->lyric().isEmpty() ? " " : cell->lyric();
        const auto lRect = lMetric.boundingRect(lyric);
        const auto syllable = cell->syllable().isEmpty() ? " " : cell->syllable();
        const auto sRect = sMetric.boundingRect(syllable);

        cell->setLyricRect(lRect);
        cell->setSyllableRect(sRect);
        cell->setFont(m_font);
        cell->update();

        this->updateCellPos();
    }

    void CellList::updateCellPos() {
        qreal x = deltaX();
        qreal y = this->y() + m_splitter->deltaY();

        for (const auto cell : m_cells) {
            const auto cellWidth = cell->width();
            if (x + cellWidth > m_curWidth && m_autoWarp) {
                // Move to the next row
                x = deltaX();
                y += cell->height();
            }
            cell->setPos(x, y);
            x += cellWidth;
        }

        auto height = y - this->y() + m_splitter->margin();
        if (!m_cells.isEmpty())
            height += m_cells[0]->height();

        if (m_height != height) {
            m_height = height;
            m_handle->setHeight(m_height);
            Q_EMIT this->heightChanged();
        }
        Q_EMIT this->cellPosChanged();
    }

    void CellList::connectCell(const LyricCell *cell) const {
        connect(cell, &LyricCell::updateWidth, this, &CellList::updateCellPos);
        connect(cell, &LyricCell::updateLyric, this, &CellList::editCell);

        connect(cell, &LyricCell::changeSyllable, this, &CellList::changeSyllable);

        // cell option
        connect(cell, &LyricCell::clearCell, this, &CellList::clearCell);
        connect(cell, &LyricCell::deleteCell, this, &CellList::deleteCell);
        connect(cell, &LyricCell::addPrevCell, this, &CellList::addPrevCell);
        connect(cell, &LyricCell::addNextCell, this, &CellList::addNextCell);
        connect(cell, &LyricCell::linebreak, this, &CellList::linebreak);
    }

    void CellList::disconnectCell(const LyricCell *cell) const {
        disconnect(cell, &LyricCell::updateWidth, this, &CellList::updateCellPos);
        disconnect(cell, &LyricCell::updateLyric, this, &CellList::editCell);

        disconnect(cell, &LyricCell::changeSyllable, this, &CellList::changeSyllable);

        disconnect(cell, &LyricCell::clearCell, this, &CellList::clearCell);
        disconnect(cell, &LyricCell::deleteCell, this, &CellList::deleteCell);
        disconnect(cell, &LyricCell::addPrevCell, this, &CellList::addPrevCell);
        disconnect(cell, &LyricCell::addNextCell, this, &CellList::addNextCell);
        disconnect(cell, &LyricCell::linebreak, this, &CellList::linebreak);
    }

    void CellList::selectList() const {
        for (const auto &cell : m_cells) {
            cell->setSelected(true);
        }
    }

    void CellList::editCell(LyricCell *cell, const QString &lyric) {
        m_history->push(new EditCellCmdfinal(this, cell, lyric));
    }

    void CellList::changeSyllable(LyricCell *cell, const QString &syllable) {
        m_history->push(new ChangeSyllableCmd(this, cell, syllable));
    }

    void CellList::clearCell(LyricCell *cell) {
        m_history->push(new ClearCellCmd(this, cell));
    }

    void CellList::deleteCell(LyricCell *cell) {
        m_history->push(new DeleteCellCmd(this, cell));
    }

    void CellList::addPrevCell(LyricCell *cell) {
        m_history->push(new AddPrevCellCmd(this, cell));
    }

    void CellList::addNextCell(LyricCell *cell) {
        m_history->push(new AddNextCellCmd(this, cell));
    }

    void CellList::linebreak(LyricCell *cell) const {
        Q_EMIT this->linebreakSignal(static_cast<int>(m_cells.indexOf(cell)));
    }

}