#ifndef TRACKLISTWIDGET_H
#define TRACKLISTWIDGET_H

#include <QListWidget>

class QWheelEvent;
class QListWidgetItem;
class QDragLeaveEvent;

class TrackListView : public QListWidget {
    Q_OBJECT
public:
    explicit TrackListView(QWidget *parent = nullptr);

    // Number of real track rows; the permanent append-slot placeholder row at
    // the bottom of the list is excluded.
    [[nodiscard]] int trackCount() const;
    // Moves the real track row `from` to final index `to` without ever moving
    // the append-slot placeholder row.
    bool moveTrackRow(int from, int to);
    // Keeps the placeholder row height in sync with the canvas append slot
    // (one track height at the current vertical scale).
    void setAppendSlotHeight(int height);

signals:
    void wheelVerScale(QWheelEvent *event);
    void wheelVerScroll(QWheelEvent *event);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    bool isInDragArea(const QPoint &pos) const;
    int dropInsertionIndex(const QPoint &pos) const;
    bool isValidDropInsertionIndex(int insertionIndex) const;
    bool setDropInsertionIndex(int insertionIndex);
    bool moveDraggedTrack(int insertionIndex);
    void updateDropIndicator(int insertionIndex);
    void clearDropIndicator();

    int m_scrollPosBeforeDrag = 0;
    QPoint m_dragStartPosition;
    int m_dragRow = -1;
    int m_dropInsertionIndex = -1;
    bool m_canStartDrag = false;
    QWidget *m_dropIndicator = nullptr;
    // Disabled, non-selectable, non-draggable placeholder row mirroring the
    // canvas append slot. Always the last row.
    QListWidgetItem *m_appendSlotItem = nullptr;
};

#endif // TRACKLISTWIDGET_H
