#ifndef EDITORSHORTCUTUTILS_H
#define EDITORSHORTCUTUTILS_H

#include <QAbstractSpinBox>
#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QObject>
#include <QPlainTextEdit>
#include <QShortcut>
#include <QTextEdit>
#include <QWidget>

#include <functional>
#include <utility>

namespace EditorShortcutUtils {

    inline bool isTextInput(const QWidget *widget) {
        return qobject_cast<const QLineEdit *>(widget) || qobject_cast<const QTextEdit *>(widget) ||
               qobject_cast<const QPlainTextEdit *>(widget) ||
               qobject_cast<const QAbstractSpinBox *>(widget);
    }

    inline void guardTextInput(QShortcut *shortcut) {
        shortcut->setEnabled(!isTextInput(QApplication::focusWidget()));
        QObject::connect(qApp, &QApplication::focusChanged, shortcut,
                         [shortcut](QWidget *, QWidget *current) {
                             shortcut->setEnabled(!isTextInput(current));
                         });
    }

    namespace Detail {

        class ApplicationShortcutOverrideFilter final : public QObject {
        public:
            ApplicationShortcutOverrideFilter(
                QShortcut *shortcut, std::function<bool(const QWidget *)> isWindowAllowed)
                : QObject(shortcut), m_shortcut(shortcut),
                  m_isWindowAllowed(std::move(isWindowAllowed)) {
                qApp->installEventFilter(this);
            }

        protected:
            bool eventFilter(QObject *watched, QEvent *event) override {
                Q_UNUSED(watched)
                if (event->type() != QEvent::ShortcutOverride || !m_shortcut->isEnabled()) {
                    return false;
                }

                const auto *keyEvent = static_cast<QKeyEvent *>(event);
                if (!m_shortcut->keys().contains(QKeySequence(keyEvent->keyCombination())))
                    return false;

                if (!m_isWindowAllowed(QApplication::activeWindow()) ||
                    isTextInput(QApplication::focusWidget()) ||
                    QApplication::activePopupWidget() || QApplication::activeModalWidget() ||
                    qobject_cast<QDialog *>(QApplication::activeWindow())) {
                    event->accept();
                    return true;
                }

                event->ignore();
                return true;
            }

        private:
            QShortcut *m_shortcut;
            std::function<bool(const QWidget *)> m_isWindowAllowed;
        };

    } // namespace Detail

    template <typename Receiver, typename Slot>
    QShortcut *add(QWidget *owner, const QKeySequence &key, Receiver *receiver, Slot &&slot) {
        auto *shortcut = new QShortcut(key, owner);
        shortcut->setContext(Qt::WidgetWithChildrenShortcut);
        guardTextInput(shortcut);
        QObject::connect(shortcut, &QShortcut::activated, receiver, std::forward<Slot>(slot));
        return shortcut;
    }

    template <typename WindowPredicate, typename Receiver, typename Slot>
    QShortcut *addApplication(QWidget *owner, const QKeySequence &key,
                              WindowPredicate &&isWindowAllowed, Receiver *receiver, Slot &&slot) {
        auto *shortcut = new QShortcut(key, owner);
        shortcut->setContext(Qt::ApplicationShortcut);
        new Detail::ApplicationShortcutOverrideFilter(
            shortcut, std::forward<WindowPredicate>(isWindowAllowed));
        QObject::connect(shortcut, &QShortcut::activated, receiver, std::forward<Slot>(slot));
        return shortcut;
    }

} // namespace EditorShortcutUtils

#endif // EDITORSHORTCUTUTILS_H
