//
// Created by FlutyDeer on 2026/7/12.
//

#ifndef INLINETEXTEDITOVERLAY_H
#define INLINETEXTEDITOVERLAY_H

#include <QPointer>
#include <QVariantMap>
#include <QWidget>

class QLineEdit;
class Menu;

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

signals:
    void textSubmitted(const QString &text);
    void navigationRequested(const QString &text, bool backwards);
    void editCancelled();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void cancel();
    void navigate(bool backwards);

    QLineEdit *m_lineEdit = nullptr;
    QPointer<Menu> m_activeMenu;
    bool m_submitted = false;
    bool m_navigationEnabled = false;
};

#endif // INLINETEXTEDITOVERLAY_H
