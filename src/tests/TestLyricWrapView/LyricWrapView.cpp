#include "LyricWrapView.h"

#include <QScrollBar>
namespace LyricWrap {
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
            }
            event->accept();
        } else {
            QGraphicsView::wheelEvent(event);
        }
    }

    void LyricWrapView::appendList(const QList<LangNote *> &noteList) {
        const auto cellList = new CellList(0, cellBaseY(static_cast<int>(m_cellLists.size())),
                                           noteList, m_scene, m_font);
        cellList->setWidth(this->width());
        m_heights.append(cellList->height());
        m_cellLists.append(cellList);

        connect(cellList, &CellList::heightChanged, this, &LyricWrapView::repaintCellLists);
    }

    qreal LyricWrapView::cellBaseY(const int &index) const {
        return std::accumulate(m_heights.constBegin(), m_heights.constBegin() + index, 0.0);
    }

    void LyricWrapView::repaintCellLists() {
        const auto width = this->width() - this->verticalScrollBar()->width();
        for (int i = 0; i < m_cellLists.size(); i++) {
            m_cellLists[i]->setBaseY(cellBaseY(i));
            m_cellLists[i]->setWidth(width);
            m_heights[i] = m_cellLists[i]->height();
        }
        this->setSceneRect(scene()->itemsBoundingRect());
    }
}