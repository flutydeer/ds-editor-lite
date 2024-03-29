#include "CellList.h"

#include <qdebug.h>

#include "../../Commands/Cell/ClearCellCmd.h"
#include "../../Commands/Cell/DeleteCellCmd.h"
#include "../../Commands/Cell/AddPrevCellCmd.h"
#include "../../Commands/Cell/AddNextCellCmd.h"

namespace FillLyric {
    CellList::CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList,
                       QGraphicsScene *scene, const QFont &font, QGraphicsView *view,
                       QUndoStack *undoStack)
        : mX(x), mY(y), m_font(font), m_view(view), m_scene(scene), m_history(undoStack) {
        m_splitter = new SplitterItem(mX, mY, m_curWidth, 1);
        m_scene->addItem(m_splitter);

        for (const auto &note : noteList) {
            const auto lyricCell = new LyricCell(0, mY + deltaY(), note, m_view);
            m_cells.append(lyricCell);
            m_scene->addItem(lyricCell);

            connect(lyricCell, &LyricCell::updateWidth, this, &CellList::updateCellPos);

            connect(lyricCell, &LyricCell::updateLyric,
                    [this, lyricCell] { this->updateRect(lyricCell); });

            // cell option
            connect(lyricCell, &LyricCell::clearCell,
                    [this, lyricCell] { m_history->push(new ClearCellCmd(this, lyricCell)); });
            connect(lyricCell, &LyricCell::deleteCell,
                    [this, lyricCell] { m_history->push(new DeleteCellCmd(this, lyricCell)); });
            connect(lyricCell, &LyricCell::addPrevCell,
                    [this, lyricCell] { m_history->push(new AddPrevCellCmd(this, lyricCell)); });
            connect(lyricCell, &LyricCell::addNextCell,
                    [this, lyricCell] { m_history->push(new AddNextCellCmd(this, lyricCell)); });

            // line option
            connect(lyricCell, &LyricCell::deleteLine, this, &CellList::deleteLine);
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

    qreal CellList::y() const {
        return mY;
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
        this->updateRect(lyricCell);
        connect(lyricCell, &LyricCell::updateWidth, this, &CellList::updateCellPos);
        connect(lyricCell, &LyricCell::updateLyric,
                [this, lyricCell] { this->updateRect(lyricCell); });

        // cell option
        connect(lyricCell, &LyricCell::clearCell,
                [this, lyricCell] { m_history->push(new ClearCellCmd(this, lyricCell)); });
        connect(lyricCell, &LyricCell::deleteCell,
                [this, lyricCell] { m_history->push(new DeleteCellCmd(this, lyricCell)); });
        connect(lyricCell, &LyricCell::addPrevCell,
                [this, lyricCell] { m_history->push(new AddPrevCellCmd(this, lyricCell)); });
        connect(lyricCell, &LyricCell::addNextCell,
                [this, lyricCell] { m_history->push(new AddNextCellCmd(this, lyricCell)); });

        // line option
        connect(lyricCell, &LyricCell::deleteLine, this, &CellList::deleteLine);
        return lyricCell;
    }


    void CellList::setWidth(const qreal &width) {
        m_curWidth = width;
        m_splitter->setWidth(width);
        this->updateCellPos();
        this->updateSplitterPos();
    }

    void CellList::setBaseY(const qreal &y) {
        mY = y;
        m_splitter->setPos(mX, mY);
        this->updateCellPos();
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

        this->updateCellPos();
    }

    qreal CellList::deltaY() const {
        return m_splitter->deltaY();
    }

    void CellList::updateSplitterPos() const {
        m_splitter->setPos(mX, mY);
    }

    void CellList::updateCellPos() {
        qreal x = 0;
        qreal y = mY + m_splitter->deltaY();

        for (const auto cell : m_cells) {
            const auto cellWidth = cell->width();
            if (x + cellWidth > m_curWidth && autoWarp) {
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
}