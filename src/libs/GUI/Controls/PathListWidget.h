#ifndef PATHLISTWIDGET_H
#define PATHLISTWIDGET_H

#include <QListWidget>

class QPoint;
class QMouseEvent;
class QDragEnterEvent;
class QDropEvent;
class ItemViewReorderController;

class PathListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit PathListWidget(QWidget *parent = nullptr);

Q_SIGNALS:
    void doubleClickedEmpty(const QPoint &pos);
    void itemsDropped(const QStringList &items);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    /// True when \p viewportPos sits in a row's reorder grip; reports the row.
    [[nodiscard]] bool handleHitTest(const QPoint &viewportPos, int *row) const;

    ItemViewReorderController *m_reorder = nullptr;
};

#endif // PATHLISTWIDGET_H