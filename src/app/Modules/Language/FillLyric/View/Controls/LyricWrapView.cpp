#include "LyricWrapView.h"

#include <QMenu>
#include <QScrollBar>
#include <QMouseEvent>

#include <LangMgr/ILanguageManager.h>

#include "../../Commands/Line/LinebreakCmd.h"
#include "../../Commands/Line/AppendCellCmd.h"
#include "../../Commands/Line/DeleteLineCmd.h"
#include "../../Commands/Line/AddPrevLineCmd.h"
#include "../../Commands/Line/AddNextLineCmd.h"

#include "../../Commands/View/DeleteCellsCmd.h"

namespace FillLyric {
    LyricWrapView::LyricWrapView(QWidget *parent) {
        m_font = this->font();
        m_scene = new QGraphicsScene(parent);

        this->setScene(m_scene);
        this->setDragMode(RubberBandDrag);

        if (m_autoWrap) {
            this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        } else {
            this->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        }
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

        this->setBackgroundBrush(QColor(35, 36, 37));

        setAlignment(Qt::AlignLeft | Qt::AlignTop);
        setRenderHint(QPainter::Antialiasing, true);
        this->installEventFilter(this);

        // notesCount
        connect(m_scene, &QGraphicsScene::changed, [this] {
            Q_EMIT noteCountChanged(
                static_cast<int>(m_scene->items().size() - m_cellLists.size() * 3));
        });
    }

    LyricWrapView::~LyricWrapView() = default;

    void LyricWrapView::resizeEvent(QResizeEvent *event) {
        QGraphicsView::resizeEvent(event);
        repaintCellLists();
    }

    void LyricWrapView::wheelEvent(QWheelEvent *event) {
        if (event->modifiers() & Qt::ControlModifier) {
            const auto fontSizeDelta = event->angleDelta().y() / 120.0;
            QFont font = this->font();
            const auto newSize = font.pointSizeF() + fontSizeDelta;
            if (newSize > 3) {
                font.setPointSizeF(newSize);
                this->setFont(font);
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
            rubberBandOrigin = event->pos();
            rubberBand.setGeometry(QRect(rubberBandOrigin, QSize()));
            rubberBand.show();
        }

        if (event->button() == Qt::RightButton) {
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
                for (const auto &item : selectedItems) {
                    item->setSelected(false);
                }
                itemAtPos->setSelected(true);
                mapToList(scenePos)->selectList();
            }
            return;
        }
        QGraphicsView::mousePressEvent(event);
    }

    void LyricWrapView::mouseMoveEvent(QMouseEvent *event) {
        if (event->buttons() & Qt::LeftButton) {
            rubberBand.setGeometry(QRect(rubberBandOrigin, event->pos()).normalized());
            QRect rect = rubberBand.geometry();
            rect = rect.normalized();

            if ((event->pos() - rubberBandOrigin).manhattanLength() > 10) {
                mapToList(rubberBandOrigin)->setSelected(false);
            }

            QList<QGraphicsItem *> itemsInRect = items(rect);
            for (QGraphicsItem *item : itemsInRect) {
                if (!dynamic_cast<CellList *>(item)) {
                    item->setSelected(true);
                }
            }
        }
    }

