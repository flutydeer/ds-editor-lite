#ifndef TRACKLISTWIDGET_H
#define TRACKLISTWIDGET_H

#include <QListWidget>

class QWheelEvent;

class TrackListView : public QListWidget {
    Q_OBJECT
public:
    explicit TrackListView(QWidget *parent = nullptr);

signals:
    void wheelVerScale(QWheelEvent *event);
    void wheelVerScroll(QWheelEvent *event);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    bool isInDragArea(const QPoint &pos) const;

    int m_scrollPosBeforeDrag = 0;
    bool m_canStartDrag = false;
};

#endif // TRACKLISTWIDGET_H
