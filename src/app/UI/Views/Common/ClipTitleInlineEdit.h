#ifndef CLIPTITLEINLINEEDIT_H
#define CLIPTITLEINLINEEDIT_H

#include <QFont>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QRect>
#include <QRectF>
#include <QString>

class InlineTextEditOverlay;
class QWidget;

// Inline rename session for an arrangement clip. The view supplies the title
// rectangle in viewport coordinates and ends the session when the target moves.
// On submission, look up the clip by ID and discard the edit if it is gone.
class ClipTitleInlineEdit : public QObject {
    Q_OBJECT

public:
    // The editor hugs the name it shows instead of covering the whole title
    // band: a one-word name stays short, a long one grows up to the ceiling and
    // never wider than the band it sits on (unless the band is too narrow to
    // type in at all).
    static constexpr int minimumEditorWidth = 96;
    static constexpr int maximumEditorWidth = 320;

    explicit ClipTitleInlineEdit(QWidget *host, QObject *parent = nullptr);

    // Start (or retarget) the session. Ignored when already editing this clip.
    // Retargeting commits the previous clip first.
    void begin(int clipId, const QRectF &titleRect, const QString &name, const QFont &font);
    // Commit the current text. No-op when not editing.
    void submit();
    // Drop the current text without touching the model.
    void cancel();

    [[nodiscard]] bool isEditing() const;
    [[nodiscard]] int editingClipId() const;

    // Width of the editor for `text`. Pure, so the sizing rule can be checked
    // without opening a session.
    [[nodiscard]] static int editorWidthFor(const QString &text, const QFont &font, int bandWidth);

    // Touch streams that begin on the editor belong to it, not to the canvas:
    // the canvas touch controller forwards them here, in viewport coordinates.
    [[nodiscard]] bool containsViewportPoint(const QPointF &viewportPosition) const;
    [[nodiscard]] bool relayTouchBegin(const QPointF &viewportPosition);
    void relayTouchMove(const QPointF &viewportPosition);
    void relayTouchEnd(const QPointF &viewportPosition);
    void relayTouchCancel();

signals:
    // Emitted after a submit or a cancel, before any model write, so the
    // backend can drop its "editing" visual for `clipId`.
    void sessionFinished(int clipId);

private:
    void ensureOverlay();
    void endSession();
    void onTextSubmitted(const QString &text);
    void applyEditorWidth(const QString &text);
    [[nodiscard]] int editorWidthFor(const QString &text) const;
    [[nodiscard]] QRect editorRectFor(const QString &text) const;

    QWidget *m_host = nullptr;
    QPointer<InlineTextEditOverlay> m_overlay;
    QMetaObject::Connection m_textChangedConnection;
    // The title band the session was opened on, in host coordinates, and the
    // font it shows: both drive the width the editor keeps while typing.
    QRect m_anchorBand;
    QFont m_font;
    int m_clipId = -1;
};

#endif // CLIPTITLEINLINEEDIT_H
