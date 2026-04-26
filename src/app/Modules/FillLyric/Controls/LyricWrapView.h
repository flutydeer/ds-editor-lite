#ifndef LYRIC_TAB_CONTROLS_LYRIC_WRAP_VIEW_H
#define LYRIC_TAB_CONTROLS_LYRIC_WRAP_VIEW_H

#include <QFont>
#include <QGraphicsView>
#include <QMap>

#include <string>

#include "Modules/FillLyric/LangCommon.h"

namespace FillLyric
{
    class CellList;
    class LyricCell;

    class LyricWrapView final : public QGraphicsView {
        Q_OBJECT
        Q_PROPERTY(QStringList cellBackgroundBrush READ cellBackgroundBrush WRITE setCellBackgroundBrush)
        Q_PROPERTY(QStringList cellBorderPen READ cellBorderPen WRITE setCellBorderPen)
        Q_PROPERTY(QStringList cellLyricPen READ cellLyricPen WRITE setCellLyricPen)
        Q_PROPERTY(QStringList cellSyllablePen READ cellSyllablePen WRITE setCellSyllablePen)
        Q_PROPERTY(QStringList handleBackgroundBrush READ handleBackgroundBrush WRITE setHandleBackgroundBrush)
        Q_PROPERTY(QStringList splitterPen READ splitterPen WRITE setSplitterPen)

    public:
        explicit LyricWrapView(QString qssPath = "", QStringList priorityG2pIds = {},
                               QMap<std::string, std::string> langToG2pId = {}, QWidget *parent = nullptr);
        ~LyricWrapView() override;

        void clear();
        void init(const QList<QList<LangNote>> &noteLists);

        CellList *createNewList();

        void insertList(const int &index, CellList *cellList);
        void removeList(const int &index);
        void removeList(CellList *cellList);
        void appendList(const QList<LangNote *> &noteList);

        void moveUpLists(const QList<CellList *> &cellLists);
        void moveDownLists(const QList<CellList *> &cellLists);

        CellList *mapToList(const QPointF &pos);
        QPointF mapToCellRect(const QPointF &pos);
        void selectCells(const QPointF &startPos, const QPointF &scenePos);

        void updateCellRect();
        void repaintCellLists();

        QList<CellList *> cellLists() const;
        QStringList priorityG2pIds() const;

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

        QStringList m_splitterPen;
        QStringList splitterPen() const;
        void setSplitterPen(const QStringList &splitterPen);

    private:
        qreal maxListWidth() const;
        qreal height();
        void connectCellList(CellList *cellList);
        qreal cellBaseY(const int &index) const;

        bool cellEqualLine(QList<LyricCell *> cells);

        void tryRubberBandSelect(const QPoint &scenePos);
        QList<CellList *> sortedByIndex(const QSet<CellList *> &listSet) const;
        void insertNewLineAt(qlonglong index);
        void lineBreak(CellList *cellList, const int &index);
        void deleteCells(const QList<LyricCell *>& selectedCells);

        QFont m_font;
        QGraphicsScene *m_scene;

        QList<CellList *> m_cellLists;
        QList<LyricCell *> m_selectedCells{};

        QPoint m_rubberBandOrigin;
        QPoint m_lastClickPos;

        QString m_qssPath;
        QStringList m_priorityG2pIds;
        QMap<std::string, std::string> m_langToG2pId;

    private Q_SLOTS:
        void updateRect();
    };
} // namespace FillLyric
#endif // LYRIC_TAB_CONTROLS_LYRIC_WRAP_VIEW_H
