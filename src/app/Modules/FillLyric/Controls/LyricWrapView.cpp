#include "Modules/FillLyric/Controls/CellList.h"
#include "Modules/FillLyric/Controls/LyricCell.h"
#include "Modules/FillLyric/Controls/LyricWrapView.h"

#include <QMenu>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTimer>
#include <utility>

#include "Modules/FillLyric/Utils/G2pService.h"

#include <QFile>

namespace FillLyric
{
    LyricWrapView::LyricWrapView(QString qssPath, QStringList priorityG2pIds,
                                 QMap<std::string, std::string> langToG2pId, QWidget *parent) :
        QGraphicsView(parent), m_qssPath(std::move(qssPath)), m_priorityG2pIds(std::move(priorityG2pIds)),
        m_langToG2pId(std::move(langToG2pId)) {
        setAttribute(Qt::WA_StyledBackground, true);
        auto qssFile = QFile(m_qssPath);
        if (qssFile.open(QIODevice::ReadOnly)) {
            this->setStyleSheet(qssFile.readAll());
            qssFile.close();
        }

        m_font = this->font();
        m_scene = new QGraphicsScene(this);
        m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);

        this->setScene(m_scene);
        this->setDragMode(NoDrag);

        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setRenderHint(QPainter::Antialiasing, true);
        this->installEventFilter(this);

