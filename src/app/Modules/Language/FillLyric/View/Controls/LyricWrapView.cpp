#include "LyricWrapView.h"
#include <QWheelEvent>
#include <QScrollBar>

#include <LangMgr/ILanguageManager.h>

#include "../../Commands/Line/DeleteLineCmd.h"
#include "../../Commands/Line/AddPrevLineCmd.h"
#include "../../Commands/Line/AddNextLineCmd.h"

namespace FillLyric {
    LyricWrapView::LyricWrapView(QWidget *parent) {
        m_font = this->font();
        m_scene = new QGraphicsScene(parent);

        this->setScene(m_scene);
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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

    CellList *LyricWrapView::createNewList() {
        const auto width = this->width() - this->verticalScrollBar()->width();
        const auto cellList =
            new CellList(0, 0, {new LangNote()}, m_scene, m_font, this, m_history);
        cellList->setWidth(width);
        this->connectCellList(cellList);
        return cellList;
    }

    void LyricWrapView::appendList(const QList<LangNote *> &noteList) {
        const auto width = this->width() - this->verticalScrollBar()->width();
        const auto cellList = new CellList(0, cellBaseY(static_cast<int>(m_cellLists.size())),
                                           noteList, m_scene, m_font, this, m_history);
        cellList->setWidth(width);
        m_cellLists.append(cellList);
        this->connectCellList(cellList);
    }

    QUndoStack *LyricWrapView::history() const {
        return m_history;
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
        this->update();
    }

    void LyricWrapView::repaintCellLists() {
        qreal height = 0;
        const auto width = this->width() - this->verticalScrollBar()->width();
        for (const auto &m_cellList : m_cellLists) {
            m_cellList->setBaseY(height);
            m_cellList->setWidth(width);
            height += m_cellList->height();
        }
        this->setSceneRect(scene()->itemsBoundingRect());
        this->update();
    }

    void LyricWrapView::connectCellList(CellList *cellList) {
        connect(cellList, &CellList::heightChanged, this, &LyricWrapView::repaintCellLists);
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
}