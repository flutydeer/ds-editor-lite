#include "LyricWrapView.h"

#include <QMenu>
#include <QScrollBar>
#include <QWheelEvent>

#include <LangMgr/ILanguageManager.h>

#include "../../Commands/Line/AppendCellCmd.h"
#include "../../Commands/Line/DeleteLineCmd.h"
#include "../../Commands/Line/AddPrevLineCmd.h"
#include "../../Commands/Line/AddNextLineCmd.h"

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
        if (event->button() == Qt::RightButton) {
            return;
        }
        QGraphicsView::mousePressEvent(event);
    }

    void LyricWrapView::contextMenuEvent(QContextMenuEvent *event) {
        const auto clickPos = event->pos();
        m_selectedItems = scene()->selectedItems();
        if (!m_selectedItems.isEmpty()) {
            bool enableMenu = false;
            for (const auto &item : m_selectedItems) {
                if (item->type() == Qt::UserRole + 1) {
                    const auto cell = static_cast<LyricCell *>(item);
                    if (cell->lyricRect().contains(clickPos))
                        enableMenu = true;
                }
            }

            // TODO: delete cells
            if (enableMenu) {
                auto *menu = new QMenu(this);
                menu->addAction("delete cells(还没做)");
                menu->exec(mapToGlobal(clickPos));
                event->accept();
                if (!m_selectedItems.isEmpty()) {
                    for (const auto &item : m_selectedItems) {
                        item->setSelected(false);
                    }
                }
                return;
            }
        }

        if (!m_selectedItems.isEmpty()) {
            for (const auto &item : m_selectedItems) {
                item->setSelected(false);
            }
        }

        if (!itemAt(clickPos)) {
            if (const auto cellList = mapToList(clickPos)) {
                auto *menu = new QMenu(this);
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
                menu->exec(mapToGlobal(clickPos));
            }
            event->accept();
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
        for (const auto &cell : cellList->m_cells) {
            cellList->sence()->addItem(cell);
        }
        this->repaintCellLists();
    }

    void LyricWrapView::removeList(const int &index) {
        if (index >= m_cellLists.size())
            return;
        const auto cellList = m_cellLists[index];
        for (const auto &cell : cellList->m_cells) {
            m_scene->removeItem(cell);
        }
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
        for (const auto &m_cellList : m_cellLists) {
            m_cellList->setBaseY(height);
            m_cellList->setWidth(width);
            height += m_cellList->height();
        }
        for (const auto &m_cellList : m_cellLists) {
            m_cellList->updateSpliter(scene()->itemsBoundingRect().width());
        }
        this->setSceneRect(scene()->itemsBoundingRect());
        this->update();
    }

    void LyricWrapView::connectCellList(CellList *cellList) {
        connect(cellList, &CellList::heightChanged, this, &LyricWrapView::repaintCellLists);
        connect(cellList, &CellList::cellPosChanged, this, &LyricWrapView::updateRect);

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
        for (const auto &m_cellList : m_cellLists) {
            m_cellList->updateSpliter(scene()->itemsBoundingRect().width());
        }
        this->setSceneRect(scene()->itemsBoundingRect());
        this->update();
    }
}