        auto *noteCountTimer = new QTimer(this);
        noteCountTimer->setSingleShot(true);
        noteCountTimer->setInterval(0);
        connect(noteCountTimer, &QTimer::timeout, this,
                [this]
                {
                    int count = 0;
                    for (const auto &cellList : m_cellLists)
                        count += static_cast<int>(cellList->m_cells.size());
                    Q_EMIT noteCountChanged(count);
                });
        connect(m_scene, &QGraphicsScene::changed, noteCountTimer,
                static_cast<void (QTimer::*)()>(&QTimer::start));
    }

    LyricWrapView::~LyricWrapView() = default;

    void LyricWrapView::keyPressEvent(QKeyEvent *event) {
        if (event->key() == Qt::Key_Delete) {
            QList<LyricCell *> selectedCells;
            for (const auto &item : scene()->selectedItems()) {
                if (const auto cell = dynamic_cast<LyricCell *>(item)) {
                    selectedCells.append(cell);
                }
            }
            if (!selectedCells.isEmpty()) {
                if (selectedCells.size() == 1) {
                    const auto cellList = this->mapToList(selectedCells.first()->scenePos());
                    if (cellList && cellList->m_cells.size() == 1) {
                        this->removeList(cellList);
                        event->accept();
                        return;
                    }
                }
                if (selectedCells.size() > 1) {
                    const auto cellList = this->mapToList(selectedCells.first()->scenePos());
                    if (this->cellEqualLine(selectedCells)) {
                        this->removeList(cellList);
                        event->accept();
                        return;
                    }
                }
            }

            this->deleteCells(selectedCells);
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            const auto selectedItems = scene()->selectedItems();
            QSet<CellList *> listSet;
            for (const auto item : selectedItems) {
                if (const auto handle = dynamic_cast<HandleItem *>(item)) {
                    listSet.insert(dynamic_cast<CellList *>(handle->parentItem()));
                } else if (const auto cellList = dynamic_cast<CellList *>(item)) {
                    listSet.insert(cellList);
                }
            }
            if (!listSet.isEmpty()) {
                if (event->key() == Qt::Key_Up && !listSet.contains(m_cellLists.first())) {
                    this->moveUpLists(sortedByIndex(listSet));
                    this->repaintCellLists();
                } else if (event->key() == Qt::Key_Down && !listSet.contains(m_cellLists.last())) {
                    this->moveDownLists(sortedByIndex(listSet));
                    this->repaintCellLists();
                }
            }
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event);
    }

    void LyricWrapView::resizeEvent(QResizeEvent *event) {
        QGraphicsView::resizeEvent(event);
        repaintCellLists();
    }

    void LyricWrapView::wheelEvent(QWheelEvent *event) {
        if (event->modifiers() & Qt::ControlModifier) {
            const auto fontSizeDelta = event->angleDelta().y() / 120.0;
            constexpr double minimumStep = 0.25;
            if (std::abs(fontSizeDelta) >= minimumStep) {
                QFont font = this->font();
                const auto newSize = font.pointSizeF() + fontSizeDelta;
                if (newSize >= 9 && newSize != font.pointSizeF()) {
                    font.setPointSizeF(newSize);
                    this->setFont(font);
                    Q_EMIT this->fontSizeChanged();
                    this->updateCellRect();
                    event->accept();
                }
            }
        } else {
            QGraphicsView::wheelEvent(event);
        }
    }

    void LyricWrapView::mousePressEvent(QMouseEvent *event) {
        const auto scenePos = mapToScene(event->pos()).toPoint();

        if (event->button() == Qt::LeftButton)
            m_rubberBandOrigin = scenePos;

        QGraphicsView::mousePressEvent(event);
    }

    void LyricWrapView::mouseMoveEvent(QMouseEvent *event) {
        if (event->buttons() & Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier)) {
            const auto scenePos = mapToScene(event->pos()).toPoint();
            tryRubberBandSelect(scenePos);
            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void LyricWrapView::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier))
            m_lastClickPos = mapToScene(event->pos()).toPoint();

        if (event->button() == Qt::LeftButton) {
            const auto scenePos = mapToScene(event->pos()).toPoint();

            if (event->modifiers() & Qt::ShiftModifier) {
                for (const auto item : scene()->selectedItems()) {
                    if (!item->contains(m_lastClickPos))
                        item->setSelected(false);
                }

                this->selectCells(m_lastClickPos, scenePos);
                event->accept();
                return;
            }

            tryRubberBandSelect(scenePos);
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }

    QList<CellList *> LyricWrapView::sortedByIndex(const QSet<CellList *> &listSet) const {
        QMap<qlonglong, CellList *> map;
        for (const auto &list : QList(listSet.values()))
            map[m_cellLists.indexOf(list)] = list;
        return map.values();
    }

    void LyricWrapView::tryRubberBandSelect(const QPoint &scenePos) {
        if ((scenePos - m_rubberBandOrigin).manhattanLength() > 10) {
            if (const auto cellList = mapToList(m_rubberBandOrigin))
                cellList->setSelected(false);
            this->selectCells(m_rubberBandOrigin, scenePos);
        }
    }

    void LyricWrapView::contextMenuEvent(QContextMenuEvent *event) {
        QMenu menu(this);
        menu.setAttribute(Qt::WA_TranslucentBackground);
        menu.setWindowFlags(menu.windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);

        const auto clickPos = event->pos();
        const auto scenePos = mapToScene(clickPos).toPoint();
        const auto itemAtPos = scene()->itemAt(scenePos, QTransform());
        const auto selectedItems = scene()->selectedItems();
        QList<HandleItem *> handleItems;

        if (selectedItems.size() > 1) {
            m_selectedCells.clear();
            bool enableMenu = false;
            for (const auto &item : selectedItems) {
                if (const auto cell = dynamic_cast<LyricCell *>(item)) {
                    m_selectedCells.append(cell);
                    if (cell->lyricRect().contains(scenePos))
                        enableMenu = true;
                } else if (const auto handle = dynamic_cast<HandleItem *>(item)) {
                    handleItems.append(handle);
                }
            }

            if (enableMenu && !m_selectedCells.isEmpty()) {
                menu.addAction(tr("clear cells"),
                               [this]
                               {
                                   for (const auto cell : m_selectedCells)
                                       cell->clear();
                               });

                if (m_selectedCells.size() > 1) {
                    const auto cellList = this->mapToList(m_selectedCells.first()->scenePos());
                    if (this->cellEqualLine(m_selectedCells)) {
                        menu.addAction(tr("delete line"), [this, cellList] { this->removeList(cellList); });
                    } else
                        menu.addAction(tr("delete cells"), [this] { this->deleteCells(m_selectedCells); });
                }

                menu.exec(mapToGlobal(clickPos));
                event->accept();
                return;
            }
        }

        if (dynamic_cast<HandleItem *>(itemAtPos) && handleItems.size() > 1) {
            QSet<CellList *> selectedSet;
            if (const auto cellList = mapToList(scenePos)) {
                selectedSet.insert(cellList);
            }

            for (const auto &handle : handleItems) {
                const auto cellList = dynamic_cast<CellList *>(handle->parentItem());
                if (cellList != nullptr) {
                    selectedSet.insert(cellList);
                }
            }
            menu.addSeparator();
            menu.addAction(tr("delete lines"),
                           [this, selectedSet]
                           {
                               QMap<int, CellList *> m_listMap;
                               for (const auto &list : QList(selectedSet.values())) {
                                   m_listMap[static_cast<int>(m_cellLists.indexOf(list))] = list;
                               }
                               for (auto it = m_listMap.end(); it != m_listMap.begin();) {
                                   --it;
                                   this->removeList(it.value());
                               }
                               this->repaintCellLists();
                           });
            menu.addAction(tr("move up"), [this, selectedSet] { this->moveUpLists(QList(selectedSet.values())); });
            menu.addAction(tr("move down"), [this, selectedSet] { this->moveDownLists(QList(selectedSet.values())); });
            if (selectedSet.contains(m_cellLists.first()))
                menu.actions().at(2)->setEnabled(false);
            if (selectedSet.contains(m_cellLists.last()))
                menu.actions().at(3)->setEnabled(false);
            menu.exec(mapToGlobal(clickPos));
            event->accept();
            return;
        }
        return QGraphicsView::contextMenuEvent(event);
    }

    CellList *LyricWrapView::createNewList() {
        const auto width = this->width() - this->verticalScrollBar()->width();
        const auto cellList = new CellList(0, 0, {}, m_scene, this, &m_cellLists, m_priorityG2pIds, m_langToG2pId);
        cellList->setFont(this->font());
        cellList->setWidth(width);
        this->connectCellList(cellList);
        return cellList;
    }

    void LyricWrapView::insertList(const int &index, CellList *cellList) {
        if (index >= 0 && index < m_cellLists.size()) {
            m_cellLists.insert(index, cellList);
            cellList->addToScene();
            this->repaintCellLists();
        } else if (index == m_cellLists.size()) {
            m_cellLists.append(cellList);
            cellList->addToScene();
            this->repaintCellLists();
        }
    }

    void LyricWrapView::removeList(const int &index) {
        if (index < 0 || index >= m_cellLists.size())
            return;
        const auto cellList = m_cellLists[index];
        cellList->removeFromScene();
        m_cellLists.removeAt(index);
        cellList->clear();
        delete cellList;
        this->repaintCellLists();
        this->repaint();
    }

    void LyricWrapView::removeList(CellList *cellList) {
        const auto index = m_cellLists.indexOf(cellList);
        if (0 <= index && index < m_cellLists.size()) {
            this->removeList(static_cast<int>(index));
        }
    }

    void LyricWrapView::appendList(const QList<LangNote *> &noteList) {
        const auto width = this->width() - this->verticalScrollBar()->width();
        const auto cellList =
            new CellList(0, cellBaseY(static_cast<int>(m_cellLists.size())), noteList, m_scene, this, &m_cellLists,
                         m_priorityG2pIds, m_langToG2pId);
        cellList->setFont(this->font());
        cellList->setWidth(width);
        m_cellLists.append(cellList);
        this->connectCellList(cellList);
    }

    void LyricWrapView::moveUpLists(const QList<CellList *> &cellLists) {
        for (auto cellList : cellLists) {
            const qlonglong i = m_cellLists.indexOf(cellList);
            if (i >= 1)
                qSwap(m_cellLists[i], m_cellLists[i - 1]);
        }
    }

    void LyricWrapView::moveDownLists(const QList<CellList *> &cellLists) {
        QList<qlonglong> moveList;
        for (auto cellList : cellLists) {
            const qlonglong i = m_cellLists.indexOf(cellList);
            if (i < m_cellLists.size() - 1)
                moveList.append(i);
        }

        std::sort(moveList.begin(), moveList.end(), std::greater<>());

        for (const auto &i : moveList)
            qSwap(m_cellLists[i], m_cellLists[i + 1]);
    }

    QList<CellList *> LyricWrapView::cellLists() const { return m_cellLists; }
    QStringList LyricWrapView::priorityG2pIds() const { return m_priorityG2pIds; }

    void LyricWrapView::clear() {
        for (auto &m_cellList : m_cellLists) {
            m_cellList->clear();
            delete m_cellList;
            m_cellList = nullptr;
        }
        m_cellLists.clear();
        this->update();
    }

    void LyricWrapView::init(const QList<QList<LangNote>> &noteLists) {
        this->clear();

        std::vector<std::string> priorityG2pIds;
        for (const auto &id : m_priorityG2pIds)
            priorityG2pIds.push_back(id.toStdString());

        for (auto notes : noteLists) {
            const auto g2pResults = G2pService::convert(notes, priorityG2pIds, m_langToG2pId);
            QList<LangNote *> tempNotes;
            for (int i = 0; i < notes.size(); i++) {
                notes[i].language = g2pResults[i].language;
                notes[i].g2pId = g2pResults[i].g2pId;
                notes[i].syllable = g2pResults[i].syllable;
                notes[i].candidates = g2pResults[i].candidates;
                tempNotes.append(new LangNote(notes[i]));
            }
            if (!tempNotes.isEmpty())
                this->appendList(tempNotes);
        }
        this->repaintCellLists();
    }

    void LyricWrapView::updateCellRect() {
        this->setUpdatesEnabled(false);
        for (const auto &cellList : m_cellLists) {
            cellList->blockSignals(true);
            cellList->updateFontOnly(this->font());
            cellList->blockSignals(false);
        }
        this->setUpdatesEnabled(true);
        this->repaintCellLists();
    }

    void LyricWrapView::repaintCellLists() {
        const bool wasEnabled = updatesEnabled();
        if (wasEnabled)
            this->setUpdatesEnabled(false);

        qreal height = 0;
        const auto width = this->width() - this->verticalScrollBar()->width();
        const bool widthChanged = (width != this->sceneRect().width());

        for (const auto &cellList : m_cellLists) {
            cellList->setBaseY(height);
            if (widthChanged) {
                cellList->setWidth(width);
            } else {
                cellList->updateCellPos();
            }
            cellList->updateSplitter(width);
            height += cellList->height();
        }

        if (widthChanged || height != this->sceneRect().height())
            this->setSceneRect(QRectF(0, 0, width, height));

        if (wasEnabled)
            this->setUpdatesEnabled(true);
        this->update();
    }

    qreal LyricWrapView::maxListWidth() const {
        qreal maxWidth = 0;
        for (const auto &m_cellList : m_cellLists) {
            if (m_cellList->cellWidth() > maxWidth)
                maxWidth = m_cellList->cellWidth();
        }
        return maxWidth;
    }

    qreal LyricWrapView::height() {
        qreal height = 0;
        for (const auto &m_cellList : m_cellLists) {
            height += m_cellList->height();
        }
        return height;
    }

    void LyricWrapView::connectCellList(CellList *cellList) {
        connect(cellList, &CellList::heightChanged, this, &LyricWrapView::repaintCellLists);
        connect(cellList, &CellList::cellPosChanged, this, &LyricWrapView::updateRect);

        connect(cellList, &CellList::linebreakSignal,
                [this, cellList](const int cellIndex) { this->lineBreak(cellList, cellIndex); });

        connect(cellList, &CellList::requestDeleteLine, [this](CellList *list) { this->removeList(list); });
        connect(cellList, &CellList::requestAddPrevLine,
                [this](CellList *list) { this->insertNewLineAt(m_cellLists.indexOf(list)); });
        connect(cellList, &CellList::requestAddNextLine,
                [this](CellList *list) { this->insertNewLineAt(m_cellLists.indexOf(list) + 1); });
        connect(cellList, &CellList::requestMoveUpLine,
                [this](CellList *list)
                {
                    this->moveUpLists({list});
                    this->repaintCellLists();
                });
        connect(cellList, &CellList::requestMoveDownLine,
                [this](CellList *list)
                {
                    this->moveDownLists({list});
                    this->repaintCellLists();
                });
    }

    void LyricWrapView::insertNewLineAt(qlonglong index) {
        const auto newList = this->createNewList();
        newList->insertCell(0, newList->createNewCell());
        this->insertList(static_cast<int>(index), newList);
    }

    qreal LyricWrapView::cellBaseY(const int &index) const {
        qreal height = 0;
        for (int i = 0; i < std::min(index, static_cast<int>(m_cellLists.size())); i++) {
            height += m_cellLists[i]->height();
        }
        return height;
    }

    bool LyricWrapView::cellEqualLine(QList<LyricCell *> cells) {
        bool deleteLine = true;
        const auto cellList = this->mapToList(cells.first()->scenePos());

        if (cellList == nullptr)
            return false;

        if (cellList->m_cells.size() != cells.size())
            return false;

        for (const auto cell : cells) {
            if (!cellList->m_cells.contains(cell)) {
                deleteLine = false;
                break;
            }
        }
        return deleteLine;
    }
    void LyricWrapView::lineBreak(CellList *cellList, const int &index) {
        const auto cells = cellList->m_cells.mid(index);
        const auto newList = this->createNewList();

        this->insertList(m_cellLists.indexOf(cellList) + 1, newList);
        for (const auto &cell : cells) {
            cellList->removeCell(cell);
            newList->appendCell(cell);
        }
        this->repaintCellLists();
    }
    void LyricWrapView::deleteCells(const QList<LyricCell *> &selectedCells) {
        QMap<CellList *, QMap<int, LyricCell *>> cellsMap;
        QList<QPair<int, CellList *>> temp_cellLists;

        for (const auto &cell : selectedCells) {
            if (const auto cellList = this->mapToList(cell->lyricRect().center().toPoint())) {
                cellsMap[cellList][static_cast<int>(cellList->m_cells.indexOf(cell))] = cell;
            }
        }

        for (const auto &cellList : cellsMap.keys()) {
            if (cellList->m_cells.size() == cellsMap[cellList].size())
                temp_cellLists.append({m_cellLists.indexOf(cellList), cellList});
        }

        const auto cellLists = cellsMap.keys();
        for (const auto &cellList : cellLists) {
            const auto indexes = cellsMap[cellList].keys();
            for (const auto &index : indexes) {
                auto *cell = cellsMap[cellList][index];
                cellList->removeCell(cell);
                delete cell;
            }
        }

        for (const auto &[fst, snd] : temp_cellLists)
            this->removeList(snd);

        this->repaintCellLists();
    }

    CellList *LyricWrapView::mapToList(const QPointF &pos) {
        qreal height = 0;
        for (const auto &cellList : m_cellLists) {
            height += cellList->height();
            if (height >= pos.y())
                return cellList;
        }
        return nullptr;
    }

    QPointF LyricWrapView::mapToCellRect(const QPointF &pos) {
        const auto cellList = this->mapToList(pos);
        if (cellList == nullptr)
            return {};
        for (const auto &cell : cellList->m_cells) {
            const qreal topY = cell->mapToScene(cell->boundingRect()).boundingRect().top();
            const qreal bottomY = topY + cell->height();
            if (topY <= pos.y() && bottomY >= pos.y()) {
                return {topY, bottomY};
            }
        }

        return {};
    }

    void LyricWrapView::selectCells(const QPointF &startPos, const QPointF &scenePos) {
        QPointF startCellPos = startPos, endCellPos = scenePos;
        const auto cellRect = mapToCellRect(scenePos);
        if (cellRect.x() != 0 && cellRect.x() <= startPos.y() && cellRect.y() >= startPos.y() &&
            cellRect.x() <= scenePos.y() && cellRect.y() >= scenePos.y()) {
            if (scenePos.x() <= startPos.x())
                qSwap(startCellPos, endCellPos);
        } else {
            if (scenePos.y() <= startPos.y())
                qSwap(startCellPos, endCellPos);
        }

        for (const auto &cellList : m_cellLists)
            cellList->selectCells(startCellPos, endCellPos);
    }

    void LyricWrapView::updateRect() {
        const auto width = this->width() - this->verticalScrollBar()->width();
        for (const auto &m_cellList : m_cellLists) {
            m_cellList->updateSplitter(width);
        }

        if (width != this->sceneRect().width())
            this->setSceneRect(QRectF(0, 0, width, this->height()));
        this->update();
    }

    QStringList LyricWrapView::cellBackgroundBrush() const { return m_cellBackgroundBrush; }

    void LyricWrapView::setCellBackgroundBrush(const QStringList &cellBackgroundBrush) {
        m_cellBackgroundBrush = cellBackgroundBrush;
    }

    QStringList LyricWrapView::cellBorderPen() const { return m_cellBorderPen; }

    void LyricWrapView::setCellBorderPen(const QStringList &cellBorderPen) { m_cellBorderPen = cellBorderPen; }

    QStringList LyricWrapView::cellLyricPen() const { return m_cellLyricPen; }

    void LyricWrapView::setCellLyricPen(const QStringList &cellLyricPen) { m_cellLyricPen = cellLyricPen; }

    QStringList LyricWrapView::cellSyllablePen() const { return m_cellSyllablePen; }

    void LyricWrapView::setCellSyllablePen(const QStringList &cellSyllablePen) { m_cellSyllablePen = cellSyllablePen; }

    QStringList LyricWrapView::handleBackgroundBrush() const { return m_handleBackgroundBrush; }

    void LyricWrapView::setHandleBackgroundBrush(const QStringList &handleBackgroundBrush) {
        m_handleBackgroundBrush = handleBackgroundBrush;
    }

    QStringList LyricWrapView::splitterPen() const { return m_splitterPen; }

    void LyricWrapView::setSplitterPen(const QStringList &splitterPen) { m_splitterPen = splitterPen; }

} // namespace FillLyric
