//
// Created by fluty on 2024/7/11.
//

#ifndef TRACKLISTWIDGET_H
#define TRACKLISTWIDGET_H

#include <QListWidget>

class TracksGraphicsView;

class TrackListView : public QListWidget {
    Q_OBJECT
public:
    explicit TrackListView(QWidget *parent = nullptr);
    void setGraphicsView(TracksGraphicsView *view);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    bool isInDragArea(const QPoint &pos) const;

    TracksGraphicsView *m_view = nullptr;
    int m_scrollPosBeforeDrag = 0;
    bool m_canStartDrag = false;
};

#endif // TRACKLISTWIDGET_H
