#ifndef DSEDITORLITE_TOUCHCLAIMFILTER_H
#define DSEDITORLITE_TOUCHCLAIMFILTER_H

#include <QObject>
#include <QPointer>

class QWidget;

/// Lets a drag-owning widget keep its gesture inside a touch-kinetic scroll
/// area: the filter claims the widget's whole rect (the widget itself starts
/// accepting touch events) and replays each touch point as a left-button mouse
/// event, so the widget's existing mouse drag logic runs unchanged. Accepting
/// the touch keeps the stream away from the ancestor viewport's QScroller and
/// stops Qt from synthesizing additional mouse events for the widget.
class TouchClaimFilter : public QObject {
    Q_OBJECT

public:
    /// Installs the filter on \p target and enables touch delivery on it.
    /// Idempotent; the filter lives as long as the target.
    static void install(QWidget *target);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit TouchClaimFilter(QWidget *target);

    void stopAncestorScroller() const;

    QPointer<QWidget> m_target;
    bool m_pressed = false;
};

#endif // DSEDITORLITE_TOUCHCLAIMFILTER_H
