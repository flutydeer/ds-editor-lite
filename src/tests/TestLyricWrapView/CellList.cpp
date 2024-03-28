#include "CellList.h"

#include <qdebug.h>

namespace LyricWrap {
    CellList::CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList,
                       QGraphicsScene *scene, const QFont &font)
        : mX(x), mY(y), m_scene(scene), m_font(font) {
        m_splitter = new SplitterItem(mX, mY, m_curWidth, 1);
        m_scene->addItem(m_splitter);

        const auto lyricHeight = QFontMetrics(m_font).height();
        const auto lyricXHeight = QFontMetrics(m_font).xHeight();

        QFont syllableFont(m_font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        const auto syllableHeight = QFontMetrics(syllableFont).height();
        const auto syllableXHeight = QFontMetrics(syllableFont).xHeight();

        for (const auto &note : noteList) {
            const auto lyricCell = new LyricCell(0, mY + deltaY(), note);

            lyricCell->setFontSize(font, lyricXHeight, lyricHeight, syllableXHeight,
                                   syllableHeight);
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

    void CellList::setFont(const QFont &font) {
        m_font = font;
        const auto lyricHeight = QFontMetrics(font).height();
        const auto lyricXHeight = QFontMetrics(font).xHeight();

        QFont syllableFont(font);
        syllableFont.setPointSize(syllableFont.pointSize() - 3);
        const auto syllableHeight = QFontMetrics(syllableFont).height();
        const auto syllableXHeight = QFontMetrics(syllableFont).xHeight();

        for (const auto &cell : m_cells) {
            cell->setFontSize(font, lyricXHeight, lyricHeight, syllableXHeight, syllableHeight);
        }
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