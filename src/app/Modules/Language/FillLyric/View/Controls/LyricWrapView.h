#ifndef LYRICWRAPVIEW_H
#define LYRICWRAPVIEW_H

#include "LyricCell.h"
#include "CellList.h"
#include <QGraphicsView>

namespace FillLyric {

    class LyricWrapView final : public QGraphicsView {
        Q_OBJECT
    public:
        explicit LyricWrapView(QWidget *parent = nullptr);
        ~LyricWrapView() override;

        void clear();
        void init(const QList<QList<LangNote>> &noteLists);
        CellList *createNewList();
        void appendList(const QList<LangNote *> &noteList);

        void repaintCellLists();

        [[nodiscard]] QUndoStack *history() const;

        QList<CellList *> m_cellLists;

    protected:
        void resizeEvent(QResizeEvent *event) override;
        void wheelEvent(QWheelEvent *event) override;

    private:
        void connectCellList(CellList *cellList);
        [[nodiscard]] qreal cellBaseY(const int &index) const;

        QFont m_font;
        QUndoStack *m_history = new QUndoStack();
        QGraphicsScene *m_scene;
    };


}
#endif // LYRICWRAPVIEW_H
