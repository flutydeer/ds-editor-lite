#include "LyricWrapView.h"

#include <QMenu>
#include <QScrollBar>
#include <QMouseEvent>

#include <language-manager/ILanguageManager.h>

#include "../../Commands/Line/LinebreakCmd.h"
#include "../../Commands/Line/AppendCellCmd.h"
#include "../../Commands/Line/DeleteLineCmd.h"
#include "../../Commands/Line/AddPrevLineCmd.h"
#include "../../Commands/Line/AddNextLineCmd.h"

#include "../../Commands/View/ClearCellsCmd.h"
#include "../../Commands/View/DeleteCellsCmd.h"
#include "../../Commands/View/DeleteLinesCmd.h"
#include "../../Commands/View/MoveUpLinesCmd.h"
#include "../../Commands/View/MoveDownLinesCmd.h"

namespace FillLyric {
    LyricWrapView::LyricWrapView(QWidget *parent) {
        this->setObjectName("LyricWrapView");

        m_font = this->font();
        m_scene = new QGraphicsScene(parent);

        m_endSplitter = new SplitterItem(0, 0, this->width());
        m_scene->addItem(m_endSplitter);

        this->setScene(m_scene);
        this->setDragMode(RubberBandDrag);

        this->setHorizontalScrollBarPolicy(m_autoWrap
                                               ? Qt::ScrollBarAlwaysOff
                                               : Qt::ScrollBarAsNeeded);
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setRenderHint(QPainter::Antialiasing, true);
        this->installEventFilter(this);

        // notesCount
        connect(m_scene, &QGraphicsScene::changed, [this] {
            Q_EMIT noteCountChanged(
                static_cast<int>(m_scene->items().size() - m_cellLists.size() * 3 - 1));
        });
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
            if (!selectedCells.isEmpty())
                m_history->push(new ClearCellsCmd(this, selectedCells));
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
                if (event->key() == Qt::Key_Up && !listSet.contains(m_cellLists.first()))
                    m_history->push(new MoveUpLinesCmd(this, QList(listSet.values())));
                else if (event->key() == Qt::Key_Down && !listSet.contains(m_cellLists.last()))
                    m_history->push(new MoveDownLinesCmd(this, QList(listSet.values())));
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
            QFont font = this->font();
            const auto newSize = font.pointSizeF() + fontSizeDelta;
            if (newSize >= 9 && newSize != font.pointSizeF()) {
                font.setPointSizeF(newSize);
                this->setFont(font);
                Q_EMIT this->fontSizeChanged();
                for (const auto &cellList : m_cellLists) {
                    cellList->setFont(font);
                }
                event->accept();
            }
        } else {
            QGraphicsView::wheelEvent(event);
        }
    }

    void LyricWrapView::mousePressEvent(QMouseEvent *event) {
        const auto selectedItems = scene()->selectedItems();
        const auto scenePos = mapToScene(event->pos()).toPoint();
        const auto itemAtPos = scene()->itemAt(scenePos, QTransform());

        if (event->button() == Qt::LeftButton) {
            // 如果按下了shift
            if (event->modifiers() & Qt::ShiftModifier) {
                int cellCount = 0;
                QPointF shiftStartPos{};
                for (const auto item : scene()->selectedItems()) {
                    if (const auto cell = dynamic_cast<LyricCell *>(item)) {
                        shiftStartPos.setX(cell->mapToScene(cell->boundingRect().center()).x());
                        shiftStartPos.setY(cell->mapToScene(cell->boundingRect().center()).y());
                        cellCount++;
                    }
                }
                if (cellCount > 1) {
                    for (const auto item : scene()->selectedItems()) {
                        item->setSelected(false);
                    }
                    this->selectCells(lastClickPos, scenePos);
                } else if (shiftStartPos.x() != 0 && (event->pos() - shiftStartPos).
                           manhattanLength() >
                           10) {
                    this->selectCells(shiftStartPos, scenePos);
                }
                event->accept();
                return;
            }

            if (const auto cellList = mapToList(scenePos)) {
                cellList->setSelected(false);
            }
            rubberBandOrigin = scenePos;
        }

        // 右键取消选中
        if (event->button() == Qt::RightButton) {
            // 右击空白处取消选中&&右击Item选中、取消其他选中
            if (!dynamic_cast<HandleItem *>(itemAtPos)) {
                if (!itemAtPos && !selectedItems.isEmpty())
                    for (const auto &item : selectedItems) {
                        item->setSelected(false);
                    }
                if (itemAtPos) {
                    if (selectedItems.contains(itemAtPos))
                        return;
                    for (const auto &item : selectedItems) {
                        item->setSelected(false);
                    }
                    itemAtPos->setSelected(true);
                }
            } else {
                // 右击行首手柄取消其他选中、选中当前整行
                if (itemAtPos->isSelected())
                    return;
                for (const auto &item : selectedItems) {
                    item->setSelected(false);
                }
                itemAtPos->setSelected(true);
                if (const auto cellList = mapToList(scenePos))
                    cellList->selectList();
            }
            return;
        }
        QGraphicsView::mousePressEvent(event);
    }

    void LyricWrapView::mouseMoveEvent(QMouseEvent *event) {
        if (event->buttons() & Qt::LeftButton) {
            const auto scenePos = mapToScene(event->pos()).toPoint();

            if ((event->pos() - rubberBandOrigin).manhattanLength() > 10) {
                if (const auto cellList = mapToList(rubberBandOrigin))
                    cellList->setSelected(false);

                this->selectCells(rubberBandOrigin, scenePos);
            }

            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }

    void LyricWrapView::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier))
            lastClickPos = mapToScene(event->pos()).toPoint();
        QGraphicsView::mouseReleaseEvent(event);
    }

    void LyricWrapView::contextMenuEvent(QContextMenuEvent *event) {
        auto *menu = new QMenu(this);
        menu->setAttribute(Qt::WA_TranslucentBackground);
        menu->setWindowFlags(menu->windowFlags() | Qt::FramelessWindowHint |
                             Qt::NoDropShadowWindowHint);

        const auto clickPos = event->pos();
        const auto scenePos = mapToScene(clickPos).toPoint();
        const auto itemAtPos = scene()->itemAt(scenePos, QTransform());
        const auto selectedItems = scene()->selectedItems();
        QList<HandleItem *> handleItems;

        // selected cells
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

            if ((enableMenu && !m_selectedCells.isEmpty())) {
                menu->addAction(tr("clear cells"), [this] {
                    m_history->push(new ClearCellsCmd(this, m_selectedCells));
                });
                menu->addAction(tr("delete cells"), [this] {
                    m_history->push(new DeleteCellsCmd(this, m_selectedCells));
                });
                menu->exec(mapToGlobal(clickPos));
                event->accept();
                delete menu;
                return;
            }
        }

        // selected handles
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
            menu->addSeparator();
            menu->addAction(tr("delete lines"), [this, selectedSet] {
                m_history->push(new DeleteLinesCmd(this, QList(selectedSet.values())));
            });
            menu->addAction(tr("move up"), [this, selectedSet] {
                m_history->push(new MoveUpLinesCmd(this, QList(selectedSet.values())));
            });
            menu->addAction(tr("move down"), [this, selectedSet] {
                m_history->push(new MoveDownLinesCmd(this, QList(selectedSet.values())));
            });
            if (selectedSet.contains(m_cellLists.first()))
                menu->actions().at(2)->setEnabled(false);
            if (selectedSet.contains(m_cellLists.last()))
                menu->actions().at(3)->setEnabled(false);
            menu->exec(mapToGlobal(clickPos));
            event->accept();
            delete menu;
            return;
        }

        // selected handle or space
        if (!dynamic_cast<LyricCell *>(itemAtPos)) {
            if (const auto cellList = mapToList(scenePos)) {
                for (const auto item : selectedItems)
                    item->setSelected(false);
                cellList->setSelected(true);
                menu->addAction(tr("append cell"), [this, cellList] {
                    m_history->push(new AppendCellCmd(this, cellList));
                });
                menu->addSeparator();
                menu->addAction(tr("delete line"), [this, cellList] {
                    m_history->push(new DeleteLineCmd(this, cellList));
                });
                menu->addAction(tr("add prev line"), [this, cellList] {
                    m_history->push(new AddPrevLineCmd(this, cellList));
                });
                menu->addAction(tr("add next line"), [this, cellList] {
                    m_history->push(new AddNextLineCmd(this, cellList));
                });
                menu->addSeparator();
                menu->addAction(tr("move up"), [this, cellList] {
                    m_history->push(new MoveUpLinesCmd(this, {cellList}));
                });
                menu->addAction(tr("move down"), [this, cellList] {
                    m_history->push(new MoveDownLinesCmd(this, {cellList}));
                });
                if (cellList == m_cellLists.first())
                    menu->actions().at(6)->setEnabled(false);
                if (cellList == m_cellLists.last())
                    menu->actions().at(7)->setEnabled(false);
                menu->exec(mapToGlobal(clickPos));
            }
            event->accept();
            delete menu;
            return;
        }
        return QGraphicsView::contextMenuEvent(event);
    }

    CellList *LyricWrapView::createNewList() {
        const auto width = this->width() - this->verticalScrollBar()->width();
        const auto cellList = new CellList(0, 0, {new LangNote()}, m_scene, this, m_history);
        cellList->setWidth(width);
        cellList->setFont(this->font());
        cellList->setAutoWrap(m_autoWrap);
        this->connectCellList(cellList);
        return cellList;
    }

    void LyricWrapView::insertList(const int &index, CellList *cellList) {
        m_cellLists.insert(index, cellList);
        cellList->addToScene();
        this->repaintCellLists();
    }

    void LyricWrapView::removeList(const int &index) {
        if (index >= m_cellLists.size())
            return;
        const auto cellList = m_cellLists[index];
        cellList->removeFromScene();
        m_cellLists.remove(index);
        this->repaintCellLists();
    }

    void LyricWrapView::removeList(CellList *cellList) {
        const auto index = m_cellLists.indexOf(cellList);
        if (0 <= index && index < m_cellLists.size()) {
            this->removeList(static_cast<int>(index));
        }
    }

    void LyricWrapView::appendList(const QList<LangNote *> &noteList) {
        const auto width = this->width() - this->verticalScrollBar()->width();
        const auto cellList = new CellList(0, cellBaseY(static_cast<int>(m_cellLists.size())),
                                           noteList, m_scene, this, m_history);
        cellList->setWidth(width);
        cellList->setFont(this->font());
        cellList->setAutoWrap(m_autoWrap);
        m_cellLists.append(cellList);
        this->connectCellList(cellList);
    }

    void LyricWrapView::moveUpLists(const QList<CellList *> &cellLists) {
        for (auto cellList : cellLists) {
            if (const qlonglong i = m_cellLists.indexOf(cellList) >= 1)
                qSwap(m_cellLists[i], m_cellLists[i - 1]);
        }
    }

    void LyricWrapView::moveDownLists(QList<CellList *> cellLists) {
        for (auto it = cellLists.rbegin(); it != cellLists.rend(); ++it) {
            if (const qlonglong i = m_cellLists.indexOf(*it) < m_cellLists.size() - 1)
                qSwap(m_cellLists[i], m_cellLists[i + 1]);
        }
    }

    QUndoStack *LyricWrapView::history() const {
        return m_history;
    }

    QList<CellList *> LyricWrapView::cellLists() const {
        return m_cellLists;
    }

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
        const auto langMgr = LangMgr::ILanguageManager::instance();

        for (const auto &notes : noteLists) {
            QList<LangNote *> tempNotes;
            for (const auto &note : notes) {
                tempNotes.append(new LangNote(note));
            }
            langMgr->convert(tempNotes);
            if (!tempNotes.isEmpty())
                this->appendList(tempNotes);
        }
        this->repaintCellLists();
    }

    bool LyricWrapView::autoWrap() const {
        return m_autoWrap;
    }

    void LyricWrapView::setAutoWrap(const bool &autoWrap) {
        m_autoWrap = autoWrap;
        if (!autoWrap) {
            this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        } else {
            this->horizontalScrollBar()->setSliderPosition(0);
            this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        }

        for (const auto m_cellList : m_cellLists) {
            m_cellList->setAutoWrap(autoWrap);
        }
        this->repaintCellLists();
    }

    void LyricWrapView::repaintCellLists() {
        qreal height = 0;
        const auto width =
            m_autoWrap ? this->width() - this->verticalScrollBar()->width() : this->maxListWidth();
        for (const auto &m_cellList : m_cellLists) {
            m_cellList->setBaseY(height);
            m_cellList->setWidth(width);
            height += m_cellList->height();
        }
        m_endSplitter->setPos(0, height);
        height += m_endSplitter->height();

        for (const auto &m_cellList : m_cellLists) {
            m_cellList->updateSplitter(width);
            m_endSplitter->setWidth(width);
        }

        this->setSceneRect(QRectF(0, 0, width, height));
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
        height += m_endSplitter->height();
        return height;
    }

    void LyricWrapView::connectCellList(CellList *cellList) {
        connect(cellList, &CellList::heightChanged, this, &LyricWrapView::repaintCellLists);
        connect(cellList, &CellList::cellPosChanged, this, &LyricWrapView::updateRect);

        connect(cellList, &CellList::linebreakSignal, [this, cellList](const int cellIndex) {
            m_history->push(new LinebreakCmd(this, cellList, cellIndex));
        });

        connect(cellList, &CellList::deleteLine,
                [this, cellList] {
                    m_history->push(new DeleteLineCmd(this, cellList));
                });
        connect(cellList, &CellList::addPrevLine,
                [this, cellList] {
                    m_history->push(new AddPrevLineCmd(this, cellList));
                });
        connect(cellList, &CellList::addNextLine,
                [this, cellList] {
                    m_history->push(new AddNextLineCmd(this, cellList));
                });
    }

    qreal LyricWrapView::cellBaseY(const int &index) const {
        qreal height = 0;
        for (int i = 0; i < std::min(index, static_cast<int>(m_cellLists.size())); i++) {
            height += m_cellLists[i]->height();
        }
        return height;
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
            // 获取boundingRect的上下两个y坐标
            const qreal topY = cell->mapToScene(cell->boundingRect()).boundingRect().top();
            const qreal bottomY = cell->mapToScene(cell->boundingRect()).boundingRect().
                                        bottom();
            if (topY <= pos.y() && bottomY >= pos.y()) {
                return {topY, bottomY};
            }
        }

        return {};
    }

    void LyricWrapView::selectCells(const QPointF &startPos, const QPointF &scenePos) {
        QPointF startCellPos = startPos, endCellPos = scenePos;
        const auto cellRect = mapToCellRect(scenePos);
        // 当前鼠标位于单行音符y坐标范围内
        if (cellRect.x() != 0 && cellRect.x() <= startPos.y() && cellRect.y() >=
            startPos.y() && cellRect.x() <= scenePos.y() && cellRect.y() >=
            scenePos.y()) {
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
        const auto width =
            m_autoWrap ? this->width() - this->verticalScrollBar()->width() : this->maxListWidth();
        for (const auto &m_cellList : m_cellLists) {
            m_cellList->updateSplitter(width);
            m_endSplitter->setWidth(width);
        }
        m_endSplitter->setPos(0, this->height() - m_endSplitter->height());
        this->setSceneRect(QRectF(0, 0, width, this->height()));
        this->update();
    }
}