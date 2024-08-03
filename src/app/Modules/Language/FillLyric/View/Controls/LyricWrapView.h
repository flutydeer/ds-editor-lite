#ifndef LYRICWRAPVIEW_H
#define LYRICWRAPVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QRubberBand>

#include "LyricCell.h"
#include "CellList.h"

namespace FillLyric {
    class LyricWrapView final : public QGraphicsView {
        Q_OBJECT

    public:
        explicit LyricWrapView(QWidget *parent = nullptr);
        ~LyricWrapView() override;

        void clear();
        void init(const QList<QList<LangNote>> &noteLists);

        [[nodiscard]] bool autoWrap() const;
        void setAutoWrap(const bool &autoWrap);

        CellList *createNewList();

        void insertList(const int &index, CellList *cellList);
        void removeList(const int &index);
        void removeList(CellList *cellList);
        void appendList(const QList<LangNote *> &noteList);

        void moveUpLists(const QList<CellList *> &cellLists);
        void moveDownLists(QList<CellList *> cellLists);

        CellList *mapToList(const QPoint &pos);
        QPointF mapToCellRect(const QPoint &pos);
        void selectCells(const QPoint &startPos, const QPoint &scenePos);
        void repaintCellLists();

        [[nodiscard]] QUndoStack *history() const;

        [[nodiscard]] QList<CellList *> cellLists() const;

    Q_SIGNALS:
        void fontSizeChanged();
        void noteCountChanged(const int &count);

    protected:
        void keyPressEvent(QKeyEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void mouseReleaseEvent(QMouseEvent *event) override;
        void contextMenuEvent(QContextMenuEvent *event) override;

    private:
        [[nodiscard]] qreal maxListWidth() const;
        [[nodiscard]] qreal height();
        void connectCellList(CellList *cellList);
        [[nodiscard]] qreal cellBaseY(const int &index) const;

        bool m_autoWrap = false;

        QFont m_font;
        QUndoStack *m_history = new QUndoStack();
        QGraphicsScene *m_scene;
        SplitterItem *m_endSplitter;

        QList<CellList *> m_cellLists;
        QList<LyricCell *> m_selectedCells{};

        QPoint rubberBandOrigin;
        QPoint lastClickPos;

    private Q_SLOTS:
        void updateRect();
    };
}
#endif // LYRICWRAPVIEW_H