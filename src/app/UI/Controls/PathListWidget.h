#ifndef PATHLISTWIDGET_H
#define PATHLISTWIDGET_H

#include <QListWidget>

class QPoint;
class QMouseEvent;

class PathListWidget : public QListWidget {
    Q_OBJECT
public:
    explicit PathListWidget(QWidget *parent = nullptr);

Q_SIGNALS:
    void doubleClickedEmpty(const QPoint &pos);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

#endif // PATHLISTWIDGET_H