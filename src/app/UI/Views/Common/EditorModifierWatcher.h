#ifndef EDITORMODIFIERWATCHER_H
#define EDITORMODIFIERWATCHER_H

#include <QObject>
#include <Qt>

#include <functional>

// Reports momentary modifier-key changes while a pointer gesture is in flight,
// including the presses and releases that arrive with no pointer movement at all.
// Qt delivers key events to the focus widget only, so a canvas whose gesture owns
// the pointer but not the focus would otherwise miss them; watching the whole
// application keeps the gesture preview correct regardless of who holds focus.
// The watcher only observes: the events it inspects reach shortcuts and text
// input untouched.
class EditorModifierWatcher final : public QObject {
public:
    explicit EditorModifierWatcher(QObject *owner,
                                   std::function<void(Qt::KeyboardModifiers)> onChanged);

    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::function<void(Qt::KeyboardModifiers)> m_onChanged;
    bool m_enabled = false;
};

#endif // EDITORMODIFIERWATCHER_H
