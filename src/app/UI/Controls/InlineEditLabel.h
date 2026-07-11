//
// Created by FlutyDeer on 2026/7/12.
//

#ifndef INLINEEDITLABEL_H
#define INLINEEDITLABEL_H

#include <QPointer>
#include <QWidget>

class QLabel;
class InlineTextEditOverlay;
class QValidator;

class InlineEditLabel : public QWidget {
    Q_OBJECT

public:
    enum EditRole {
        Default,
        TrackName,
        ClipName,
        Lyric,
        Pronunciation,
        Tempo,
        TimeSignature,
        PlaybackPosition,
        Pan,
        Gain
    };
    Q_ENUM(EditRole)

    explicit InlineEditLabel(QWidget *parent = nullptr);
    ~InlineEditLabel() override;

    QString text() const;
    void setText(const QString &text);

    QFont displayFont() const;
    void setDisplayFont(const QFont &font);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    QValidator *validator() const;
    void setValidator(QValidator *validator);

    EditRole editRole() const;
    void setEditRole(EditRole role);

    /// Set the widget that will parent the editing overlay (typically the viewport).
    /// When not set, the overlay is parented to this label's parent widget.
    void setOverlayParent(QWidget *parent);

    /// Finish editing immediately, submitting current text. No-op if not editing.
    void finishEditing();

signals:
    void editCompleted(const QString &text);
    void editingStarted();

protected:
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void startEditing();
    void onTextSubmitted(const QString &text);
    void onEditCancelled();

    QString m_text;
    QFont m_displayFont;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
    QPointer<QValidator> m_validator;
    EditRole m_editRole = Default;
    QPointer<QWidget> m_overlayParent;
    QPointer<InlineTextEditOverlay> m_overlay;
    QLabel *m_label = nullptr;
};

#endif // INLINEEDITLABEL_H
