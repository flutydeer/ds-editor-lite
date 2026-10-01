#ifndef INLINETEXTEDITOVERLAY_H
#define INLINETEXTEDITOVERLAY_H

#include <QEvent>
#include <QPointer>
#include <QVariantMap>
#include <QWidget>

class LineEdit;
class Menu;
class QTimer;

class InlineTextEditOverlay : public QWidget {
    Q_OBJECT

public:
    explicit InlineTextEditOverlay(QWidget *parent = nullptr);
    ~InlineTextEditOverlay() override = default;

    void showAt(const QRect &anchorRect, const QString &text, const QFont &font,
                const QVariantMap &editorProperties = {});
    /// Force-submit the current text. No-op if not editing or already submitted.
    void submit();
    void dismiss(bool cancel = false);
    [[nodiscard]] bool isEditing() const;

    // Touch streams that begin on the editor are owned by the canvas touch
    // controller (its delivery is the only one that reaches every event of a
    // stream reliably), which forwards them here. Begin answers whether the
    // point is on this editor and took the stream; the rest replay it to the
    // line edit as mouse events. The long press opens the edit menu on
    // release, the same ownership the canvas gives its own menus.
    [[nodiscard]] bool relayTouchBegin(const QPointF &globalPos);
    void relayTouchMove(const QPointF &globalPos);
    void relayTouchEnd(const QPointF &globalPos);
    void relayTouchCancel();

signals:
    void textSubmitted(const QString &text);
    void navigationRequested(const QString &text, bool backwards);
    void editCancelled();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void cancel();
    void navigate(bool backwards);
    void showContextMenu(const QPoint &globalPos);

    void sendTouchMouse(QEvent::Type type, const QPointF &globalPos, Qt::MouseButton button,
                        Qt::MouseButtons buttons);
    void postTouchContextMenu();

    LineEdit *m_lineEdit = nullptr;
    QPointer<Menu> m_activeMenu;
    bool m_submitted = false;
    bool m_navigationEnabled = false;

    QPointer<QWidget> m_mouseTarget;
    bool m_touchClaimed = false;
    bool m_touchMenuPending = false;
    QTimer *m_touchLongPressTimer = nullptr;
    QPointF m_touchPressGlobal;
    QPointF m_touchLastGlobal;
};

#endif // INLINETEXTEDITOVERLAY_H
