#ifndef CELLLIST_H
#define CELLLIST_H

#include <QObject>
#include <QTimer>
#include <QUndoStack>

#include "LyricCell.h"
#include "HandleItem.h"
#include "SplitterItem.h"

namespace FillLyric {

    class CellList final : public QGraphicsObject {
        Q_OBJECT
    public:
        explicit CellList(const qreal &x, const qreal &y, const QList<LangNote *> &noteList,
                          QGraphicsScene *scene, QGraphicsView *view, QUndoStack *undoStack);

        void clear();
        void setAutoWrap(const bool &autoWrap);

        void highlight();

        [[nodiscard]] qreal deltaX() const;

        [[nodiscard]] qreal y() const;
        [[nodiscard]] qreal deltaY() const;

        void setBaseY(const qreal &y);

        [[nodiscard]] qreal height() const;

        [[nodiscard]] QGraphicsView *view() const;
        [[nodiscard]] QGraphicsScene *scene() const;

        [[nodiscard]] LyricCell *createNewCell() const;

        void appendCell(LyricCell *cell);
        void removeCell(LyricCell *cell);
        void insertCell(const int &index, LyricCell *cell);

        void addToScene();
        void removeFromScene();

        void setWidth(const qreal &width);
        void updateSplitter(const qreal &width) const;

        void setFont(const QFont &font);
        void updateRect(LyricCell *cell);
        void updateCellPos();

        void connectCell(const LyricCell *cell) const;
        void disconnectCell(const LyricCell *cell) const;

        QList<LyricCell *> m_cells;

    Q_SIGNALS:
        void heightChanged() const;
        void cellPosChanged() const;

        void deleteLine() const;
        void addPrevLine() const;
        void addNextLine() const;
        void linebreakSignal(const int &cellIndex) const;

    public Q_SLOTS:
        void selectList() const;

    protected:
        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        [[nodiscard]] QRectF boundingRect() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                   QWidget *widget) override;

    private:
        void updateSplitterPos() const;

        bool m_autoWarp = false;
        QTimer *highlightTimer;
        QRectF m_highlightRect = {};

        qreal mX;
        qreal mY;

        qreal m_curWidth = 0;
        qreal m_height = 0;
        qreal m_cellMargin = 5;

        QFont m_font = QApplication::font();
        QGraphicsView *m_view;
        QGraphicsScene *m_scene;
        QUndoStack *m_history;

        SplitterItem *m_splitter;
        HandleItem *m_handle;

    private Q_SLOTS:
        void resetHighlight();

        void editCell(LyricCell *cell, const QString &lyric);
        void changeSyllable(LyricCell *cell, const QString &syllable);
        void clearCell(LyricCell *cell);
        void deleteCell(LyricCell *cell);
        void addPrevCell(LyricCell *cell);
        void addNextCell(LyricCell *cell);
        void linebreak(LyricCell *cell) const;
    };
}

#endif // CELLLIST_H
