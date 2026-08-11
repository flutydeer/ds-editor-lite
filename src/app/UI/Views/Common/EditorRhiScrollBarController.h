#ifndef EDITORRHISCROLLBARCONTROLLER_H
#define EDITORRHISCROLLBARCONTROLLER_H

#include <QObject>
#include <QPointF>
#include <QSizeF>

class OverlayScrollBar;
class QWidget;

class EditorRhiScrollBarController final : public QObject {
    Q_OBJECT

public:
    explicit EditorRhiScrollBarController(QWidget *viewport, QObject *parent = nullptr);

    void setMetrics(const QSizeF &contentSize, const QPointF &offset,
                    const QSizeF &singleStep = {});
    [[nodiscard]] OverlayScrollBar *horizontalBar() const;
    [[nodiscard]] OverlayScrollBar *verticalBar() const;

signals:
    void offsetChangeRequested(const QPointF &offset);

private:
    QWidget *m_viewport = nullptr;
    OverlayScrollBar *m_horizontalBar = nullptr;
    OverlayScrollBar *m_verticalBar = nullptr;
    bool m_updating = false;
};

#endif // EDITORRHISCROLLBARCONTROLLER_H
