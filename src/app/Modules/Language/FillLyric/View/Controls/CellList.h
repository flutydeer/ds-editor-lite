#ifndef CELLLIST_H
#define CELLLIST_H

#include <QObject>
#include <QUndoStack>

#include "LyricCell.h"
#include "SplitterItem.h"

namespace FillLyric {

    class CellList final : public QObject {
        Q_OBJECT
    public:
        explicit CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList,
                          QGraphicsScene *scene, const QFont &font, QGraphicsView *view,
                          QUndoStack *undoStack);

        void clear();

        [[nodiscard]] qreal y() const;
        [[nodiscard]] qreal deltaY() const;

        void setBaseY(const qreal &y);

        [[nodiscard]] qreal height() const;

        [[nodiscard]] QGraphicsView *view() const;
        [[nodiscard]] QGraphicsScene *sence() const;

        LyricCell *createNewCell();

        void setWidth(const qreal &width);

        void setFont(const QFont &font);
        void updateRect(LyricCell *cell);
        void updateCellPos();

        QList<LyricCell *> m_cells;

    Q_SIGNALS:
        void heightChanged() const;
        void cellPosChanged() const;

        void deleteLine() const;
        void addPrevLine() const;
        void addNextLine() const;

    private:
        void updateSplitterPos() const;

        void connectCell(LyricCell *cell);

        bool autoWarp = true;

        qreal mX;
        qreal mY;

        qreal m_curWidth = 0;

        qreal m_height = 0;

        qreal m_cellMargin = 5;

        QFont m_font;
        QGraphicsView *m_view;
        QGraphicsScene *m_scene;
        QUndoStack *m_history;

        SplitterItem *m_splitter;
    };
}

#endif // CELLLIST_H
