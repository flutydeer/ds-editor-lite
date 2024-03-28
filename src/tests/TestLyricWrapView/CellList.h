#ifndef CELLLIST_H
#define CELLLIST_H

#include <QObject>

#include "LyricCell.h"
#include "SplitterItem.h"

namespace LyricWrap {

    class CellList final : public QObject {
        Q_OBJECT
    public:
        explicit CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList,
                          QGraphicsScene *scene, const QFont &font);

        [[nodiscard]] qreal height() const;

        void setWidth(const qreal &width);

        void setBaseY(const qreal &y);

        void setFont(const QFont &font);

        [[nodiscard]] qreal deltaY() const;

    Q_SIGNALS:
        void heightChanged() const;
        void cellPosChanged() const;

    private:
        void updateSplitterPos() const;
        void updateCellPos();

        bool autoWarp = true;

        qreal mX;
        qreal mY;

        qreal m_curWidth = 0;

        qreal m_height;
        qreal m_cellHeight;
        QFont m_font;

        qreal m_cellMargin = 5;

        QGraphicsScene *m_scene;

        SplitterItem *m_splitter;

        QList<qreal> m_widths;

        QList<LyricCell *> m_cells;
    };
}

#endif // CELLLIST_H