    void LyricWrapView::mouseReleaseEvent(QMouseEvent *event) {
        if (event->button() == Qt::LeftButton) {
            rubberBand.hide();
            QRect rect = rubberBand.geometry();
            rect = rect.normalized();

            QList<QGraphicsItem *> itemsInRect = items(rect);
            for (QGraphicsItem *item : itemsInRect) {
                if (!dynamic_cast<CellList *>(item)) {
                    item->setSelected(true);
                }
            }
        }
        rubberBandOrigin = QPoint();
        rubberBand.setGeometry(QRect(rubberBandOrigin, QSize()));
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
                menu->addAction("clear cells(没做)", [=] {
                    // TODO: clear cells
                });
                menu->addAction("delete cells", [=] {
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
            menu->addSeparator();
            menu->addAction("delete lines(没做)", [=] {
                // TODO: delete lines
            });
            menu->addAction("move up(没做)", [=] {
                // TODO: move up
            });
            menu->addAction("move down(没做)", [=] {
                // TODO: move down
            });
            menu->exec(mapToGlobal(clickPos));
            event->accept();
            delete menu;
            return;
        }

        // selected handle or space
        if (!dynamic_cast<LyricCell *>(itemAtPos)) {
            if (const auto cellList = mapToList(scenePos)) {
                menu->addAction("append cell", [this, cellList] {
                    m_history->push(new AppendCellCmd(this, cellList));
                });
                menu->addSeparator();
                menu->addAction("delete line", [this, cellList] {
                    m_history->push(new DeleteLineCmd(this, cellList));
                });
                menu->addAction("add prev line", [this, cellList] {
                    m_history->push(new AddPrevLineCmd(this, cellList));
                });
                menu->addAction("add next line", [this, cellList] {
                    m_history->push(new AddNextLineCmd(this, cellList));
                });
                menu->addSeparator();
                menu->addAction("move up(没做)", [=] {
                    // TODO: move up
                });
                menu->addAction("move down(没做)", [=] {
                    // TODO: move down
                });
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
        cellList->setAutoWrap(m_autoWrap);
        cellList->setWidth(width);
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
        cellList->setAutoWrap(m_autoWrap);
        cellList->setWidth(width);
        m_cellLists.append(cellList);
        this->connectCellList(cellList);
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
        this->updateRect();
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
        const auto width = this->width() - this->verticalScrollBar()->width();
        const auto maxWidth = this->maxListWidth();
        for (const auto &m_cellList : m_cellLists) {
            m_cellList->setBaseY(height);
            if (m_autoWrap)
                m_cellList->setWidth(width);
            else
                m_cellList->setWidth(maxWidth);
            height += m_cellList->height();
        }

        for (const auto &m_cellList : m_cellLists) {
            if (m_autoWrap)
                m_cellList->updateSplitter(width);
            else
                m_cellList->updateSplitter(maxWidth);
        }

        if (m_autoWrap)
            this->setSceneRect(QRectF(0, 0, width, height));
        else
            this->setSceneRect(QRectF(0, 0, maxWidth, height));
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

        connect(cellList, &CellList::linebreakSignal, [this, cellList](const int cellIndex) {
            m_history->push(new LinebreakCmd(this, cellList, cellIndex));
        });

        connect(cellList, &CellList::deleteLine,
                [this, cellList] { m_history->push(new DeleteLineCmd(this, cellList)); });
        connect(cellList, &CellList::addPrevLine,
                [this, cellList] { m_history->push(new AddPrevLineCmd(this, cellList)); });
        connect(cellList, &CellList::addNextLine,
                [this, cellList] { m_history->push(new AddNextLineCmd(this, cellList)); });
    }

    qreal LyricWrapView::cellBaseY(const int &index) const {
        qreal height = 0;
        for (int i = 0; i < std::min(index, static_cast<int>(m_cellLists.size())); i++) {
            height += m_cellLists[i]->height();
        }
        return height;
    }

    CellList *LyricWrapView::mapToList(const QPoint &pos) {
        qreal height = 0;
        for (const auto &cellList : m_cellLists) {
            height += cellList->height();
            if (height >= pos.y())
                return cellList;
        }
        return nullptr;
    }

    void LyricWrapView::updateRect() {
        const auto width = this->width() - this->verticalScrollBar()->width();
        const auto maxWidth = this->maxListWidth();
        for (const auto &m_cellList : m_cellLists) {
            if (m_autoWrap)
                m_cellList->updateSplitter(width);
            else
                m_cellList->updateSplitter(maxWidth);
        }

        if (m_autoWrap)
            this->setSceneRect(QRectF(0, 0, width, this->height()));
        else
            this->setSceneRect(QRectF(0, 0, maxWidth, this->height()));
        this->update();
    }
}