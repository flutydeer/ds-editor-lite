#include "CellList.h"

#include <qdebug.h>

#include "../../Commands/Cell/EditCellCmd.h"
#include "../../Commands/Cell/ChangeSyllableCmd.h"

#include "../../Commands/Cell/ClearCellCmd.h"
#include "../../Commands/Cell/DeleteCellCmd.h"
#include "../../Commands/Cell/AddPrevCellCmd.h"
#include "../../Commands/Cell/AddNextCellCmd.h"

namespace FillLyric {
    CellList::CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList,
                       QGraphicsScene *scene, QGraphicsView *view, QUndoStack *undoStack)
        : mX(x), mY(y), m_view(view), m_scene(scene), m_history(undoStack) {
        m_splitter = new SplitterItem(mX, mY, m_curWidth, 1);
        m_scene->addItem(m_splitter);

        for (const auto &note : noteList) {
            const auto lyricCell = new LyricCell(0, mY + deltaY(), note, m_view);
            m_cells.append(lyricCell);
            m_scene->addItem(lyricCell);
            this->connectCell(lyricCell);
        }
        this->updateCellPos();
    }

    void CellList::clear() {
        for (auto &m_cell : m_cells) {
            delete m_cell;
            m_cell = nullptr;
        }
        delete m_splitter;
        m_splitter = nullptr;
    }

    void CellList::setAutoWrap(const bool &autoWrap) {
        m_autoWarp = autoWrap;
    }

    qreal CellList::y() const {
        return mY;
    }

    qreal CellList::deltaY() const {
        return m_splitter->deltaY();
    }

    void CellList::setBaseY(const qreal &y) {
        mY = y;
        m_splitter->setPos(mX, mY);
        this->updateCellPos();
    }

    qreal CellList::height() const {
        return m_height;
    }

    QGraphicsView *CellList::view() const {
        return m_view;
    }

    QGraphicsScene *CellList::sence() const {
        return m_scene;
    }

    LyricCell *CellList::createNewCell() {
        const auto lyricCell = new LyricCell(0, mY + this->deltaY(), new LangNote(), m_view);
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
        if (0 <= index < m_cells.size()) {
            m_cells.remove(index);
            m_scene->removeItem(cell);
            this->updateCellPos();
        }
    }

    void CellList::setWidth(const qreal &width) {
        m_curWidth = width;
        m_splitter->setWidth(width);
        this->updateCellPos();
        this->updateSplitterPos();
    }

    void CellList::updateSpliter(const qreal &width) const {
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
        qreal x = 0;
        qreal y = mY + m_splitter->deltaY();

        for (const auto cell : m_cells) {
            const auto cellWidth = cell->width();
            if (x + cellWidth > m_curWidth && m_autoWarp) {
                // Move to the next row
                x = 0;
                y += cell->height();
            }
            cell->setPos(x, y + m_cellMargin);
            x += cellWidth;
        }

        auto height = y - mY + m_splitter->margin() + m_cellMargin;
        if (!m_cells.isEmpty())
            height += m_cells[0]->height();

        if (m_height != height) {
            m_height = height;
            Q_EMIT this->heightChanged();
        }
        Q_EMIT this->cellPosChanged();
    }

    void CellList::updateSplitterPos() const {
        m_splitter->setPos(mX, mY);
    }

    void CellList::connectCell(LyricCell *cell) {
        connect(cell, &LyricCell::updateWidth, this, &CellList::updateCellPos);
        connect(cell, &LyricCell::updateLyric, [this, cell](const QString &lyric) {
            m_history->push(new EditCellCmdfinal(this, cell, lyric));
        });

        connect(cell, &LyricCell::changeSyllable, [this, cell](const QString &syllable) {
            m_history->push(new ChangeSyllableCmd(this, cell, syllable));
        });

        // cell option
        connect(cell, &LyricCell::clearCell,
                [this, cell] { m_history->push(new ClearCellCmd(this, cell)); });
        connect(cell, &LyricCell::deleteCell,
                [this, cell] { m_history->push(new DeleteCellCmd(this, cell)); });
        connect(cell, &LyricCell::addPrevCell,
                [this, cell] { m_history->push(new AddPrevCellCmd(this, cell)); });
        connect(cell, &LyricCell::addNextCell,
                [this, cell] { m_history->push(new AddNextCellCmd(this, cell)); });

        // line option
        connect(cell, &LyricCell::deleteLine, this, &CellList::deleteLine);
        connect(cell, &LyricCell::addPrevLine, this, &CellList::addPrevLine);
        connect(cell, &LyricCell::addNextLine, this, &CellList::addNextLine);
    }

}