#include "CellList.h"

#include <qdebug.h>

namespace LyricWrap {
    CellList::CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList,
                       QGraphicsScene *scene)
        : mX(x), mY(y), m_scene(scene) {
        m_splitter = new SplitterItem(mX, mY, m_curWidth, 1);
        m_scene->addItem(m_splitter);

        for (const auto &note : noteList) {
            const auto lyricCell = new LyricCell(0, mY + deltaY(), note);

            m_widths.append(lyricCell->width());
            m_cellHeight = lyricCell->height();
            m_cells.append(lyricCell);
            m_scene->addItem(lyricCell);

            connect(lyricCell, &LyricCell::updateWidthSignal, [this, lyricCell](const qreal &w) {
                m_widths[m_cells.indexOf(lyricCell)] = w;
                this->updateCellPos();
            });
        }
        this->updateCellPos();
    }

    qreal CellList::height() const {
        return m_height;
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

    qreal CellList::deltaY() const {
        return m_splitter->deltaY();
    }

    void CellList::updateSplitterPos() const {
        m_splitter->setPos(mX, mY);
    }

    void CellList::updateCellPos() {
        int rowCount = 0;
        qreal x = 0;
        qreal y = mY + deltaY();

        for (int i = 0; i < m_widths.size(); i++) {
            if (x + m_widths[i] > m_curWidth && autoWarp) {
                // Move to the next row
                rowCount++;
                x = 0;
                y += m_cellHeight;
            }
            m_cells[i]->setPos(x, y);
            x += m_widths[i];
        }

        m_height = y - mY + m_cellHeight;

        if (m_rowCount != rowCount) {
            m_rowCount = rowCount;
            Q_EMIT this->rowCountChanged();
        }
    }
}