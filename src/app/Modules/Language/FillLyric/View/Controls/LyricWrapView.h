#ifndef LYRICWRAPVIEW_H
#define LYRICWRAPVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>

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

        CellList *mapToList(const QPoint &pos);
        void repaintCellLists();

        [[nodiscard]] QUndoStack *history() const;

        [[nodiscard]] QList<CellList *> cellLists() const;

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;
        void mousePressEvent(QMouseEvent *event) override;
        void contextMenuEvent(QContextMenuEvent *event) override;

    private:
        void connectCellList(CellList *cellList);
        [[nodiscard]] qreal cellBaseY(const int &index) const;
        void deleteCells(const QList<QGraphicsItem *> &items);

        bool m_autoWrap = false;

        QFont m_font;
        QUndoStack *m_history = new QUndoStack();
        QGraphicsScene *m_scene;

        QList<CellList *> m_cellLists;

        QList<LyricCell *> m_selectedCells{};

    private Q_SLOTS:
        void updateRect();
    };
}
#endif // LYRICWRAPVIEW_H
