#ifndef PATHLISTWIDGET_H
#define PATHLISTWIDGET_H

#include <QListWidget>

class QPoint;
class QMouseEvent;
class QDragEnterEvent;
class QDropEvent;

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
};

#endif // PATHLISTWIDGET_H