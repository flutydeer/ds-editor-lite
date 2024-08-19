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

        Q_PROPERTY(
            QStringList cellBackgroundBrush READ cellBackgroundBrush WRITE setCellBackgroundBrush)
        Q_PROPERTY(QStringList cellBorderPen READ cellBorderPen WRITE setCellBorderPen)
        Q_PROPERTY(QStringList cellLyricPen READ cellLyricPen WRITE setCellLyricPen)
        Q_PROPERTY(QStringList cellSyllablePen READ cellSyllablePen WRITE setCellSyllablePen)
        Q_PROPERTY(QStringList handleBackgroundBrush READ handleBackgroundBrush WRITE
                       setHandleBackgroundBrush)
        Q_PROPERTY(QStringList spliterPen READ spliterPen WRITE setSpliterPen)

    public:
        explicit LyricWrapView(QString qssPath = "", QWidget *parent = nullptr);
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

        CellList *mapToList(const QPointF &pos);
        QPointF mapToCellRect(const QPointF &pos);
        void selectCells(const QPointF &startPos, const QPointF &scenePos);
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


        QStringList m_cellBackgroundBrush;
        QStringList cellBackgroundBrush() const;
        void setCellBackgroundBrush(const QStringList &cellBackgroundBrush);

        QStringList m_cellBorderPen;
        QStringList cellBorderPen() const;
        void setCellBorderPen(const QStringList &cellBorderPen);

        QStringList m_cellLyricPen;
        QStringList cellLyricPen() const;
        void setCellLyricPen(const QStringList &cellLyricPen);

        QStringList m_cellSyllablePen;
        QStringList cellSyllablePen() const;
        void setCellSyllablePen(const QStringList &cellSyllablePen);

        QStringList m_handleBackgroundBrush;
        QStringList handleBackgroundBrush() const;
        void setHandleBackgroundBrush(const QStringList &handleBackgroundBrush);

        QStringList m_spliterPen;
        QStringList spliterPen() const;
        void setSpliterPen(const QStringList &spliterPen);

    private:
        [[nodiscard]] qreal maxListWidth() const;
        [[nodiscard]] qreal height();
        void connectCellList(CellList *cellList);
        [[nodiscard]] qreal cellBaseY(const int &index) const;

        bool m_autoWrap = false;

        QFont m_font;
        QUndoStack *m_history = new QUndoStack();
        QGraphicsScene *m_scene;
        SplitterItem *m_startSplitter = nullptr;

        QList<CellList *> m_cellLists;
        QList<LyricCell *> m_selectedCells{};

        QPoint rubberBandOrigin;
        QPoint lastClickPos;

        QString m_qssPath;

    Q_SIGNALS:
        void cellBackgroundBrushChanged();

    private Q_SLOTS:
        void updateRect();
    };
}
#endif // LYRICWRAPVIEW_H