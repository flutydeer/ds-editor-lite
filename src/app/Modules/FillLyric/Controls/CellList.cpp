#include "Modules/FillLyric/Controls/CellList.h"
#include "Modules/FillLyric/Controls/LyricCell.h"

#include <QMenu>
#include <QStyleOptionGraphicsItem>
#include <qgraphicssceneevent.h>

#include "Modules/FillLyric/Controls/LyricWrapView.h"
#include "Modules/FillLyric/Utils/G2pService.h"
#include "Modules/FillLyric/Utils/QssParser.h"

namespace FillLyric
{
    class LyricWrapView;

    CellList::CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList, QGraphicsScene *scene,
                       QGraphicsView *view, QList<CellList *> *cellLists,
                       const QStringList &priorityG2pIds,
                       const QMap<std::string, std::string> &langToG2pId) :
        m_view(view), m_scene(scene), m_cellQss(new CellQss()), m_cellLists(cellLists),
        m_priorityG2pIds(priorityG2pIds), m_langToG2pId(langToG2pId) {
        this->setPos(x, y);
        m_scene->addItem(this);
        setFlag(ItemIsSelectable);

        this->setCellQss();

        m_splitter = new SplitterItem(0, 0, m_curWidth, m_view, this);
        if (y <= 0)
            m_splitter->hide();
        else
            m_splitter->show();

        m_handle = new HandleItem(m_view, this);
        m_handle->setPos(0, m_splitter->margin());

        for (const auto &note : noteList) {
            const auto lyricCell = new LyricCell(deltaX(), this->y() + deltaY(), note, m_view, m_cellQss, &m_cells);
            m_cells.append(lyricCell);
            m_scene->addItem(lyricCell);
            this->connectCell(lyricCell);
        }
        this->updateCellPos();
        m_handle->setHeight(m_height);

        connect(m_handle, &HandleItem::contextMenuRequested, this, &CellList::showContextMenu);
        connect(m_handle, &HandleItem::selectAll, this, &CellList::selectList);
    }

    QRectF CellList::boundingRect() const { return {0, deltaY(), m_curWidth, m_height}; }

    void CellList::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) {
        painter->setPen(Qt::NoPen);
        if (option->state & QStyle::State_Selected) {
            painter->setBrush(QColor(255, 255, 255, 10));
        } else {
            painter->setBrush(Qt::NoBrush);
        }
        painter->drawRect(boundingRect());
    }

    void CellList::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
        if (!m_scene->selectedItems().empty())
            return;
        for (const auto &cell : m_cells) {
            if (cell->y() <= event->scenePos().y() && cell->y() + cell->height() >= event->scenePos().y() &&
                cell->x() + cell->width() >= event->scenePos().x()) {
                return;
            }
        }
        this->showContextMenu(event->screenPos());
    }

    void CellList::clear() {
        for (auto &m_cell : m_cells) {
            delete m_cell;
            m_cell = nullptr;
        }
        m_cells.clear();
        delete m_handle;
        m_handle = nullptr;
        delete m_splitter;
        m_splitter = nullptr;
        delete m_cellQss;
        m_cellQss = nullptr;
    }

    CellList::~CellList() {
        delete m_cellQss;
        m_cellQss = nullptr;
        qDeleteAll(m_cells);
        m_cells.clear();
    }

    qreal CellList::deltaX() const { return m_handle->width() + 3; }

    qreal CellList::deltaY() const { return m_splitter->deltaY(); }

    void CellList::setBaseY(const qreal &y) {
        if (y <= 0)
            m_splitter->hide();
        else
            m_splitter->show();

        m_handle->setPos(0, m_splitter->margin());
        this->setPos(x(), y);
    }

    qreal CellList::height() const { return m_height; }

    qreal CellList::cellWidth() const { return m_curWidth; }

    QGraphicsView *CellList::view() const { return m_view; }

    void CellList::highlight() { this->setSelected(true); }

    void CellList::selectCells(const QPointF &startPos, const QPointF &endPos) {
        qreal x = this->x() + deltaX();
        qreal y = this->y() + m_splitter->deltaY();

        for (const auto cell : m_cells) {
            const auto cellWidth = cell->width();
            if (x + cellWidth > m_curWidth) {
                x = deltaX();
                y += cell->height();
            }

            if (startPos.y() >= y && startPos.y() < y + cell->height()) {
                if (startPos.x() < x + cellWidth) {
                    if (endPos.y() > y && endPos.y() <= y + cell->height()) {
                        if (x < endPos.x())
                            cell->setSelected(true);
                        else
                            cell->setSelected(false);
                    } else
                        cell->setSelected(true);
                } else
                    cell->setSelected(false);
            } else if (startPos.y() < y && endPos.y() > y) {
                if (endPos.y() > y + cell->height())
                    cell->setSelected(true);
                else if (endPos.y() > y && endPos.y() <= y + cell->height()) {
                    if (x < endPos.x())
                        cell->setSelected(true);
                    else
                        cell->setSelected(false);
                } else
                    cell->setSelected(false);
            } else {
                cell->setSelected(false);
            }
            x += cellWidth;
        }
    }

    LyricCell *CellList::createNewCell() {
        const auto lyricCell = new LyricCell(0, this->y() + deltaY(), new LangNote(), m_view, m_cellQss, &m_cells);
        this->updateRect(lyricCell);
        this->connectCell(lyricCell);
        return lyricCell;
    }

    void CellList::appendCell(LyricCell *cell) {
        this->connectCell(cell);
        m_cells.append(cell);
        m_scene->addItem(cell);
        this->updateCellPos();
    }

    void CellList::removeCell(LyricCell *cell) {
        const auto index = m_cells.indexOf(cell);
        if (0 <= index && index < m_cells.size()) {
            this->disconnectCell(cell);
            m_cells.removeAt(index);
            m_scene->removeItem(cell);
            this->updateCellPos();
        }
    }

    void CellList::insertCell(const int &index, LyricCell *cell) {
        if (0 <= index && index < m_cells.size()) {
            m_cells.insert(index, cell);
            m_scene->addItem(cell);
            this->updateCellPos();
        } else if (index == m_cells.size()) {
            m_cells.append(cell);
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

    QFont CellList::syllableFont() const {
        QFont font(m_font);
        if (font.pointSize() >= 3)
            font.setPointSize(font.pointSize() - 3);
        return font;
    }

    void CellList::setFont(const QFont &font) {
        m_font = font;
        this->recalcCellRects();
        this->updateCellPos();
    }

    void CellList::updateFontOnly(const QFont &font) {
        m_font = font;
        this->recalcCellRects();
    }

    void CellList::recalcCellRects() {
        const auto lMetric = QFontMetrics(m_font);
        const auto sMetric = QFontMetrics(syllableFont());

        const auto minLRect = lMetric.boundingRect("0");
        const auto minSRect = sMetric.boundingRect("0");

        for (const auto &cell : m_cells) {
            const auto lRect = lMetric.boundingRect(cell->lyric());
            const auto sRect = sMetric.boundingRect(cell->syllable());

            cell->setLyricRect(lRect.height() > 0 ? lRect : minLRect);
            cell->setSyllableRect(sRect.height() > 0 ? sRect : minSRect);
            cell->setFont(m_font);
        }
    }

    void CellList::updateRect(LyricCell *cell) {
        const auto lMetric = QFontMetrics(m_font);
        const auto sMetric = QFontMetrics(syllableFont());

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
            if (x + cellWidth > m_curWidth) {
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

    void CellList::connectCell(const LyricCell *cell) {
        connect(cell, &LyricCell::updateWidth, this, &CellList::updateCellPos);
        connect(cell, &LyricCell::updateLyric, this, &CellList::editCell);

        connect(cell, &LyricCell::changeSyllable, this, &CellList::changeSyllable);

        connect(cell, &LyricCell::deleteLine, this, [this] { Q_EMIT requestDeleteLine(this); });
        connect(cell, &LyricCell::deleteCell, this, &CellList::deleteCell);
        connect(cell, &LyricCell::addPrevCell, this, &CellList::addPrevCell);
        connect(cell, &LyricCell::addNextCell, this, &CellList::addNextCell);
        connect(cell, &LyricCell::linebreak, this, &CellList::linebreak);
    }

    void CellList::disconnectCell(const LyricCell *cell) const {
        cell->disconnect(this);
    }

    void CellList::selectList() const {
        for (const auto &cell : m_cells) {
            cell->setSelected(true);
        }
    }

    void CellList::editCell(LyricCell *cell, const QString &lyric) {
        const auto cellIndex = m_cells.indexOf(cell);

        QList<LangNote> tempNotes;
        tempNotes.reserve(m_cells.size());
        for (const auto &lyricCell : m_cells) {
            tempNotes.append(*lyricCell->note());
        }
        tempNotes[cellIndex].lyric = lyric;

        std::vector<std::string> priorityG2pIds;
        priorityG2pIds.reserve(m_priorityG2pIds.size());
        for (const auto &id : m_priorityG2pIds)
            priorityG2pIds.push_back(id.toStdString());

        const auto g2pResults = G2pService::convert(tempNotes, priorityG2pIds, m_langToG2pId);
        for (int i = 0; i < m_cells.size(); i++) {
            auto *note = m_cells[i]->note();
            const auto &res = g2pResults[i];
            const bool changed = (note->language != res.language) || (note->g2pId != res.g2pId) ||
                                 (note->syllable != res.syllable) || (note->lyric != tempNotes[i].lyric);
            if (changed) {
                note->lyric = tempNotes[i].lyric;
                note->language = res.language;
                note->g2pId = res.g2pId;
                note->syllable = res.syllable;
                note->candidates = res.candidates;
            }
        }

        this->updateRect(cell);
    }

    void CellList::changeSyllable(LyricCell *cell, const QString &syllable) {
        cell->note()->syllableRevised = syllable;
        cell->note()->revised = true;
        this->updateRect(cell);
    }

    void CellList::clearCell(LyricCell *cell) { cell->clear(); }

    void CellList::deleteCell(LyricCell *cell) {
        this->removeCell(cell);
        delete cell;
    }

    void CellList::addPrevCell(LyricCell *cell) {
        this->insertCell(static_cast<int>(m_cells.indexOf(cell)), createNewCell());
    }

    void CellList::addNextCell(LyricCell *cell) {
        this->insertCell(static_cast<int>(m_cells.indexOf(cell)) + 1, createNewCell());
    }

    void CellList::linebreak(LyricCell *cell) const {
        Q_EMIT this->linebreakSignal(static_cast<int>(m_cells.indexOf(cell)));
    }

    void CellList::showContextMenu(const QPointF &pos) {
        QMenu menu(m_view);
        menu.setAttribute(Qt::WA_TranslucentBackground);
        menu.setWindowFlags(menu.windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);

        this->highlight();
        menu.addAction(tr("append cell"), this, [this] { this->appendCell(this->createNewCell()); });
        menu.addSeparator();
        menu.addAction(tr("delete line"), this, [this] { Q_EMIT requestDeleteLine(this); });
        menu.addAction(tr("add prev line"), this, [this] { Q_EMIT requestAddPrevLine(this); });
        menu.addAction(tr("add next line"), this, [this] { Q_EMIT requestAddNextLine(this); });
        menu.addSeparator();
        if (m_cellLists->indexOf(this) != 0)
            menu.addAction(tr("move up"), this, [this] { Q_EMIT requestMoveUpLine(this); });
        if (m_cellLists->indexOf(this) != m_cellLists->count() - 1)
            menu.addAction(tr("move down"), this, [this] { Q_EMIT requestMoveDownLine(this); });
        menu.exec(pos.toPoint());
    }

    void CellList::setCellQss() const {
        const auto cellBackBrush = QssParser::propertyValue(m_view, "cellBackgroundBrush");
        const auto brushes = QssParser::parseBrushes(cellBackBrush, 3);
        if (brushes.size() == 3) {
            for (int i = 0; i < 3; i++)
                m_cellQss->cellBackgroundBrush[i] = brushes[i];
        }

        const auto lyricPen = QssParser::parsePens(QssParser::propertyValue(m_view, "cellLyricPen"), 4);
        if (lyricPen.size() == 4)
            m_cellQss->cellLyricPen = {lyricPen[0], lyricPen[1], lyricPen[2], lyricPen[3]};

        const auto syllablePen = QssParser::parsePens(QssParser::propertyValue(m_view, "cellSyllablePen"), 4);
        if (syllablePen.size() == 4)
            m_cellQss->cellSyllablePen = {syllablePen[0], syllablePen[1], syllablePen[2], syllablePen[3]};

        const auto borderPen = QssParser::parsePens(QssParser::propertyValue(m_view, "cellBorderPen"), 3);
        if (borderPen.size() == 3)
            m_cellQss->cellBorderPen = {borderPen[0], borderPen[1], borderPen[2]};
    }
} // namespace FillLyric
