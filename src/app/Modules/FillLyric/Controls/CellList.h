#ifndef LYRIC_TAB_CONTROLS_CELL_LIST_H
#define LYRIC_TAB_CONTROLS_CELL_LIST_H

#include <QApplication>
#include <QMap>

#include <string>

#include "Modules/FillLyric/Controls/HandleItem.h"
#include "Modules/FillLyric/Controls/SplitterItem.h"
#include "Modules/FillLyric/LangCommon.h"

namespace FillLyric
{
    class CellQss;
    class LyricCell;

    class CellList final : public QGraphicsObject {
        Q_OBJECT

    public:
        explicit CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList, QGraphicsScene *scene,
                          QGraphicsView *view, QList<CellList *> *cellLists,
                          const QStringList &priorityG2pIds = {},
                          const QMap<std::string, std::string> &langToG2pId = {});
        ~CellList() override;

        void clear();

        qreal deltaX() const;
        qreal deltaY() const;

        void setBaseY(const qreal &y);

        qreal height() const;
        qreal cellWidth() const;

        QGraphicsView *view() const;

        LyricCell *createNewCell();

        void highlight();
        void selectCells(const QPointF &startPos, const QPointF &endPos);

        void appendCell(LyricCell *cell);
        void removeCell(LyricCell *cell);
        void insertCell(const int &index, LyricCell *cell);

        void addToScene();
        void removeFromScene();

        void setWidth(const qreal &width);
        void updateSplitter(const qreal &width);

        void setFont(const QFont &font);
        void updateFontOnly(const QFont &font);
        void recalcCellRects();
        void updateRect(LyricCell *cell);
        void updateCellPos();

        void connectCell(const LyricCell *cell);
        void disconnectCell(const LyricCell *cell) const;

        QList<LyricCell *> m_cells;

    Q_SIGNALS:
        void heightChanged() const;
        void cellPosChanged() const;

        void requestDeleteLine(CellList *cellList);
        void requestAddPrevLine(CellList *cellList);
        void requestAddNextLine(CellList *cellList);
        void requestMoveUpLine(CellList *cellList);
        void requestMoveDownLine(CellList *cellList);

        void linebreakSignal(const int &cellIndex) const;

    public Q_SLOTS:
        void selectList() const;

    protected:
        QRectF boundingRect() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

        void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    private:
        void showContextMenu(const QPointF &pos);
        void setCellQss() const;
        QFont syllableFont() const;

        qreal m_curWidth = 0;
        qreal m_height = 0;
        qreal m_cellMargin = 5;

        QFont m_font = QApplication::font();
        QGraphicsView *m_view;
        QGraphicsScene *m_scene;

        SplitterItem *m_splitter;
        HandleItem *m_handle;

        CellQss *m_cellQss;
        QList<CellList *> *m_cellLists;

        QStringList m_priorityG2pIds;
        QMap<std::string, std::string> m_langToG2pId;

    private Q_SLOTS:
        void editCell(FillLyric::LyricCell *cell, const QString &lyric);
        void changeSyllable(FillLyric::LyricCell *cell, const QString &syllable);
        static void clearCell(FillLyric::LyricCell *cell);
        void deleteCell(FillLyric::LyricCell *cell);
        void addPrevCell(FillLyric::LyricCell *cell);
        void addNextCell(FillLyric::LyricCell *cell);
        void linebreak(FillLyric::LyricCell *cell) const;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_CONTROLS_CELL_LIST_H
