#include "LyricWrapView.h"

#include <QScrollBar>
namespace LyricWrap {
    LyricWrapView::LyricWrapView(QWidget *parent) {
        m_scene = new QGraphicsScene(parent);
        this->setScene(m_scene);
        this->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        this->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

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

    void LyricWrapView::appendList(const QList<LangNote *> &noteList) {
        const auto cellList =
            new CellList(0, cellBaseY(static_cast<int>(m_cellLists.size())), noteList, m_scene);
        cellList->setWidth(this->width());
        m_heights.append(cellList->height());
        m_cellLists.append(cellList);

        connect(cellList, &CellList::rowCountChanged, this, &LyricWrapView::repaintCellLists);
    }

    qreal LyricWrapView::cellBaseY(const int &index) const {
        return std::accumulate(m_heights.constBegin(), m_heights.constBegin() + index, 0.0);
    }

    void LyricWrapView::repaintCellLists() {
        const auto width = this->width();
        for (int i = 0; i < m_cellLists.size(); i++) {
            m_cellLists[i]->setBaseY(cellBaseY(i));
            m_cellLists[i]->setWidth(width);
            m_heights[i] = m_cellLists[i]->height();
        }
        const auto height = std::accumulate(m_heights.constBegin(), m_heights.constEnd(), 0.0);
        this->setFixedHeight(static_cast<int>(height));
        scene()->setSceneRect(rect());
    }

    void LyricWrapView::refreshSceneRect() const {
        QRectF sceneRect = scene()->sceneRect();
        sceneRect.setWidth(width());
        sceneRect.setHeight(height());
        scene()->setSceneRect(sceneRect);
    }

}