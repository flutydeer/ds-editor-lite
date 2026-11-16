#ifndef POPUPINLINEEDITLABEL_H
#define POPUPINLINEEDITLABEL_H

#include <lite/GUI/Controls/InlineEditLabel.h>

class QFrame;
class QMouseEvent;
class QTimer;

// Inline editor with a Qt::Popup companion: a single left click opens the
// popup, a double click edits the label in place, and the right button toggles
// the popup. The click arbitration and the popup's open/close bookkeeping are
// identical for the tempo, time signature and transport time fields, so they
// live here once instead of being copied per field.
class PopupInlineEditLabel : public InlineEditLabel {
    Q_OBJECT

public:
    explicit PopupInlineEditLabel(QWidget *parent = nullptr);
    ~PopupInlineEditLabel() override = default;

signals:
    // Emitted right before the popup is shown, so the owner can snapshot the
    // state the popup is about to edit (the playhead keeps moving).
    void popupAboutToShow();

protected:
    // Subclasses hand their popup over once, in their constructor.
    void setPopupWidget(QFrame *popup);
    [[nodiscard]] QFrame *popupWidget() const;
    // Refreshes the popup content from the owner's current state.
    virtual void preparePopup();
    // Positions and shows the popup; subclasses forward to their popup's
    // showAt() so each popup keeps its own screen clamping.
    virtual void presentPopup(const QPoint &globalPos) = 0;

    void showPopup();
    void hidePopup();
    [[nodiscard]] bool isPopupVisible() const;
    void setPopupVisible(bool visible);

    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QFrame *m_popup = nullptr;
    QTimer *m_clickTimer = nullptr;
    bool m_ignoreNextShow = false;
    bool m_hidingPopupFromCombo = false;
};

#endif // POPUPINLINEEDITLABEL_H
